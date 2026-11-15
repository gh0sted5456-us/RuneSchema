#include "Core/JsonDocument.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <cstdint>
#include <thread>
#include <fstream>
#include <format>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
namespace PS::JsonHelpers {
    namespace {
        std::atomic<DiagnosticReporter> diagnosticReporter{nullptr};
        thread_local const fs::path* activeSourceFile=nullptr;
        struct SourceScope {
            const fs::path* previous;
            explicit SourceScope(const fs::path& source):previous(activeSourceFile){activeSourceFile=&source;}
            ~SourceScope(){activeSourceFile=previous;}
        };
        void Publish(const char* code,const fs::path& path,const std::string& field,const std::string& detail) noexcept {
            if(const auto reporter=diagnosticReporter.load(std::memory_order_acquire))try{reporter(code,path,field,detail);}catch(...){}
        }
        void PublishField(const char* code,const std::string& field,const std::string& detail) noexcept {if(activeSourceFile)Publish(code,*activeSourceFile,field,detail);}
        // A discovered definition and its path relative to the canonical loader root, computed once.
        struct JsonFile{fs::path Path;fs::path Relative;std::string Key;};
        std::vector<JsonFile> DiscoverJsonFiles(const fs::path& root)
        {
            if (!fs::is_directory(root)) return {};
            const auto canonicalRoot = fs::weakly_canonical(root);
            std::vector<JsonFile> files;
            std::error_code error;
            fs::recursive_directory_iterator iterator(root,
                fs::directory_options::skip_permission_denied, error), end;
            for (; iterator != end; iterator.increment(error))
            {
                if (error) { error.clear(); continue; }
                const auto& entry = *iterator;
                if (entry.is_symlink(error))
                {
                    if (entry.is_directory(error)) iterator.disable_recursion_pending();
                    continue;
                }
                if (!entry.is_regular_file(error)) continue;
                const auto extension = entry.path().extension();
                if (extension != ".json" && extension != ".jsonc") continue;
                const auto resolved = fs::weakly_canonical(entry.path(), error);
                if (error) { error.clear(); continue; }
                auto relative = resolved.lexically_relative(canonicalRoot);
                if (relative.empty() || relative.native().starts_with(fs::path("..").native())) continue;
                auto key=relative.generic_string();
                files.push_back({resolved,std::move(relative),std::move(key)});
                if (files.size() > 4096) {
                    Publish("RS-JSON-005",root,{},"Loader directory exceeds the 4096 JSON-file safety limit");
                    throw std::runtime_error("Loader directory exceeds the 4096 JSON-file safety limit");
                }
            }
            // Sort by the precomputed relative key: comparisons no longer rebuild relative paths.
            std::ranges::sort(files, [](const auto& left, const auto& right) {return left.Key < right.Key;});
            files.erase(std::unique(files.begin(), files.end(), [](const auto& left, const auto& right) {return left.Path == right.Path;}), files.end());
            return files;
        }
        // A definition read and parsed away from the caller. Diagnostics are published later, on the caller's thread.
        struct LoadedJson{nlohmann::json Document;bool Missing=false;const char* Code=nullptr;std::string Detail,Message;std::exception_ptr ParseFailure;};
        LoadedJson LoadJson(const fs::path& path) noexcept
        {
            LoadedJson result;
            try{
                std::error_code error;
                if(!fs::exists(path,error)){result.Missing=true;return result;}
                const auto size=fs::file_size(path);
                if(size>2*1024*1024){result.Code="RS-JSON-004";result.Detail=result.Message="JSON definition exceeds the 2 MiB safety limit";return result;}
                std::ifstream input(path,std::ios::binary);
                if(!input){result.Code="RS-JSON-001";result.Detail=result.Message="JSON definition could not be opened";return result;}
                std::string text(static_cast<size_t>(size)+1,'\0');
                input.read(text.data(),static_cast<std::streamsize>(text.size()));
                if(input.bad()||static_cast<uintmax_t>(input.gcount())!=size)throw std::runtime_error("JSON definition changed or could not be read");
                text.resize(static_cast<size_t>(size));
                try{result.Document=nlohmann::json::parse(text,nullptr,true,true);}
                catch(const nlohmann::json::parse_error& parseError){
                    result.Code="RS-JSON-001";result.Detail=parseError.what();result.ParseFailure=std::current_exception();
                }
            }catch(const std::exception& failure){result.Code="RS-JSON-001";result.Detail=result.Message=failure.what();result.Document=nullptr;}
            catch(...){result.Code="RS-JSON-001";result.Detail=result.Message="unknown JSON or definition error";result.Document=nullptr;}
            return result;
        }
        // Reads and parses ahead on a few worker threads while the caller consumes results strictly in order.
        // The window bounds how many parsed documents are held at once.
        class JsonPrefetch {
        public:
            // Machines with fewer than four hardware threads stay serial: one worker cannot overlap parsing with
            // the caller's work, and loaders compete with the engine's own loading threads.
            static size_t Threads(size_t files){
                const auto hardware=std::thread::hardware_concurrency();
                return files<4||hardware<4?0:std::min<size_t>(3,hardware/2);
            }
            explicit JsonPrefetch(const std::vector<JsonFile>& files,size_t threads=SIZE_MAX):Files(files),Slots(files.size()){
                if(threads==SIZE_MAX)threads=Threads(files.size());
                Window=std::max<size_t>(threads*2,1);
                try{for(size_t i=0;i<threads;++i)Workers.emplace_back([this]{Work();});}catch(...){}
            }
            ~JsonPrefetch(){{std::lock_guard lock(Mutex);Stopping=true;}Room.notify_all();for(auto& worker:Workers)if(worker.joinable())worker.join();}
            LoadedJson Take(size_t index){
                if(Workers.empty())return LoadJson(Files[index].Path);
                std::unique_lock lock(Mutex);Ready.wait(lock,[&]{return Slots[index].has_value();});
                auto value=std::move(*Slots[index]);Slots[index].reset();++Delivered;lock.unlock();Room.notify_all();return value;
            }
            // Hands a consumed document back so a worker frees it; large trees are not torn down on the caller's thread.
            void Retire(LoadedJson&& loaded){
                if(Workers.empty())return;
                {std::lock_guard lock(Mutex);Retired.push_back(std::move(loaded.Document));}Room.notify_one();
            }
        private:
            void Work(){
                for(;;){
                    size_t index=0;std::vector<nlohmann::json> retired;bool parse=false;
                    {std::unique_lock lock(Mutex);Room.wait(lock,[&]{return Stopping||!Retired.empty()||(Next<Files.size()&&Next<Delivered+Window);});
                        retired.swap(Retired);
                        if(!Stopping&&Next<Files.size()&&Next<Delivered+Window){index=Next++;parse=true;}
                        else if(retired.empty()&&(Stopping||Next>=Files.size()))return;}
                    retired.clear();
                    if(!parse)continue;
                    auto loaded=LoadJson(Files[index].Path);
                    {std::lock_guard lock(Mutex);Slots[index]=std::move(loaded);}Ready.notify_all();
                }
            }
            const std::vector<JsonFile>& Files;std::vector<std::optional<LoadedJson>> Slots;std::vector<std::thread> Workers;
            std::vector<nlohmann::json> Retired;
            std::mutex Mutex;std::condition_variable Ready,Room;size_t Next=0,Delivered=0,Window=1;bool Stopping=false;
        };
        // Publishes a load failure exactly as the serial parser did, then hands the document to the callback.
        void DeliverJson(const fs::path& path,LoadedJson& loaded,const std::function<void(const nlohmann::json&)>& callback)
        {
            if(loaded.Missing)return;
            SourceScope scope(path);
            if(loaded.Code){
                Publish(loaded.Code,path,{},loaded.Detail);
                if(loaded.ParseFailure)std::rethrow_exception(loaded.ParseFailure);
                throw std::runtime_error(loaded.Message);
            }
            callback(loaded.Document);
        }
    }
    void SetDiagnosticReporter(DiagnosticReporter reporter) noexcept {diagnosticReporter.store(reporter,std::memory_order_release);}
    bool FieldExists(const nlohmann::json& data, const std::string& fieldName)
    {
        return data.contains(fieldName);
    }

