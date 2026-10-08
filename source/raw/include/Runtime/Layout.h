#pragma once
#include <filesystem>
#include <initializer_list>
#include <utility>
#include "Core/ConfigFiles.h"

namespace PS::RuntimeLayout {
    namespace fs = std::filesystem;
    // Copy-only migration: existing destinations win; originals remain recoverable.
    inline void CopyMissing(const fs::path& source, const fs::path& destination) {
        if (!fs::exists(source) || fs::is_symlink(fs::symlink_status(source))) return;
        if (fs::is_regular_file(source)) {
            fs::create_directories(destination.parent_path());
            fs::copy_file(source, destination, fs::copy_options::skip_existing);
        } else if (fs::is_directory(source)) {
            for (const auto& entry : fs::directory_iterator(source))
                CopyMissing(entry.path(), destination / entry.path().filename());
        }
    }
    inline void ArchiveKnownFile(const fs::path& source,const fs::path& archive) {
        if(!fs::is_regular_file(source)||fs::is_symlink(fs::symlink_status(source)))return;
        CopyMissing(source,archive);
        if(fs::is_regular_file(archive))fs::remove(source);
    }
    inline void Migrate(const fs::path& root) {
        const auto legacyRuntime=root/"runtime";
        const auto runtime=root/"runtime"/"live",jobs=runtime/"jobs",saved=runtime/"saved";
        const auto exports = jobs / "exports", searches=jobs/"searches";
        const auto references=saved/"references",cache=saved/"cache",progress=saved/"progress";
        const auto marker = references / "layout-migration-v2.json";
        if (fs::exists(marker)) return;
        const auto diagnostics = root / "config" / "diagnostics";
        CopyMissing(root / "settings" / "settings.json", root / "settings" / "settings.jsonc");
        CopyMissing(root / "config" / "config.json", root / "settings" / "settings.jsonc");
        CopyMissing(root / "config.json", root / "settings" / "settings.jsonc");
        CopyMissing(root/"settings"/"runtime",runtime);
        CopyMissing(legacyRuntime/"imports",jobs/"imports");
        CopyMissing(legacyRuntime/"searches",searches);
        CopyMissing(legacyRuntime/"exports",exports);
        CopyMissing(legacyRuntime/"cache",cache);
        CopyMissing(legacyRuntime/"progress",progress);
        CopyMissing(legacyRuntime/"reference",references);
        CopyMissing(legacyRuntime/"saved",saved);
        CopyMissing(legacyRuntime/"jobs",jobs);
        CopyMissing(legacyRuntime/"Helpy",saved/"references");
        CopyMissing(diagnostics / "presets", searches / "presets");
        CopyMissing(diagnostics / "trace-profiles", searches / "trace-profiles");
        CopyMissing(diagnostics / "niagara-presets", searches / "presets" / "loaders" / "niagara");
        CopyMissing(diagnostics / "exports", exports);
        CopyMissing(diagnostics / "schema", exports / "schema");
        if (fs::is_directory(diagnostics))
            for (const auto& entry : fs::directory_iterator(diagnostics))
                if (entry.is_regular_file()) CopyMissing(entry.path(), exports / entry.path().filename());
        CopyMissing(root / "diagnostics", exports);
        CopyMissing(root / "dlls" / "runtime", exports);
        for(const auto& [oldName,newName]:std::initializer_list<std::pair<const char*,const char*>>{
            {"npc-catalog.json","Helpy-catalog-npcs.json"},{"ai-catalog.json","Helpy-catalog-ai.json"},
            {"resource-catalog.json","Helpy-catalog-resources.json"},{"item-catalog.json","Helpy-catalog-items.json"},
            {"building-catalog.json","Helpy-catalog-buildings.json"},{"authoring-roster.json","Helpy-catalog-authoring.json"}})
            CopyMissing(legacyRuntime/"reference"/oldName,references/newName);
        CopyMissing(root/"settings"/"F2-catalog-cache.json",cache/"Helpy-catalog-cache.json");
        CopyMissing(root/"settings"/"F2-reference-index.json",references/"Helpy-reference-index.json");
        CopyMissing(root/"settings"/"F2-catalog-sources.json",references/"Helpy-catalog-sources.json");
        CopyMissing(root/"settings"/"F2-catalog-index.json",references/"Helpy-catalog-index.json");
        CopyMissing(root/"settings"/"F2-resource-index.json",references/"Helpy-resource-index.json");
        CopyMissing(root/"settings"/"F2-favorites.json",references/"Helpy-favorites.json");
        // Core startup owns only durable progress and bridge references. Helpy,
        // jobs, searches, exports, and cache create their directories on demand.
        fs::create_directories(progress);fs::create_directories(references);
        const auto legacy=references/"legacy-settings";
        for(const auto* name:{"settings.json","Helpy.json","F2-catalog-cache.json","F2-reference-index.json",
            "F2-catalog-sources.json","F2-catalog-index.json","F2-resource-index.json","F2-favorites.json"})
            ArchiveKnownFile(root/"settings"/name,legacy/name);
        ConfigFiles::Write(marker, "{\"Complete\":true,\"Policy\":\"Copy missing data; known legacy settings archived under saved/references\"}\n");
    }
}
