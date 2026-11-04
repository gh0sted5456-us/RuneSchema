#include "Core/UE4SSInventory.h"
#include "Core/CacheBundleIO.h"
#include <cassert>
#include <iostream>

using Json=nlohmann::json;namespace fs=std::filesystem;
static void Write(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<text;assert(out.good());}
int main(int argc,char** argv){
    if(argc==3&&std::string_view(argv[1])=="--read"){
        const auto report=PS::UE4SSInventory::Read(fs::path(argv[2]));Json rows=Json::array();for(const auto& row:report.at("Rows"))rows.push_back({{"Name",row.at("Name")},{"Version",row.at("Version")},{"Managed",row.at("Managed")},{"ConfiguredEnabled",row.at("ConfiguredEnabled")},{"Kind",row.at("Kind")}});std::cout<<rows.dump(2)<<'\n';return 0;
    }
    const auto fixture=fs::temp_directory_path()/("runeschema-ue4ss-inventory-"+std::to_string(GetCurrentProcessId()));assert(!fs::exists(fixture));
    const auto mods=fixture/"ue4ss/Mods",root=mods/"RuneSchema";
    Write(mods/"MenuMod/Scripts/main.lua","error('Inventory must never execute Lua')");
    Write(mods/"MenuMod/modmenu.json",R"({"schema":99,"name":"Menu Mod","version":2.5,"settings":[]})");
    Write(mods/"MenuMod/enabled.txt","");Write(mods/"mods.txt","MenuMod : 0\n NoVersion : 1 ; configured through UE4SS\n");
    Write(mods/"NoVersion/Scripts/main.lua","return");
    Write(mods/"Native/dlls/main.dll","not an executable");Write(mods/"Native/modmenu.txt",R"({"version":"1.2b","name":"Native metadata"})");
    Write(mods/"Malformed/Scripts/main.lua","return");Write(mods/"Malformed/modmenu.json","{");
    Write(mods/"shared/utility.lua","not a UE4SS mod entrypoint");
    Write(root/"mods/Owner/ID.txt","ModID: Owner\nName: Owner\nVersion: 3.2\n");
    Write(root/"mods/Owner/ue4ss/Child/Scripts/main.lua","return");
    Write(root/"mods/NativeOwner/ue4ss/dlls/main.dll","never loaded");Write(root/"mods/runeschema.txt","Owner : 0\nNativeOwner : 1\n");
    std::map<fs::path,std::string> before;for(const auto& entry:fs::recursive_directory_iterator(fixture))if(entry.is_regular_file())before[entry.path()]=PS::CacheBundleIO::Hash(PS::ConfigFiles::Read(entry.path(),1024*1024));
    const auto report=PS::UE4SSInventory::Read(root);const auto& rows=report.at("Rows");assert(rows.size()==6);
    const auto find=[&](const char* name)->const Json&{for(const auto& row:rows)if(row.at("Name")==name)return row;throw std::runtime_error("Inventory row absent");};
    assert(find("MenuMod").at("Version")=="2.5"&&find("MenuMod").at("ConfiguredEnabled")==true&&!find("MenuMod").at("Managed").get<bool>());
    assert(find("NoVersion").at("Version")==""&&find("NoVersion").at("ConfiguredEnabled")==true);
    assert(find("Native").at("Version")=="1.2b"&&!find("Native").at("NativeUnsupported").get<bool>()&&find("Native").at("Kind")=="Native");
    assert(find("Malformed").at("Metadata").contains("issue")&&find("Malformed").at("Version")=="");
    assert(find("Child").at("Managed")==true&&find("Child").at("Owner")=="Owner"&&find("Child").at("ConfiguredEnabled")==false&&find("Child").at("Version")=="3.2");
    assert(find("NativeOwner").at("NativeUnsupported")==true);
    for(const auto& [path,hash]:before)assert(PS::CacheBundleIO::Hash(PS::ConfigFiles::Read(path,1024*1024))==hash);
    bool rejected=false;try{(void)PS::UE4SSInventory::Read(fixture);}catch(...){rejected=true;}assert(rejected);
    assert(fs::equivalent(fixture.parent_path(),fs::temp_directory_path())&&fixture.filename().string().starts_with("runeschema-ue4ss-inventory-"));fs::remove_all(fixture);
}