    void ValidateFieldExists(const nlohmann::json& data, const std::string& fieldName)
    {
        if (!data.contains(fieldName))
        {
            PublishField("RS-JSON-002",fieldName,"Required configuration field is missing");
            throw std::runtime_error(std::format("Missing a required field of '{}'.", fieldName));
        }
    }

    void ParseDouble(const nlohmann::json& value, const std::string& fieldName, double& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_number())
        {
            PublishField("RS-JSON-003",fieldName,"Configuration field has an incompatible type; expected number");
            throw std::runtime_error(std::format("Value '{}' must be a number.", fieldName));
        }

        outValue = field.get<double>();
    }

    void ParseInteger(const nlohmann::json& value, const std::string& fieldName, int& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_number_integer())
        {
            PublishField("RS-JSON-003",fieldName,"Configuration field has an incompatible type; expected integer");
            throw std::runtime_error(std::format("Value '{}' must be an integer.", fieldName));
        }

        outValue = field.get<int>();
    }

    void ParseString(const nlohmann::json& value, const std::string& fieldName, std::string& outValue)
    {
        auto& field = value.at(fieldName);

        if (!field.is_string())
        {
            PublishField("RS-JSON-003",fieldName,"Configuration field has an incompatible type; expected string");
            throw std::runtime_error(std::format("Value '{}' must be a string.", fieldName));
        }

        outValue = field.get<std::string>();
    }

    void ParseJsonFileInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback)
    {
        if (path.extension() != ".json" && path.extension() != ".jsonc")
        {
            return;
        }
        auto loaded=LoadJson(path);DeliverJson(path,loaded,callback);
    }
    void ParseJsonFilesInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback)
    {
        ParseJsonFilesInPathWithSource(path,
            [&](const nlohmann::json& document, const fs::path&) { callback(document); });
    }

    void ParseJsonFilesInPathWithSource(const std::filesystem::path& path,
        const std::function<void(const nlohmann::json&, const std::filesystem::path&)>& callback)
    {
        const auto files=DiscoverJsonFiles(path);JsonPrefetch prefetch(files);
        for (size_t index=0;index<files.size();++index)
        {
            const auto& file=files[index];
            try
            {
                auto loaded=prefetch.Take(index);
                DeliverJson(file.Path,loaded,[&](const nlohmann::json& document) {callback(document, file.Relative);});
                prefetch.Retire(std::move(loaded));
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(std::format("Failed parsing mod file {} - {}.\n", file.Path.string(), e.what()));
            }
        }
    }
    void ParseJsonFilesInPathIsolated(const std::filesystem::path& path,
        const std::function<void(const nlohmann::json&)>& callback,
        const std::function<void(const std::filesystem::path&, const std::string&)>& onError)
    {
        ParseJsonFilesInPathWithSourceIsolated(path,
            [&](const nlohmann::json& document, const fs::path&) { callback(document); }, onError);
    }

    void ParseJsonFilesInPathWithSourceIsolated(const std::filesystem::path& path,
        const std::function<void(const nlohmann::json&, const std::filesystem::path&)>& callback,
        const std::function<void(const std::filesystem::path&, const std::string&)>& onError)
    {
        const auto files=DiscoverJsonFiles(path);JsonPrefetch prefetch(files);
        for (size_t index=0;index<files.size();++index)
        {
            const auto& file=files[index];
            try { auto loaded=prefetch.Take(index);DeliverJson(file.Path,loaded,[&](const nlohmann::json& document) {
                callback(document, file.Relative);
            });prefetch.Retire(std::move(loaded)); }
            catch (const std::exception& error)
            {
                if (onError) onError(file.Path, error.what());
            }
            catch (...)
            {
                if (onError) onError(file.Path, "unknown JSON or definition error");
            }
        }
    }

}
