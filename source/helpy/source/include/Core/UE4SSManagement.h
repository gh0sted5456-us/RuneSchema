#pragma once
#include "Core/UE4SSInventory.h"
#include "Core/CacheBundleIO.h"

namespace PS::UE4SSManagement {
namespace fs=std::filesystem;using Json=nlohmann::json;namespace Order=DragonWilds::ModOrderState;
inline Json Snapshot(const fs::path& root){
    auto result=UE4SSInventory::Read(root);const auto file=root.parent_path()/"mods.txt";const auto order=Order::Read(file);result["ModsTxt"]={{"Exists",order.Exists},{"Contents",order.Contents}};
    for(auto& row:result["Rows"]){
        if(!row.value("ConfiguredKnown",true))continue;
        const auto folder=fs::u8path(row.at("Folder").get<std::string>());
        const auto file=row.value("Managed",false)?ModFolderLayout::FindChildDirectory(root/"mods"/fs::u8path(row.at("Owner").get<std::string>()),"ue4ss")/"status.txt":folder/"enabled.txt";
        ModManager::Safe(file);const bool exists=fs::is_regular_file(file);row["ControlFile"]={{"Exists",exists},{"Contents",exists?ConfigFiles::Read(file,64*1024):std::string{}}};
    }return result;
}
inline std::string SetFlag(std::string text,const std::string& name,bool enabled){
    std::string result;bool found=false;size_t at=0;if(text.starts_with("\xef\xbb\xbf")){result="\xef\xbb\xbf";at=3;}
    while(at<text.size()){
        const auto next=text.find('\n',at);auto line=text.substr(at,next==text.npos?text.size()-at:next-at);const auto comment=line.find_first_of(";#"),colon=line.find(':');
        if(colon!=line.npos&&(comment==line.npos||colon<comment)&&Order::Fold(ModIdentity::Trim(line.substr(0,colon)))==Order::Fold(name)){
            const auto end=comment==line.npos?line.size():comment;const auto start=line.find_first_not_of(" \t",colon+1);const auto last=line.find_last_not_of(" \t\r",end-1);
            if(start==line.npos||start>=end||last<start)line.insert(colon+1,enabled?" 1":" 0");else line.replace(start,last-start+1,enabled?"1":"0");found=true;
        }result+=line;if(next==text.npos)break;result+='\n';at=next+1;
    }
    if(!found){if(!result.empty()&&!result.ends_with('\n'))result+='\n';result+=name+(enabled?" : 1\n":" : 0\n");}return result;
}
inline Json Save(const fs::path& root,const Json& expected,const Json& selected,bool enabled,bool allowExternal,const fs::path& data){
    const auto current=Snapshot(root);const auto name=selected.at("Name").get<std::string>();const bool managed=selected.at("Managed").get<bool>();
    const auto found=std::find_if(current["Rows"].begin(),current["Rows"].end(),[&](const auto& row){return row.at("Name")==name&&row.at("Managed")==managed&&row.at("Owner")==selected.at("Owner");});
    if(found==current["Rows"].end()||found->value("NativeUnsupported",false)||!found->value("ConfiguredKnown",true))throw std::runtime_error("Refresh UE4SS inventory before editing this entry.");
    if(found->at("ControlFile")!=selected.at("ControlFile"))throw std::runtime_error("UE4SS loader settings changed outside Helpy. Refresh before saving.");
    const auto backup=[&](const fs::path& file,const std::string& contents){const auto target=data/"ue4ss-control-backups"/(CacheBundleIO::Hash(file.generic_string()+"\n"+contents)+".txt");CacheBundleIO::Safe(target);if(!fs::exists(target))ConfigFiles::Write(target,contents);};
    const auto& control=selected.at("ControlFile");const Order::Snapshot flag{control.at("Exists").get<bool>(),control.at("Contents").get<std::string>()};
    if(managed){
        const auto owner=selected.at("Owner").get<std::string>();if(!ModManager::Name(owner))throw std::runtime_error("Invalid RuneSchema owner.");
        const auto container=ModFolderLayout::FindChildDirectory(root/"mods"/owner,"ue4ss");if(container.empty())throw std::runtime_error("RuneSchema UE4SS loader folder is missing.");
        const auto status=container/"status.txt";ModManager::Safe(status);if(flag.Exists)backup(status,flag.Contents);
        if(!Order::WriteExpected(status,flag,enabled?"1\n":"0\n"))throw std::runtime_error("UE4SS loader status changed before saving.");
    }else{
        if(!allowExternal)throw std::runtime_error("Enable Manage external UE4SS mods before editing mods.txt.");
        if(!UE4SSPackages::NameSafe(name)||ModFolderLayout::EqualsInsensitive(name,"RuneSchema"))throw std::runtime_error("This external UE4SS entry cannot be edited here.");
        const auto mods=root.parent_path(),file=mods/"mods.txt",marker=mods/fs::u8path(name)/"enabled.txt";ModManager::Safe(file);ModManager::Safe(marker);
        const auto& saved=expected.at("ModsTxt");if(saved!=current.at("ModsTxt"))throw std::runtime_error("UE4SS mods.txt changed outside Helpy. Refresh before saving.");
        const Order::Snapshot before{saved.at("Exists").get<bool>(),saved.at("Contents").get<std::string>()};const auto after=SetFlag(before.Contents,name,enabled);if(before.Exists)backup(file,before.Contents);
        if(!enabled&&flag.Exists)backup(marker,flag.Contents);
        if(!Order::WriteExpected(file,before,after))throw std::runtime_error("UE4SS mods.txt changed before saving.");
        try{if(!enabled&&flag.Exists){if(ConfigFiles::Read(marker,64*1024)!=flag.Contents)throw std::runtime_error("The enabled marker changed during the update.");if(!fs::remove(marker))throw std::runtime_error("Could not remove the backed-up enabled marker.");}}
        catch(...){if(Order::WriteExpected(file,{true,after},before.Contents)&&!before.Exists&&Order::Read(file)==Order::Snapshot{true,std::string{}})fs::remove(file);throw;}
    }
    auto result=Snapshot(root);result["Ok"]=true;result["Status"]="Saved for the next game launch. Running UE4SS mods stay loaded until restart.";return result;
}
}
