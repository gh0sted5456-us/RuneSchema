#include "Runtime/SearchJobs.h"
#include "Runtime/SearchJob.h"
#include "Runtime/HostServices.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Core/ConfigFiles.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Helpers/String.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <mutex>
#include <vector>
#include <future>
#include <optional>

namespace PS::RuntimeJobs {
    using namespace RC;
    using namespace RC::Unreal;
    using nlohmann::json;
    namespace fs = std::filesystem;
    namespace {
        using Clock = std::chrono::steady_clock;
        std::string Lower(std::string value);
        struct Pending { json job; std::string file; Clock::time_point due; };
        struct Search {
            Pending Work;json Report;SearchCursor Cursor;std::string Needle,ClassNeedle;size_t Limit;bool Truncated;
            Search(Pending work,size_t count):Work(std::move(work)),
                Report({{"Job",Work.job},{"Source",Work.file},{"Matches",json::array()},{"ReadOnly",true}}),Cursor(count),
                Needle(Lower(Work.job.at("Query").get<std::string>())),ClassNeedle(Lower(Work.job.value("ClassContains",std::string{}))),
                Limit(Work.job.value("MaxResults",100u)),Truncated(count>500000){}
        };
        std::optional<Search> active;
        std::future<void> writer;
        std::vector<Pending> pending;
        std::mutex mutex;
        bool travelling = false;
        uint64_t sequence = 0;
        std::vector<Hook::GlobalCallbackId> hooks;
        std::string Lower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }
        void Error(const std::string& message) {
            PS::Log<LogLevel::Warning>(STR("Runtime search job: {}\n"), to_generic_string(message));
        }
        void Queue(const std::string& trigger) {
            try {
                std::vector<fs::path> files;
                const auto folder = HostServices::JobsDirectory();
                fs::create_directories(folder);
                size_t visited = 0;
                for (const auto& entry : fs::directory_iterator(folder)) {
                    if (++visited > 256) throw std::runtime_error("Jobs directory exceeds 256 entries");
                    const auto extension = Lower(entry.path().extension().string());
                    if (!entry.is_symlink() && entry.is_regular_file() && (extension == ".json" || extension == ".jsonc")) files.push_back(entry.path());
                }
                if (files.size() > 32) throw std::runtime_error("At most 32 job files are supported");
                std::sort(files.begin(), files.end());
                for (const auto& file : files) try {
                    auto job = json::parse(ConfigFiles::Read(file, 16384), [](int depth, json::parse_event_t, json&) {
                        if (depth > 8) throw std::runtime_error("Job nesting exceeds 8"); return true;
                    }, true, true);
                    Validate(job);
                    if (!job.value("Enabled", false) || job.at("Trigger") != trigger) continue;
                    std::lock_guard lock(mutex);
                    if (pending.size() >= 64) throw std::runtime_error("Pending job limit reached");
                    pending.push_back({job, file.filename().string(), Clock::now() + std::chrono::seconds(job.value("DelaySeconds", 0))});
                } catch (const std::exception& error) { Error(file.filename().string() + ": " + error.what()); }
            } catch (const std::exception& error) { Error(error.what()); }
        }
        void Tick() {
            std::lock_guard lock(mutex);
            // One owned report at a time bounds memory even when storage is slow.
            if(writer.valid()){
                if(writer.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
                try{writer.get();}catch(const std::exception& error){Error(error.what());}
            }
            if(travelling)return;
            if(!active){
                auto it = std::find_if(pending.begin(), pending.end(), [](const auto& item) { return item.due <= Clock::now(); });
                if (it == pending.end()) return;
                const auto count=FUObjectArray::GetNumElements();if(count<0||count>4000000){Error("Object array exceeds its bound");pending.erase(it);return;}
                active.emplace(std::move(*it),static_cast<size_t>(count));pending.erase(it);
            }
            auto& search=*active;auto& report=search.Report;bool done=false;
            try {
                const auto count=FUObjectArray::GetNumElements();if(count<0||count>4000000)throw std::runtime_error("Object array exceeds its bound");
                const auto deadline=Clock::now()+std::chrono::microseconds(500);
                for(size_t scanned=0;scanned<512&&Clock::now()<deadline;++scanned){
                    const auto index=search.Cursor.Next(static_cast<size_t>(count));if(!index){done=true;break;}
                    auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(*index));auto* object=slot?slot->GetUObject():nullptr;
                    if (!object || !object->GetClassPrivate() || object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed | RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))) continue;
                    const auto path = to_string(object->GetPathName());
                    if (Lower(path).find(search.Needle) == std::string::npos) continue;
                    const auto type = to_string(object->GetClassPrivate()->GetPathName());
                    if (Lower(type).find(search.ClassNeedle) == std::string::npos) continue;
                    report["Matches"].push_back({{"Path", path}, {"Class", type}});
                    if(report["Matches"].size()>=search.Limit){done=true;break;}
                }
                if(!done)return;
                report["Visited"] = search.Cursor.Position;
                report["LimitReached"] = (search.Truncated&&search.Cursor.Position>=500000) || report["Matches"].size() >= search.Limit;
                report["Scope"] = "Loaded object paths and classes only; case-insensitive substring filters. No asset loading, function execution or property traversal.";
            } catch (const std::exception& error) { report["Error"] = error.what(); }
            try {
                const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                const auto name = "Job-" + search.Work.job.at("Name").get<std::string>() + "-" + std::to_string(stamp) + "-" + std::to_string(++sequence) + ".json";
                const auto file=HostServices::ExportsDirectory()/name;
                writer=std::async(std::launch::async,[file,name,report=std::move(report)]{
                    ConfigFiles::Write(file,report.dump(2));
                    PS::Log<LogLevel::Normal>(STR("Runtime search exported: {}\n"),to_generic_string(name));
                });
            } catch (const std::exception& error) { Error(error.what()); }
            active.reset();
        }
    }
    void Shutdown() {
        for (auto id : hooks) if (id != Hook::ERROR_ID) Hook::UnregisterCallback(id);
        hooks.clear();
        std::lock_guard lock(mutex); pending.clear();active.reset();travelling = false;
        if(writer.valid())try{writer.get();}catch(const std::exception& error){Error(error.what());}
    }
    void Initialize() {
        if (!hooks.empty() || !PSConfig::Get()->GetSettings().advancedRuntime) return;
        Hook::FCallbackOptions options{}; options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("RuntimeSearchWorldReset");
        hooks.push_back(Hook::RegisterInitGameStatePreCallback([](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
            std::lock_guard lock(mutex); travelling = true;
            if(active){Error("Search cancelled on world transition; no mixed-world report exported");active.reset();}
            std::erase_if(pending, [](const auto& item) { return item.job.at("Trigger") != "game-load"; });
        }, options));
        options.HookName = TEXT("RuntimeSearchWorldReady");
        hooks.push_back(Hook::RegisterInitGameStatePostCallback([](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
            { std::lock_guard lock(mutex); travelling = false; }
            Queue("world-load");
            Queue("game-state-ready");
        }, options));
        options.HookName = TEXT("RuntimeSearchJobs");
        hooks.push_back(Hook::RegisterEngineTickPostCallback([](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) { Tick(); }, options));
        if (std::find(hooks.begin(), hooks.end(), Hook::ERROR_ID) != hooks.end()) { Shutdown(); Error("Lifecycle hook unavailable; jobs disabled for this session"); return; }
        Queue("game-load");
    }
}
