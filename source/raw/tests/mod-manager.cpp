#include "Core/ModManager.h"
#include "Core/RuntimeModPath.h"
#include "Core/RegistryEnrollment.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    PS::RegistryEnrollment::Reset();
    PS::RegistryEnrollment::RecordDeclaration("Dye","pak:/Dye/Registry");
    PS::RegistryEnrollment::RecordDeclaration("Halloween","registry/dependencies.jsonc","missing native reference");
    auto audit=PS::RegistryEnrollment::DeclarationSnapshot();assert(audit.size()==2&&audit[0].Reason.empty()&&!audit[1].Reason.empty());
    PS::RegistryEnrollment::RecordDeclaration("Halloween","registry/dependencies.jsonc");assert(PS::RegistryEnrollment::DeclarationSnapshot()[1].Reason.empty());
    PS::RegistryEnrollment::Reset();assert(PS::RegistryEnrollment::DeclarationSnapshot().empty());
    if(argc==3&&std::string(argv[1])=="--mapping-paths"){
        try{
        const auto active=std::filesystem::u8path(argv[2]);const auto [cache,host]=PS::RuntimeModPath::MappingRoots(active,active.parent_path().parent_path(),active/"runtime/live/saved/cache");PS::ModManager::Safe(cache);PS::ModManager::Safe(host);
        assert(std::filesystem::equivalent(active,PS::RuntimeModPath::Resolve(active)));
        std::cout<<nlohmann::json{{"Cache",PS::RuntimeModPath::Text(cache)},{"Host",PS::RuntimeModPath::Text(host)},{"ReadOnly",true}}.dump()<<"\n";return 0;
        }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 2;}
    }
    if(argc==3&&(std::string(argv[1])=="--resolve"||std::string(argv[1])=="--read-runtime")){
        const auto resolved=PS::RuntimeModPath::Resolve(std::filesystem::u8path(argv[2]));PS::ModManager::Safe(resolved);
        nlohmann::json result={{"Root",PS::RuntimeModPath::Text(resolved)},{"ReadOnly",true}};
        if(std::string(argv[1])=="--read-runtime"){const auto state=PS::ModManager::Read(resolved,true);result["ModCount"]=state.at("Rows").size();result["Names"]=nlohmann::json::array();for(const auto& row:state.at("Rows"))result["Names"].push_back(row.at("Name"));}
        std::cout<<result.dump()<<"\n";return 0;
    }
    assert(argc==2);namespace fs=std::filesystem;using namespace PS::ModManager;
    const auto root=fs::path(argv[1]);assert(root.filename()=="mod-manager-fixture"&&!fs::exists(root));fs::create_directories(root/"mods/Alpha/equipment");fs::create_directories(root/"mods/Beta/paks");
    const auto canonical=PS::RuntimeModPath::Resolve(root);assert(fs::equivalent(root,canonical));PS::RuntimeModPath::Expected(canonical,PS::RuntimeModPath::Text(canonical));
    fs::create_directories(root/"host/Mods/RuneSchema");
    const auto activeMod=root/"host/Mods/RuneSchema";const auto [mappingCache,mappingHost]=PS::RuntimeModPath::MappingRoots(activeMod,root/"host",activeMod/"runtime/live/saved/cache");
    assert(mappingHost==PS::RuntimeModPath::Resolve(root/"host")&&mappingCache==PS::RuntimeModPath::Resolve(activeMod)/"runtime/live/saved/cache/mappings/automatic");
    bool escapedCache=false;try{(void)PS::RuntimeModPath::MappingRoots(activeMod,root/"host",root/"elsewhere");}catch(...){escapedCache=true;}assert(escapedCache);
    bool wrongHost=false;try{(void)PS::RuntimeModPath::MappingRoots(activeMod,root,activeMod/"runtime/live/saved/cache");}catch(...){wrongHost=true;}assert(wrongHost);
    bool otherRuntime=false;try{PS::RuntimeModPath::Expected(canonical,PS::RuntimeModPath::Text(root/"another-runtime"));}catch(...){otherRuntime=true;}assert(otherRuntime);
    bool absentRoot=false;try{(void)PS::RuntimeModPath::Resolve(root/"missing");}catch(...){absentRoot=true;}assert(absentRoot);
    PS::ConfigFiles::Write(root/"mods/runeschema.txt","; preserve this comment\nAlpha : 0\nBeta : 1\n");
    PS::ConfigFiles::Write(root/"mods/Alpha/mod.json",R"({"format":1,"id":"Alpha","version":"1.2","description":"example"})");
    PS::ConfigFiles::Write(root/"mods/Beta/mod.json",R"({"id":"Beta","name":"Legacy author metadata","version":"2","contentRoot":"/Game/Mods/Beta","website":"https://example.com/mod"})");
    const auto legacy=Metadata(root/"mods/Beta");assert(legacy.at("name")=="Legacy author metadata"&&legacy.at("website")=="https://example.com/mod");
    PS::ConfigFiles::Write(root/"mods/Beta/mod.json",R"({"id":"Beta","website":"https://example.com/\u0000bad"})");assert(!Metadata(root/"mods/Beta").contains("website"));
    assert(Name("RSDWSpears")&&!Name("../bad")&&!Name("CON")&&!Name("bad ")&&!Name("a:b"));
    const auto identity=PS::ModIdentity::Parse("ModID=stable\nName=Original\nNickname=Friendly name\nVersion=1.2\nAuthor=Example","Alpha");assert(identity.Id=="stable"&&identity.Name=="Original"&&identity.DisplayName()=="Friendly name"&&identity.Version=="1.2");
    assert(PS::ModIdentity::Parse("LegacyID","Alpha").Id=="LegacyID");
    {const auto extended=PS::ModIdentity::Parse("ModID=stable\nVersion=V142\nBuild=local-2\nNexusModID=123\nNexusFileID=Unknown\nDescription=Owned source cache\nBanner=cache/banner.png","Folder");
        assert(extended.Id=="stable"&&extended.Build=="local-2"&&extended.NexusModID=="123"&&extended.NexusFileID.empty()&&extended.Description=="Owned source cache"&&extended.Banner=="cache/banner.png");
        assert(PS::ModIdentity::Parse("NexusFileID=456","Folder").NexusFileID=="456"&&PS::ModIdentity::Parse("NexusFileID=unset","Folder").NexusFileID.empty());
        for(const auto* invalid:{"NexusFileID=-1","NexusModID=abc","Banner=../escape.png","Banner=C:/outside.png","Banner=cache/payload.dll"}){bool rejected=false;try{(void)PS::ModIdentity::Parse(invalid,"Folder");}catch(...){rejected=true;}assert(rejected);}
        PS::ConfigFiles::Write(root/"mods/Alpha/ID.txt","ModID=stable-alpha\nName=Alpha display\nBuild=test\nNexusModID=123\nNexusFileID=456\nDescription=From ID.txt\n");const auto info=Metadata(root/"mods/Alpha");assert(info["modId"]=="stable-alpha"&&info["name"]=="Alpha display"&&info["build"]=="test"&&info["nexusModId"]=="123"&&info["nexusFileId"]=="456"&&info["description"]=="example");
    }
    auto state=Read(root,true);assert(state["Rows"].size()==2&&state["Rows"][0]["Name"]=="Alpha"&&state["Rows"][0]["Enabled"]==false);
    fs::create_directories(root/"mods/Beta/cache");PS::ConfigFiles::Write(root/"mods/Beta/cache/mod.png","fixture-pixels");
    const auto banner=Read(root,true);assert(banner["Rows"][1]["BannerRelative"]=="cache/mod.png");
    auto request=state;request["Rows"][0]["Enabled"]=true;std::swap(request["Rows"][0],request["Rows"][1]);
    const auto saved=Save(root,true,request);assert(saved["Rows"][0]["Name"]=="Beta"&&saved["Rows"][1]["Enabled"]==true);
    assert(PS::ConfigFiles::Read(root/"mods/runeschema.txt").find("preserve this comment")!=std::string::npos);
    bool stale=false;try{Save(root,true,request);}catch(...){stale=true;}assert(stale);
    bool disabled=false;try{Save(root,false,saved);}catch(...){disabled=true;}assert(disabled);
    auto removed=saved;removed["Rows"].erase(removed["Rows"].begin());bool missing=false;try{Save(root,true,removed);}catch(...){missing=true;}assert(missing);
    PS::ConfigFiles::Write(root/"mods/runeschema.txt",PS::ConfigFiles::Read(root/"mods/runeschema.txt")+"Gone : 0\nRetired : 1\n");
    auto orphaned=Read(root,true);assert(orphaned["Rows"].size()==4);
    std::erase_if(orphaned["Rows"].get_ref<Json::array_t&>(),[](const auto& row){return row.at("Name")=="Gone";});
    auto one=Save(root,true,orphaned);assert(one["Rows"].size()==3);
    assert(std::any_of(one["Rows"].begin(),one["Rows"].end(),[](const auto& row){return row.at("Name")=="Retired"&&!row.at("Present").template get<bool>();}));
    std::erase_if(one["Rows"].get_ref<Json::array_t&>(),[](const auto& row){return !row.at("Present").template get<bool>();});
    const auto cleaned=Save(root,true,one);assert(cleaned["Rows"].size()==2&&PS::ConfigFiles::Read(root/"mods/runeschema.txt").find("preserve this comment")!=std::string::npos);
    auto foreign=cleaned;foreign["Rows"].push_back({{"Name","Unlisted"},{"Enabled",true},{"Present",false}});
    bool unknown=false;try{Save(root,true,foreign);}catch(...){unknown=true;}assert(unknown);
    PS::ConfigFiles::Write(root/"mods/runeschema.txt",PS::ConfigFiles::Read(root/"mods/runeschema.txt")+"Returning : 0\n");
    auto returning=Read(root,true);std::erase_if(returning["Rows"].get_ref<Json::array_t&>(),[](const auto& row){return row.at("Name")=="Returning";});
    fs::create_directories(root/"mods/Returning/equipment");bool reappeared=false;try{Save(root,true,returning);}catch(...){reappeared=true;}assert(reappeared);
    assert(fs::is_directory(root/"mods/Beta/paks")&&fs::is_directory(root/"mods/Returning/equipment"));
    assert(fs::is_directory(root/"mods/Alpha/equipment"));
    std::cout<<"Mod manager: metadata, order preservation, CAS, disabled policy, complete-list guard passed.\n";
    fs::remove_all(root);
}
