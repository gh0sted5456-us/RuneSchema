#pragma once
#include "Core/ModMenuGate.h"
#include "Core/UE4SSPackages.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/CacheBundleIO.h"

namespace PS::ModMenuFiles {
using Json=nlohmann::json;
namespace fs=std::filesystem;
class Scope {
    fs::path Root,UE4SSMods;
    std::map<std::string,fs::path> ReadRoots,WriteFiles;
    static Json Parse(const std::string& text,bool comments=false){
        return Json::parse(text,[](int depth,Json::parse_event_t,Json&){
            if(depth>32)throw std::runtime_error("Mod Menu file nesting is too deep.");
            return true;
        },true,comments);
    }
public:
    void Open(const fs::path& root){
        Root=RuntimeModPath::Resolve(root);
        UE4SSMods=RuntimeModPath::Resolve(root.parent_path());
    }
    static std::string FoldPath(const fs::path& path){const auto key=fs::absolute(path).lexically_normal().generic_u8string();return ModManager::Order::Fold(std::string(key.begin(),key.end()));}
    Json Catalog(){
        Json pages=Json::array();ReadRoots.clear();WriteFiles.clear();const auto add=[&](const fs::path& folder,const std::string& name){
            auto page=Json{{"Folder",name},{"Path",RuntimeModPath::Text(folder)}};
            try{
                fs::path schema;
                for(const auto* leaf:{"modmenu.json","modmenu.txt"}){const auto file=folder/leaf;if(fs::is_regular_file(file)){schema=file;break;}}
                if(schema.empty())return;
                ModManager::Safe(schema);
                const auto doc=Parse(ConfigFiles::Read(schema,512*1024),true);const auto config=doc.value("config",Json::object());auto relative=config.value("file",config.value("format",std::string{})=="lua"?std::string("Scripts/main.lua"):std::string("config.txt"));
                if(relative.empty()||relative.find("..")!=relative.npos||relative.find(':')!=relative.npos||fs::u8path(relative).has_root_path()||std::any_of(relative.begin(),relative.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Config path leaves its mod folder.");const auto file=(folder/fs::u8path(relative)).lexically_normal();
                if(FoldPath(file)==FoldPath(folder)||FoldPath(file)==FoldPath(schema)||file.filename().empty())
                    throw std::runtime_error("Config must be a separate file inside its mod folder.");
                ModManager::Safe(file);ReadRoots[FoldPath(folder)]=folder;WriteFiles[FoldPath(file)]=file;
            }catch(const std::exception& error){page["Error"]=error.what();}
            pages.push_back(std::move(page));
        };
        std::string warning;
        size_t scanned=0;
        for(const auto& entry:fs::directory_iterator(UE4SSMods)){
            if(++scanned>4096)throw std::runtime_error("Too many UE4SS folders to scan.");
            std::error_code error;
            if(!entry.is_directory(error))continue;
            const auto name=RuntimeModPath::Text(entry.path().filename());
            if(ModMenuGate::Identity(name)!="runeschema")add(entry.path(),name);
        }
        try{
            const auto parents=ModManager::Read(Root,true);
            for(const auto& row:parents.at("Rows"))if(row.value("Enabled",false)&&row.value("Present",false))try{
                for(const auto& package:UE4SSPackages::Discover(fs::u8path(row.at("Folder").get<std::string>()),row.at("Name").get<std::string>()))
                    if(package.Enabled&&package.Native.empty())add(package.Root,package.Name);
            }catch(const std::exception& error){warning=error.what();}
        }catch(const std::exception& error){warning=error.what();}
        return {{"Pages",std::move(pages)},{"Warning",std::move(warning)}};
    }
    fs::path Allowed(const Json& request,bool writing){const auto path=fs::absolute(fs::u8path(request.at("Path").get<std::string>())).lexically_normal();const auto key=FoldPath(path);bool allowed=writing?WriteFiles.contains(key):key==FoldPath(UE4SSMods/"mods.txt")||std::any_of(ReadRoots.begin(),ReadRoots.end(),[&](const auto& entry){return key.starts_with(entry.first+"/");});if(!allowed)throw std::runtime_error("Mod Menu file is outside its registered mod scope.");ModManager::Safe(path);return path;}
    Json Read(const Json& request){
        const auto file=Allowed(request,false);
        const bool exists=fs::exists(file);
        return {{"Exists",exists},{"Text",exists?ConfigFiles::Read(file,512*1024):std::string{}}};
    }
    Json Write(const Json& request){
        const auto file=Allowed(request,true);
        auto after=request.at("Text").get<std::string>();
        const DragonWilds::ModOrderState::Snapshot before{request.at("Exists").get<bool>(),request.at("Expected").get<std::string>()};
        if(after.size()>512*1024||before.Contents.size()>512*1024)throw std::runtime_error("Mod config is too large.");
        if(request.value("JsonPatch",false)){
            auto document=before.Exists?Parse(before.Contents,true):Json::object();
            const auto values=Parse(after);
            if(!document.is_object()||!values.is_object())throw std::runtime_error("Expected a JSON settings object. Original config retained.");
            document.update(values);
            after=document.dump(2)+"\n";
            if(after.size()>512*1024)throw std::runtime_error("Mod config is too large.");
        }
        if(before.Exists){
            const auto backup=RuneSchemaData::SharedDirectory("HELPY")/"mod-menu-backups"/(CacheBundleIO::Hash(FoldPath(file)+before.Contents)+".txt");
            CacheBundleIO::Safe(backup);
            if(!fs::exists(backup))ConfigFiles::Write(backup,before.Contents);
        }
        return {{"Saved",ModManager::Order::WriteExpected(file,before,after)}};
    }
};
}
