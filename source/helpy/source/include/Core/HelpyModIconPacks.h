#pragma once
#include "Core/HelpyIconPack.h"
namespace PS::HelpyModIconPacks {
namespace fs=std::filesystem;
struct Bundle {fs::path File;std::string Owner;std::set<std::string> Groups;};
inline bool NotifyAutomatic(const HelpyIconPack::Result& result){return result.Imported||result.RecordsImported||result.Warnings;}
inline std::map<std::string,std::set<std::string>> Groups(const HelpySourceCache::Snapshot& snapshot){
    std::map<std::string,std::set<std::string>> groups;
    for(const auto& [group,owners]:snapshot.Owners)if(snapshot.SourceKeys.contains(group))for(const auto& owner:owners)groups[owner].insert(group);
    return groups;
}
inline fs::path SharedFile(const fs::path& shared,const std::string& source){
    if(!source.starts_with("mods/")||ContentSource::Mod(source.substr(5))!=source)throw std::runtime_error("Choose one mod source to share in core");
    const auto file=shared/"mods"/source.substr(5)/"shared.rsmk-cache";HelpyIconPack::Safe(file);return file;
}
inline std::vector<Bundle> Discover(const fs::path& root,const HelpySourceCache::Snapshot& snapshot,const fs::path& shared={},bool presentation=false){
    if(!presentation&&!snapshot.Authoritative)return {};
    const auto groups=Groups(snapshot);std::vector<Bundle> result;
    const auto scan=[&](const fs::path& mods,bool packaged){HelpyIconPack::Safe(mods);
    if(!fs::is_directory(mods))return;size_t folders=0;
    for(const auto& entry:fs::directory_iterator(mods)){
        if(++folders>1024)throw std::runtime_error("Mod cache discovery exceeds 1024 folders");
        const auto owner=HelpyCacheSources::Lower(entry.path().filename().string());
        if(!entry.is_directory()||(!presentation&&(!groups.contains(owner)||groups.at(owner).empty())))continue;
        try{
            HelpyIconPack::Safe(entry.path());const auto cache=packaged?entry.path()/"cache":entry.path();HelpyIconPack::Safe(cache);
            if(!fs::is_directory(cache))continue;size_t entries=0;
            for(const auto& file:fs::directory_iterator(cache)){
                if(++entries>256)throw std::runtime_error("Mod cache folder exceeds 256 entries");
                const auto extension=HelpyCacheSources::Lower(file.path().extension().string());
                if((extension!=".rsmk-icons"&&extension!=".rsmk-cache")||!file.is_regular_file())continue;
                HelpyIconPack::Safe(file.path());if(result.size()>=64)throw std::runtime_error("Mod cache discovery exceeds 64 packs");
                result.push_back({file.path(),owner,groups.contains(owner)?groups.at(owner):std::set<std::string>{}});
            }
        }catch(const fs::filesystem_error&){continue;}
    }};
    scan(root/"mods",true);if(!shared.empty())scan(shared/"mods",false);
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.File<b.File;});return result;
}
// A mod pack can publish pictures/relationships for its verified ACTIVE roots
// only, even if its manifest claims unrelated Origins. Never loads definitions.
inline HelpyIconPack::Result Import(const Bundle& bundle,const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,uint64_t budget,HelpyIconPack::Progress& progress){
    if(bundle.Groups.empty())throw std::runtime_error("Mod cache has no active verified roots");
    const auto groups=Groups(snapshot);const auto active=groups.find(bundle.Owner);
    for(const auto& group:bundle.Groups)if(active==groups.end()||!active->second.contains(group))throw std::runtime_error("Mod cache ownership is stale");
    return HelpyIconPack::Import(bundle.File,root,cache,snapshot,budget,progress,HelpyIconPack::ImportPolicy::VerifiedContent,{},bundle.Groups,"mods/"+bundle.Owner);
}
inline HelpyIconPack::Result ImportAll(const fs::path& root,const fs::path& cache,const HelpySourceCache::Snapshot& snapshot,uint64_t budget,HelpyIconPack::Progress& progress,const fs::path& shared={},bool presentation=false){
    const auto bundles=Discover(root,snapshot,shared,presentation);HelpyIconPack::Result result;size_t warnings=0;std::set<std::string> changed;std::string details;
    for(const auto& bundle:bundles){HelpyIconPack::Check(progress);
        try{
            const auto stamp=std::to_string(fs::last_write_time(bundle.File).time_since_epoch().count())+"|"+std::to_string(fs::file_size(bundle.File));
            static std::map<fs::path,std::string> importedFiles;
            if(presentation&&importedFiles[bundle.File]==stamp)continue;
            auto imported=presentation?HelpyIconPack::Import(bundle.File,root,cache,snapshot,budget,progress,HelpyIconPack::ImportPolicy::Presentation,{}, {},"mods/"+bundle.Owner):Import(bundle,root,cache,snapshot,budget,progress);
            if(presentation)importedFiles[bundle.File]=stamp;result.Imported+=imported.Imported;result.RecordsImported+=imported.RecordsImported;result.Unchanged+=imported.Unchanged;result.Skipped+=imported.Skipped;
            for(const auto& icon:imported.ChangedIcons)if(changed.size()<HelpyIconPack::MaxIcons)changed.insert(icon);
            if(imported.Skipped)++warnings;
        }catch(const std::exception& error){HelpyIconPack::Check(progress);++warnings;if(details.size()<1024)details+=" "+bundle.Owner+"/"+bundle.File.filename().string()+": "+error.what()+";";}
    }
    result.Message="Mod caches: "+std::to_string(bundles.size())+" pack(s), "+std::to_string(result.Imported)+" new/changed icons, "+std::to_string(result.Unchanged)+" unchanged; "+std::to_string(warnings)+" warning(s).";
    result.Message+=details;result.ChangedIcons.assign(changed.begin(),changed.end());result.Packs=bundles.size();result.Warnings=warnings;
    progress.Stage=HelpyIconPack::Phase::Complete;return result;
}
}
