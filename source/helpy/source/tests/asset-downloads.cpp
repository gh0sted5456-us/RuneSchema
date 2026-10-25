#include "Runtime/HelpyDownloads.h"
#include "Core/HelpyUiAssets.h"
#include "Core/HelpySplash.h"
#include "Core/BaseCacheDownloads.h"
#include "Core/HelpySourceCache.h"
#include "Runtime/HostServices.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("Asset source and isolated destination required");
    const std::filesystem::path source=argv[1],destination=argv[2];
    using namespace PS;
    const auto nativePack=nlohmann::json{{"Runtime","Steam"},{"GameVersion","same-build"},{"Locale","en-US"},{"Sources",{{"game",std::string(64,'a')},{"futuremajorversion",std::string(64,'b')}}}};
    const auto candidates=[&](const nlohmann::json& pack,const std::string& runtime,const std::string& version="same-build"){return BaseCacheDownloads::Candidates(nlohmann::json::array({pack}),{{"game","active"}},{},runtime,version,"en-US",true);};
    if(candidates(nativePack,"Dedicated server").size()!=1||candidates(nativePack,"Game Pass").size()!=1||candidates(nativePack,"Steam").size()!=1)throw std::runtime_error("Canonical native paths must allow cross-runtime download candidates and inactive official plugin subsets");
    if(candidates(nativePack,"Steam","different-build").size()!=1||!candidates(nativePack,"Unknown").empty())throw std::runtime_error("A different build may offer native artwork references; unknown runtimes must not select packs");
    auto currentPack=nativePack;currentPack["GameVersion"]="new-build";
    const auto ranked=BaseCacheDownloads::Candidates(nlohmann::json::array({nativePack,currentPack}),{{"game","active"}},{},"Steam","new-build","en-US",true);
    if(ranked.size()!=2||ranked.front()["GameVersion"]!="new-build")throw std::runtime_error("The current cook must rank ahead of reference-only fallback candidates");
    if(BaseCacheDownloads::Candidates(nlohmann::json::array({nativePack}),{{"game","active"}},{},"Steam","same-build","de-DE",true).size()!=1)throw std::runtime_error("Native artwork can be offered across locales; metadata still requires the matching locale at import");
    auto modPack=nativePack;modPack["Sources"]["my-mod"]=std::string(64,'c');if(!candidates(modPack,"Dedicated server").empty())throw std::runtime_error("Mod bundles cannot masquerade as the native base cache");
    auto malformed=nativePack;malformed["Sources"]["game"]="bad";if(!candidates(malformed,"Steam").empty())throw std::runtime_error("Invalid portable hashes cannot become candidates");
    const auto isolated=std::filesystem::absolute(destination/"isolated-local");std::filesystem::create_directories(isolated);if(!SetEnvironmentVariableW(L"LOCALAPPDATA",isolated.c_str()))throw std::runtime_error("Cannot isolate download state");
    const auto managed=HostServices::CacheDirectory()/"downloads";std::filesystem::create_directories(managed);
    const auto pack=managed/(std::string(64,'a')+".rsmk-cache"),userPack=destination/"keep-user-pack.rsmk-cache",unknown=managed/"keep-custom.rsmk-cache";
    {std::ofstream f(HelpySourceCache::NativePath(pack));f<<"verified-import-fixture";if(!f)throw std::runtime_error("Cannot create managed fixture");}
    {std::ofstream f(HelpySourceCache::NativePath(userPack));f<<"user";}
    {std::ofstream f(HelpySourceCache::NativePath(unknown));f<<"custom";}
    if(HelpyDownloads::ReleaseImportedPack(userPack)||HelpyDownloads::ReleaseImportedPack(unknown)||!std::filesystem::exists(userPack)||!std::filesystem::exists(unknown))throw std::runtime_error("Cleanup touched a user-owned download");
    const bool released=HelpyDownloads::ReleaseImportedPack(pack);
    if(!released||std::filesystem::exists(HelpySourceCache::NativePath(pack))||HelpyDownloads::ReleaseImportedPack(pack))throw std::runtime_error("Managed import staging cleanup failed (released="+std::to_string(released)+", length="+std::to_string(pack.wstring().size())+")");
    if(HelpyUiAssets::Files.size()!=31||!HelpyUiAssets::Known("power-abovezone.png")||!HelpyUiAssets::Known("power-severelybelowzone.png")||!HelpyUiAssets::Known("scale-null.png")||!HelpyUiAssets::Known("scale-hauler.png")||!HelpyUiAssets::Known("source-base-game.png")||!HelpyUiAssets::Known("source-colorfuldye.png")||!HelpyUiAssets::Known("RuneSchema.png")||HelpyUiAssets::Known("../RuneSchema.png")||HelpyUiAssets::Known("main.dll"))throw std::runtime_error("UI artwork whitelist must contain only fixed PNG names");
    if(HelpySplash::TipIndex(-1)!=0||HelpySplash::TipIndex(0)!=0||HelpySplash::TipIndex(7)!=1||HelpySplash::TipIndex(56)!=0)throw std::runtime_error("Splash tips must rotate predictably");
    HelpyDownloads::TestArtwork(source,destination);
    if(!std::filesystem::is_regular_file(destination/"ready")||HelpyDownloads::Completed()!=HelpyUiAssets::Files.size())throw std::runtime_error("Artwork install did not commit complete readiness");
    const auto stamp=std::filesystem::last_write_time(destination/"RuneSchema.png");
    HelpyDownloads::TestArtwork(source,destination);
    if(std::filesystem::last_write_time(destination/"RuneSchema.png")!=stamp)throw std::runtime_error("Unchanged artwork was rewritten");
    auto longDestination=destination;while(longDestination.wstring().size()<280)longDestination/="long-cache-folder";
    HelpyDownloads::TestArtwork(source,longDestination);
    if(!std::filesystem::is_regular_file(HelpySourceCache::NativePath(longDestination/"ready")))throw std::runtime_error("Long cache paths must support atomic download commits");
    const auto longImage=HelpySourceCache::NativePath(longDestination/"RuneSchema.png");
    const auto oldStamp=std::filesystem::file_time_type::clock::now()-std::chrono::hours(24);std::filesystem::last_write_time(longImage,oldStamp);
    HelpyDownloads::TestArtwork(source,longDestination);
    if(std::filesystem::last_write_time(longImage)!=oldStamp)throw std::runtime_error("Long cache paths must reuse verified artwork");
    {std::ofstream bad(destination/"RuneSchema.png",std::ios::binary|std::ios::trunc);bad<<"corrupt";}
    HelpyDownloads::TestArtwork(source,destination);
    if(std::filesystem::file_size(destination/"RuneSchema.png")!=std::filesystem::file_size(source/"RuneSchema.png"))throw std::runtime_error("Corrupt artwork was not repaired");
    bool rejected=false;try{HelpyDownloads::TestArtwork(source/"missing",destination/"rejected");}catch(...){rejected=true;}
    if(!rejected||std::filesystem::exists(destination/"rejected"/"ready"))throw std::runtime_error("Missing/corrupt download was promoted");
    // An already-running inspection must not undo an explicit Hide click.
    HelpyDownloads::Initialize();HelpyDownloads::DismissHint();
    while(HelpyDownloads::Busy()){HelpyDownloads::Poll();std::this_thread::yield();}
    if(!HelpyDownloads::Current().HintDismissed)throw std::runtime_error("Background artwork inspection undid prompt dismissal");
    HelpyDownloads::Shutdown();HelpyDownloads::Initialize();
    while(HelpyDownloads::Busy()){HelpyDownloads::Poll();std::this_thread::yield();}
    if(!HelpyDownloads::Current().HintDismissed)throw std::runtime_error("Hidden artwork prompt was not retained");
    HelpyDownloads::Shutdown();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
