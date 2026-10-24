#include "Core/MappingCompatibility.h"
#include <fstream>
#include <chrono>
#include <cassert>
#include <iostream>
#ifdef _WIN32
#include "Core/RuntimeModPath.h"
#endif
int main(int argc,char** argv){
    namespace fs=std::filesystem;using namespace PS::MappingCompatibility;
#ifdef _WIN32
    if(argc==4&&std::string_view(argv[1])=="--runtime"){
        const auto begin=std::chrono::steady_clock::now();const auto mod=PS::RuntimeModPath::Resolve(fs::u8path(argv[3]));
        const auto identity=Identity(fs::u8path(argv[2]),mod.parent_path().parent_path(),mod,"read-only-test");
        std::cout<<"PASS read-only runtime compatibility scan: identity_bytes="<<identity.size()<<" elapsed_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()<<"\n";return 0;
    }
#endif
    const auto root=fs::temp_directory_path()/("mapping-compatibility-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto host=root/"Game/Binaries/Win64/ue4ss",mod=host/"Mods/RuneSchema",binary=host.parent_path()/"Game.exe";
    const auto write=[](const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<text;assert(out.good());};
    write(binary,"game");write(host/"UE4SS.dll","ue4ss");write(mod/"dlls/main.dll","runeschema");
    const auto key=[&]{return Identity(binary,host,mod,"game-version");};const auto original=key();assert(key()==original);
    write(mod/"mods/Dye/cache/item.json","cached metadata");write(mod/"mods/Dye/art.png","art");assert(key()==original);
    write(mod/"runtime/live/saved/cache/mappings/automatic/generated.usmap","mapping");assert(key()==original);
    write(mod/"mods/Dye/paks/Dye.PAK","pak");assert(key()==original);
    write(mod/"mods/Dye/registry/dependencies.jsonc","declarations");assert(key()==original);
    write(mod/"mods/runeschema.txt","Dye : 0");write(mod/"plugins/plugins.txt","Helpy : 1");assert(key()==original);
    write(host/"UE4SS.dll","ue4ss update");write(mod/"dlls/main.dll","runeschema update");assert(key()==original);
    write(root/"Game/Content/Paks/patch.utoc","game update");const auto game=key();assert(game!=original);
    write(root/"Game/Plugins/NativeFeature/Content/Paks/Native.utoc","native game plugin update");const auto feature=key();assert(feature!=game);
    write(binary,"updated game executable");assert(key()!=feature);
    assert(Identity(binary,host,mod,"another-game-version")!=key());
    assert(Input("Content.UCAS",false)&&!Input("art.png",true)&&!Input("data.json",false)&&Input("data.JSONC",true));
}
