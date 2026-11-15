#include "Core/RuneSchemaDataRoot.h"
#include "Core/ConfigFiles.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <fstream>
int main(int argc,char** argv)try{
    namespace fs=std::filesystem;namespace R=PS::RuneSchemaData;
    const auto print=[](const fs::path& path){const auto text=path.generic_u8string();std::cout.write(reinterpret_cast<const char*>(text.data()),text.size());std::cout<<"\n";};
    if(argc==2&&std::string_view(argv[1])=="--save-paths"){
        const auto profile=R::CharacterCopiesDirectory();
        const auto copies=profile/"00112233445566778899aabbccddeeff";
        R::Safe(copies/"snapshots");
        R::Safe(copies/"exports");
        print(profile);
        const auto jobs=R::PlatformDirectory("steam")/"runtime/jobs";
        print(R::ResolveSaveRoot(jobs/"imports/saves"));
        print(R::ResolveSaveRoot(jobs/"exports/saves"));
        return 0;
    }
    if(argc==2&&std::string_view(argv[1])=="--paths"){
        print(R::Directory());
        for(const auto* platform:{"steam","gamepass","epic","server"})print(R::MappingDirectory(platform));
        return 0;
    }
    const auto root=fs::temp_directory_path()/("runeschema-data-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto local=root/"local",old=local/"RuneSchema",next=local/"RSDragonwilds/RuneSchema";
    const auto write=[](const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream stream(file,std::ios::binary);stream<<text;assert(stream.good());};
    assert(R::Resolve(local)==next&&!fs::exists(next));
    write(old/"runeschema.profile/profile.json","old profile");
    write(next/"runeschema.profile/profile.json","current profile");
    write(next/"HELPY/ui.jsonc","user theme");
    assert(R::Resolve(local)==next&&PS::ConfigFiles::Read(old/"runeschema.profile/profile.json")=="old profile");
    assert(R::SharedDirectory(next,"runeschema.profile")==next/"runeschema.profile");
    assert(R::SharedDirectory(next,"HELPY")==next/"HELPY");
    assert(PS::ConfigFiles::Read(next/"HELPY/ui.jsonc")=="user theme"&&!fs::exists(next/"core"));
    write(next/"core/runeschema.profile/profile.json","established core profile");
    assert(R::SharedDirectory(next,"runeschema.profile")==next/"core/runeschema.profile");
    assert(PS::ConfigFiles::Read(next/"runeschema.profile/profile.json")=="current profile");
    assert(R::SharedDirectory(next,"health")==next/"core/health"&&!fs::exists(next/"core/health"));
    for(const auto* platform:{"STEAM","GAMEPASS","EPIC","SERVER"}){
        const auto legacy=next/"cache"/platform/"mappings/keep.usmap";write(legacy,platform);
        const auto destination=next/R::RuntimeKey(platform)/"cache/mappings";
        assert(R::MappingDirectory(next,platform)==destination&&!fs::exists(destination));
        assert(PS::ConfigFiles::Read(legacy)==platform);
    }
    bool failed=false;try{R::RuntimeKey("../escape");}catch(...){failed=true;}assert(failed);
    failed=false;try{R::SharedDirectory(next,"../escape");}catch(...){failed=true;}assert(failed);
    const auto outside=root/"outside";fs::create_directory(outside);
    const auto staleLink=old/"redirect";std::error_code error;fs::create_directory_symlink(outside,staleLink,error);
    if(!error){assert(R::Resolve(local)==next);fs::remove(staleLink);}
    const auto characters=next/"core/runeschema.profile/characters";
    error.clear();fs::create_directory_symlink(outside,characters,error);
    if(!error){assert(R::CharacterCopiesDirectory(next)==outside);fs::remove(characters);}
    fs::create_directories(next/"steam/cache");const auto cacheLink=next/"steam/cache/mappings";
    error.clear();fs::create_directory_symlink(outside,cacheLink,error);
    if(!error){failed=false;try{R::MappingDirectory(next,"steam");}catch(...){failed=true;}assert(failed);fs::remove(cacheLink);}
    assert(root.filename().string().starts_with("runeschema-data-"));fs::remove_all(root);
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
