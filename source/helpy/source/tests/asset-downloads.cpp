#include "Runtime/HelpyDownloads.h"
#include "Core/HelpyUiAssets.h"
#include "Core/HelpySplash.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
int main(int argc,char** argv){
    if(argc!=3)throw std::runtime_error("Asset source and isolated destination required");
    const std::filesystem::path source=argv[1],destination=argv[2];
    using namespace PS;
    if(HelpyUiAssets::Files.size()!=22||!HelpyUiAssets::Known("source-base-game.png")||!HelpyUiAssets::Known("source-colorfuldye.png")||!HelpyUiAssets::Known("RuneSchema.png")||HelpyUiAssets::Known("../RuneSchema.png")||HelpyUiAssets::Known("main.dll"))throw std::runtime_error("UI artwork whitelist must contain only fixed PNG names");
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
