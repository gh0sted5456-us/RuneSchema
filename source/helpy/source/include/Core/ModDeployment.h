#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/HelpyCacheSources.h"
#include <nlohmann/json.hpp>

namespace PS::ModDeployment {
namespace fs=std::filesystem;using Json=nlohmann::json;
inline fs::path Relative(std::string text){
    std::replace(text.begin(),text.end(),'\\','/');const auto path=fs::u8path(text);
    if(path.is_absolute()||path.has_root_name()||text.find(':')!=text.npos)throw std::runtime_error("Deployment path is not relative.");
    for(const auto& part:path)if(part==".."||part==".")throw std::runtime_error("Deployment path traverses folders.");return path;
}
inline bool Inside(const fs::path& file,const fs::path& folder){
    const auto path=HelpyCacheSources::Lower(fs::absolute(file).lexically_normal().generic_string());
    auto root=HelpyCacheSources::Lower(fs::absolute(folder).lexically_normal().generic_string());if(!root.ends_with('/'))root+='/';return path.starts_with(root);
}
inline bool VortexOwns(const Json& manifest,const fs::path& directory,const fs::path& folder){
    if(!manifest.is_object()||!manifest.contains("files")||!manifest["files"].is_array()||manifest["files"].size()>100000)return false;
    for(const auto& file:manifest["files"])try{
        if(!file.is_object()||!file.contains("relPath")||!file["relPath"].is_string()||file.value("source",std::string{}).empty())continue;
        const auto deployed=directory/Relative(file.value("target",std::string{}))/Relative(file["relPath"].get<std::string>());
        if(Inside(deployed,folder)&&fs::is_regular_file(deployed))return true;
    }catch(...){}return false;
}
inline void Annotate(Json& snapshot){
    if(!snapshot.contains("Root")||!snapshot.contains("Rows"))return;const auto root=fs::u8path(snapshot.at("Root").get<std::string>());
    std::vector<std::pair<fs::path,Json>> manifests;bool vortex=false;
    for(auto at=root;!at.empty();){for(const auto* name:{"vortex.deployment.json","__vortex_deployment.json"}){
        const auto file=at/name;std::error_code error;if(!fs::is_regular_file(file,error))continue;vortex=true;
        try{manifests.emplace_back(at,Json::parse(CacheBundleIO::Read(file,8*1024*1024)));}catch(...){}
    }if(fs::is_directory(at/"Content/Paks"))break;const auto parent=at.parent_path();if(parent==at)break;at=parent;}
    const bool virtualized=GetModuleHandleW(L"usvfs_x64.dll")||GetModuleHandleW(L"usvfs_x86.dll");
    snapshot["VortexDeploymentDetected"]=vortex;snapshot["ModOrganizerVirtualView"]=virtualized;
    for(auto& row:snapshot["Rows"])if(row.value("Present",false))try{
        const auto folder=fs::u8path(row.at("Folder").get<std::string>());row["Origin"]="Not confirmed";
        for(const auto& [directory,manifest]:manifests)if(VortexOwns(manifest,directory,folder)){row["Origin"]="Vortex deployment manifest";break;}
        const auto meta=folder/"meta.ini";if(fs::is_regular_file(meta)){const auto text=CacheBundleIO::Read(meta,128*1024);if(text.find("[General]")!=text.npos&&text.find("installationFile=")!=text.npos)row["Origin"]="Mod Organizer 2 metadata";}
        row["ModOrganizerVirtualView"]=virtualized;
    }catch(const std::exception& error){row["OriginIssue"]=error.what();}
}
}
