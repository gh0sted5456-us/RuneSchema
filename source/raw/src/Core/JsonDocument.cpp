#include "Core/JsonDocument.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
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
        std::vector<fs::path> DiscoverJsonFiles(const fs::path& root)
        {
            if (!fs::is_directory(root)) return {};
            const auto canonicalRoot = fs::weakly_canonical(root);
            std::vector<fs::path> files;
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
                const auto relative = resolved.lexically_relative(canonicalRoot);
                if (relative.empty() || relative.native().starts_with(fs::path("..").native())) continue;
                files.push_back(resolved);
                if (files.size() > 4096) {
                    Publish("RS-JSON-005",root,{},"Loader directory exceeds the 4096 JSON-file safety limit");
                    throw std::runtime_error("Loader directory exceeds the 4096 JSON-file safety limit");
                }
            }
            std::ranges::sort(files, [&](const auto& left, const auto& right) {
                return left.lexically_relative(canonicalRoot).generic_string()
                    < right.lexically_relative(canonicalRoot).generic_string();
            });
            files.erase(std::unique(files.begin(), files.end()), files.end());
            return files;
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
        if (!fs::exists(path))
        {
            return;
        }

        if (path.extension() != ".json" && path.extension() != ".jsonc")
        {
            return;
        }

        SourceScope scope(path);
        if (fs::file_size(path) > 2 * 1024 * 1024) {
            Publish("RS-JSON-004",path,{},"JSON definition exceeds the 2 MiB safety limit");
            throw std::runtime_error("JSON definition exceeds the 2 MiB safety limit");
        }
        std::ifstream f(path);
        if(!f) {
            Publish("RS-JSON-001",path,{},"JSON definition could not be opened");
            throw std::runtime_error("JSON definition could not be opened");
        }
        nlohmann::json data;
        try { data=nlohmann::json::parse(f,nullptr,true,true); }
        catch(const nlohmann::json::parse_error& error) {
            Publish("RS-JSON-001",path,{},error.what());
            throw;
        }
        callback(data);
    }

    void ParseJsonFilesInPath(const std::filesystem::path& path, const std::function<void(const nlohmann::json&)>& callback)
    {
        ParseJsonFilesInPathWithSource(path,
            [&](const nlohmann::json& document, const fs::path&) { callback(document); });
    }

    void ParseJsonFilesInPathWithSource(const std::filesystem::path& path,
        const std::function<void(const nlohmann::json&, const std::filesystem::path&)>& callback)
    {
        for (const auto& filePath : DiscoverJsonFiles(path))
        {
            try
            {
                ParseJsonFileInPath(filePath,
                    [&](const nlohmann::json& document) {
                        callback(document, filePath.lexically_relative(fs::weakly_canonical(path)));
                    });
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(std::format("Failed parsing mod file {} - {}.\n", filePath.string(), e.what()));
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
        for (const auto& filePath : DiscoverJsonFiles(path))
        {
            try { ParseJsonFileInPath(filePath, [&](const nlohmann::json& document) {
                callback(document, filePath.lexically_relative(fs::weakly_canonical(path)));
            }); }
            catch (const std::exception& error)
            {
                if (onError) onError(filePath, error.what());
            }
            catch (...)
            {
                if (onError) onError(filePath, "unknown JSON or definition error");
            }
        }
    }

}
