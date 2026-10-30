#pragma once
#include "Loader/ModOrderState.h"
#include "Loader/ModOrderPolicy.h"
#include "Utility/ModFolderLayout.h"
#include "Core/ConfigFiles.h"
#include "Core/ModIdentity.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>
#include <mutex>
#include <windows.h>

// Filesystem-only operations. Never unload packages, destroy actors or remove
// installed files. The loader remains authoritative for the current session.
namespace PS::ModManager {
using Json=nlohmann::json;
namespace Order=DragonWilds::ModOrderState;
inline std::mutex Gate;
inline bool Name(std::string_view name){
    if(name.empty()||name.size()>96||name.front()=='.'||name.back()=='.'||name.back()==' ')return false;
    for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==' '))return false;
    auto lower=Order::Fold(name);const auto stem=lower.substr(0,lower.find('.'));
    return stem!="con"&&stem!="prn"&&stem!="aux"&&stem!="nul"&&!(stem.size()==4&&(stem.starts_with("com")||stem.starts_with("lpt"))&&stem[3]>='0'&&stem[3]<='9');
}
inline void Safe(const std::filesystem::path& path){
    auto current=std::filesystem::absolute(path).lexically_normal();
    while(!current.empty()){
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("A mod-management path contains a link/reparse point.");
        const auto parent=current.parent_path();if(parent==current)break;current=parent;
    }
}
inline Json Metadata(const std::filesystem::path& folder){
    Json display=Json::object();const auto identityFile=folder/"ID.txt";Safe(identityFile);
    if(std::filesystem::exists(identityFile)){const auto identity=ModIdentity::Parse(ConfigFiles::Read(identityFile,64*1024),folder.filename().string());
        display={{"name",identity.Name},{"nickname",identity.Nickname},{"author",identity.Author},{"version",identity.Version},{"website",identity.Website},{"modId",identity.Id},{"build",identity.Build},{"nexusModId",identity.NexusModID},{"nexusFileId",identity.NexusFileID},{"description",identity.Description}};
        if(!identity.Banner.empty())display["banner"]=identity.Banner;
    }
    const auto file=folder/"mod.json";Safe(file);
    if(!std::filesystem::exists(file))return display;
    auto document=Json::parse(ConfigFiles::Read(file,128*1024));
    if(!document.is_object()||(document.contains("format")&&document.at("format")!=1)||document.value("id",folder.filename().string())!=folder.filename().string())throw std::runtime_error("mod.json has an unsupported format or a different folder ID.");
    for(const auto* key:{"name","version","author","description","banner"})if(document.contains(key)&&document[key].is_string())display[key]=document[key].get<std::string>().substr(0,1024);
    for(const auto* key:{"website","url"})if(document.value(key,Json{}).is_string()){const auto url=document.at(key).get<std::string>();if(url.size()<=2048&&(url.starts_with("https://")||url.starts_with("http://"))&&std::none_of(url.begin(),url.end(),[](unsigned char c){return c<32||c==127;}))display["website"]=url;}
    if(document.value("contentRoot",Json{}).is_string())display["contentRoot"]=document.at("contentRoot").get<std::string>().substr(0,1024);
    if(document.contains("history")&&document["history"].is_array()){
        display["history"]=Json::array();for(const auto& row:document["history"]){if(display["history"].size()>=12)break;if(!row.is_object())continue;
            Json change=Json::object();for(const auto* key:{"version","notes"})if(row.contains(key)&&row[key].is_string())change[key]=row[key].get<std::string>().substr(0,256);display["history"].push_back(std::move(change));}
    }return display;
}
inline Json Read(const std::filesystem::path& root,bool orderEnabled){
    Safe(root);const auto mods=root/"mods",file=mods/"runeschema.txt";Safe(file);
    const auto before=Order::Read(file);std::vector<std::string> names;
    if(std::filesystem::is_directory(mods))for(const auto& entry:std::filesystem::directory_iterator(mods)){
        if(names.size()>=1024)throw std::runtime_error("More than 1024 mod folders; management refused.");
        if(entry.is_directory()&&!entry.is_symlink()&&Name(entry.path().filename().string())){Safe(entry.path());names.push_back(entry.path().filename().string());}
    }
    std::sort(names.begin(),names.end(),[](const auto&a,const auto&b){return Order::Fold(a)<Order::Fold(b);});
    auto entries=Order::Parse(before.Contents);
    // Explicitly display implicit AA_/ZZ_ rows too, in their effective order.
    const auto plan=Order::Reconcile(entries,names);std::vector<Order::Entry> rows;
    std::set<std::string> explicitNames;for(const auto& item:plan.Entries)explicitNames.insert(Order::Fold(item.Name));
    for(const auto& name:plan.Active)if(!explicitNames.contains(Order::Fold(name))&&DragonWilds::ModOrderPolicy::Priority(name)==0)rows.push_back({name,true});
    for(const auto& item:plan.Entries)rows.push_back(item);
    for(const auto& name:plan.Active)if(!explicitNames.contains(Order::Fold(name))&&DragonWilds::ModOrderPolicy::Priority(name)!=0)rows.push_back({name,true});
    Json output=Json::array();std::set<std::string> seen;
    for(const auto& item:rows){if(!seen.insert(Order::Fold(item.Name)).second)continue;
        const auto folder=mods/item.Name;Safe(folder);const bool exists=std::filesystem::is_directory(folder);
        Json row={{"Name",item.Name},{"Enabled",item.Enabled},{"Present",exists},{"Folder",folder.string()},{"Metadata",Json::object()},{"Loaders",Json::array()}};
        if(exists){try{row["Metadata"]=Metadata(folder);
                if(!row["Metadata"].contains("banner")&&std::filesystem::is_regular_file(folder/"cache"/"mod.png"))row["Metadata"]["banner"]="cache/mod.png";
                if(row["Metadata"].contains("banner")){
                    const auto relative=std::filesystem::u8path(row["Metadata"]["banner"].get<std::string>());
                    if(relative.empty()||relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Banner must be a relative mod-owned image path.");
                    for(const auto& part:relative)if(part==".."||part=="."||part.string().find(':')!=std::string::npos)throw std::runtime_error("Unsafe mod banner path.");
                    const auto art=folder/relative;Safe(art);const auto extension=ModFolderLayout::AsciiLower(art.extension().string());
                    if((extension!=".png"&&extension!=".jpg"&&extension!=".jpeg"&&extension!=".webp")||!std::filesystem::is_regular_file(art)||std::filesystem::file_size(art)>20*1024*1024)throw std::runtime_error("Banner must be an existing PNG/JPEG/WebP below 20 MiB.");
                    row["BannerRelative"]=relative.generic_string();
                }
            }catch(const std::exception& error){row["Issue"]=error.what();}
            for(const auto& entry:std::filesystem::directory_iterator(folder))if(entry.is_directory()&&!entry.is_symlink()){
                const auto name=ModFolderLayout::AsciiLower(entry.path().filename().string());
                if(name=="paks")row["Loaders"].push_back("Unreal PAK");else if(name=="logicmods")row["Loaders"].push_back("LogicMods");
                else if(name=="ue4ss")row["Loaders"].push_back("UE4SS");
                else if(ModFolderLayout::IsContentDirectoryName(name))row["Loaders"].push_back("RuneSchema: "+name);
            }
            row["HasCache"]=std::filesystem::is_directory(folder/"cache");
        }output.push_back(std::move(row));
    }
    return {{"Ok",true},{"OrderEnabled",orderEnabled},{"ExpectedExists",before.Exists},{"ExpectedContents",before.Contents},{"Rows",std::move(output)}};
}
inline Json Save(const std::filesystem::path& root,bool enabled,const Json& request){
    std::lock_guard lock(Gate);if(!enabled)throw std::runtime_error("Enable RuneSchema load-order handling in Settings first.");
    const auto current=Read(root,enabled);const Order::Snapshot expected{request.at("ExpectedExists").get<bool>(),request.at("ExpectedContents").get<std::string>()};
    if(expected.Contents.size()>8*1024*1024||current.at("ExpectedContents")!=expected.Contents||current.at("ExpectedExists")!=expected.Exists)throw std::runtime_error("Mod order changed externally. Refresh before applying.");
    std::set<std::string> allowed,required,provided;for(const auto& row:current.at("Rows")){
        const auto name=Order::Fold(row.at("Name").get<std::string>());allowed.insert(name);if(row.value("Present",true))required.insert(name);
    }
    std::vector<Order::Entry> entries;const auto& rows=request.at("Rows");if(!rows.is_array()||rows.size()>1024)throw std::runtime_error("Invalid mod order.");
    for(const auto& row:rows){const auto name=row.at("Name").get<std::string>();if(!Name(name)||!provided.insert(Order::Fold(name)).second)throw std::runtime_error("Invalid or duplicate mod folder.");entries.push_back({name,row.at("Enabled").get<bool>()});}
    if(!std::includes(provided.begin(),provided.end(),required.begin(),required.end())
        ||!std::includes(allowed.begin(),allowed.end(),provided.begin(),provided.end()))throw std::runtime_error("The mod list changed. Refresh before applying.");
    const auto file=root/"mods"/"runeschema.txt";Safe(file);
    if(expected.Exists)ConfigFiles::Backup(file);
    if(!Order::WriteExpected(file,expected,Order::Serialize(entries,expected,true)))throw std::runtime_error("Mod order changed externally; nothing applied.");
    auto result=Read(root,enabled);result["Status"]="Saved. Restart required; currently mounted content is unchanged.";return result;
}
}
