#include "Runtime/HelpyDownloads.h"
#include "Core/HelpyUiAssets.h"
#include "Core/HelpySplash.h"
#include "Core/BaseCacheDownloads.h"
#include "Runtime/HostServices.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
int main(int argc,char** argv){
    if(argc!=3)throw std::runtime_error("Asset source and isolated destination required");
    const std::filesystem::path source=argv[1],destination=argv[2];
    using namespace PS;
    const auto nativePack=nlohmann::json{{"Runtime","Steam"},{"GameVersion","same-build"},{"Locale","en-US"},{"Sources",{{"game",std::string(64,'a')},{"futuremajorversion",std::string(64,'b')}}}};
    const auto candidates=[&](const nlohmann::json& pack,const std::string& runtime,const std::string& version="same-build"){return BaseCacheDownloads::Candidates(nlohmann::json::array({pack}),{{"game","active"}},{},runtime,version,"en-US",true);};
    if(candidates(nativePack,"Dedicated server").size()!=1||candidates(nativePack,"Game Pass").size()!=1||candidates(nativePack,"Steam").size()!=1)throw std::runtime_error("Canonical native paths must allow cross-runtime download candidates and inactive official plugin subsets");
    if(!candidates(nativePack,"Steam","different-build").empty()||!candidates(nativePack,"Unknown").empty())throw std::runtime_error("Unknown builds/runtimes must not select a pack");
    auto modPack=nativePack;modPack["Sources"]["my-mod"]=std::string(64,'c');if(!candidates(modPack,"Dedicated server").empty())throw std::runtime_error("Mod bundles cannot masquerade as the native base cache");
    auto malformed=nativePack;malformed["Sources"]["game"]="bad";if(!candidates(malformed,"Steam").empty())throw std::runtime_error("Invalid portable hashes cannot become candidates");
    const auto isolated=std::filesystem::absolute(destination/"isolated-local");std::filesystem::create_directories(isolated);if(!SetEnvironmentVariableW(L"LOCALAPPDATA",isolated.c_str()))throw std::runtime_error("Cannot isolate download state");
    const auto managed=HostServices::CacheDirectory()/"downloads";std::filesystem::create_directories(managed);
    const auto pack=managed/(std::string(64,'a')+".rsmk-cache"),userPack=destination/"keep-user-pack.rsmk-cache",unknown=managed/"keep-custom.rsmk-cache";
    {std::ofstream f(pack);f<<"verified-import-fixture";}{std::ofstream f(userPack);f<<"user";}{std::ofstream f(unknown);f<<"custom";}
    if(HelpyDownloads::ReleaseImportedPack(userPack)||HelpyDownloads::ReleaseImportedPack(unknown)||!std::filesystem::exists(userPack)||!std::filesystem::exists(unknown))throw std::runtime_error("Cleanup touched a user-owned download");
    if(!HelpyDownloads::ReleaseImportedPack(pack)||std::filesystem::exists(pack)||HelpyDownloads::ReleaseImportedPack(pack))throw std::runtime_error("Managed import staging cleanup failed");
    if(HelpyUiAssets::Files.size()!=31||!HelpyUiAssets::Known("power-abovezone.png")||!HelpyUiAssets::Known("power-severelybelowzone.png")||!HelpyUiAssets::Known("scale-null.png")||!HelpyUiAssets::Known("scale-hauler.png")||!HelpyUiAssets::Known("source-base-game.png")||!HelpyUiAssets::Known("source-colorfuldye.png")||!HelpyUiAssets::Known("RuneSchema.png")||HelpyUiAssets::Known("../RuneSchema.png")||HelpyUiAssets::Known("main.dll"))throw std::runtime_error("UI artwork whitelist must contain only fixed PNG names");
    if(HelpySplash::TipIndex(-1)!=0||HelpySplash::TipIndex(0)!=0||HelpySplash::TipIndex(7)!=1||HelpySplash::TipIndex(56)!=0)throw std::runtime_error("Splash tips must rotate predictably");
    HelpyDownloads::TestArtwork(source,destination);
    if(!std::filesystem::is_regular_file(destination/"ready")||HelpyDownloads::Completed()!=HelpyUiAssets::Files.size())throw std::runtime_error("Artwork install did not commit complete readiness");
    const auto stamp=std::filesystem::last_write_time(destination/"RuneSchema.png");
    HelpyDownloads::TestArtwork(source,destination);
    if(std::filesystem::last_write_time(destination/"RuneSchema.png")!=stamp)throw std::runtime_error("Unchanged artwork was rewritten");
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
}
