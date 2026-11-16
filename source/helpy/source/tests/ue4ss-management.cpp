#include "Core/UE4SSManagement.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("UE4SS management regression");}
int main(){
    namespace fs=std::filesystem;namespace M=PS::UE4SSManagement;using Json=nlohmann::json;
    const auto fixture=fs::temp_directory_path()/("ue4ss-management-"+std::to_string(GetCurrentProcessId()));const auto mods=fixture/"ue4ss/Mods",root=mods/"RuneSchema",data=fixture/"AppData";
    const auto write=[](const fs::path& file,const std::string& text){PS::ConfigFiles::Write(file,text);};
    const auto find=[](const Json& snapshot,const char* name){for(const auto& row:snapshot["Rows"])if(row["Name"]==name)return row;throw std::runtime_error("Missing UE4SS row");};
    write(mods/"Example/Scripts/main.lua","error('never execute')");write(mods/"Example/enabled.txt","");write(mods/"Other/Scripts/main.lua","return");
    const std::string list="; keep this comment\r\nExample : 1 ; keep annotation\r\nOther : 0\r\nRuneSchema : 1\r\n";write(mods/"mods.txt",list);
    write(root/"mods/Owner/ue4ss/Child/Scripts/main.lua","return");write(root/"mods/Owner/paks/data.pak","unchanged PAK");write(root/"mods/runeschema.txt","Owner : 1\n");
    try{
        auto state=M::Snapshot(root);auto row=find(state,"Example");Check(row["ConfiguredEnabled"]==true);
        bool refused=false;try{M::Save(root,state,row,false,false,data);}catch(...){refused=true;}Check(refused&&PS::ConfigFiles::Read(mods/"mods.txt")==list&&fs::exists(mods/"Example/enabled.txt"));
        auto disabled=M::Save(root,state,row,false,true,data);Check(find(disabled,"Example")["ConfiguredEnabled"]==false&&!fs::exists(mods/"Example/enabled.txt"));
        Check(PS::ConfigFiles::Read(mods/"mods.txt")=="; keep this comment\r\nExample : 0 ; keep annotation\r\nOther : 0\r\nRuneSchema : 1\r\n");Check(fs::is_directory(data/"ue4ss-control-backups"));
        refused=false;try{M::Save(root,state,row,true,true,data);}catch(...){refused=true;}Check(refused);
        auto enabled=M::Save(root,disabled,find(disabled,"Example"),true,true,data);Check(find(enabled,"Example")["ConfiguredEnabled"]==true);
        auto managed=M::Save(root,enabled,find(enabled,"Child"),false,false,data);Check(find(managed,"Child")["ConfiguredEnabled"]==false&&PS::ConfigFiles::Read(root/"mods/Owner/ue4ss/status.txt")=="0\n");
        const auto packages=PS::UE4SSPackages::Discover(root/"mods/Owner","Owner");Check(packages.size()==1&&!packages[0].Enabled);Check(PS::UE4SSPackages::Manifest(root/"mods",packages).find("\tlua\n")==std::string::npos);
        Check(PS::ConfigFiles::Read(root/"mods/Owner/paks/data.pak")=="unchanged PAK"&&PS::ConfigFiles::Read(root/"mods/runeschema.txt")=="Owner : 1\n");
        write(root/"mods/Owner/ue4ss/status.txt","1\n");Check(find(M::Snapshot(root),"Child")["ConfiguredEnabled"]==true);
        const auto owner=root/"mods/Owner",metadata=root/"mods/MetadataOnly";
        Check(PS::UE4SSPackages::HudRoots(owner,"Owner").size()==2);
        write(owner/"ue4ss/status.txt","0");Check(PS::UE4SSPackages::HudRoots(owner,"Owner").empty());
        write(metadata/"ue4ss/hud.json","{}");Check(PS::UE4SSPackages::Discover(metadata,"MetadataOnly").empty());
        const auto roots=PS::UE4SSPackages::HudRoots(metadata,"MetadataOnly");Check(roots.size()==1&&roots[0]==metadata/"ue4ss");
        Check(PS::UE4SSPackages::Discover(metadata,"MetadataOnly").empty());
        write(metadata/"ue4ss/status.txt","0");Check(PS::UE4SSPackages::HudRoots(metadata,"MetadataOnly").empty());
        Check(M::SetFlag("Example: 1\nExample : 1 # duplicate\n","Example",false)=="Example: 0\nExample : 0 # duplicate\n");
        Check(M::SetFlag("\xef\xbb\xbf" "Example: 1\r\n","Example",false)=="\xef\xbb\xbf" "Example: 0\r\n");
    }catch(...){fs::remove_all(fixture);throw;}fs::remove_all(fixture);
}
