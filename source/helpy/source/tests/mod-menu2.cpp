#include "Core/ModMenuGate.h"
#include "HelpyHudLayout.h"
#include "Core/HelpyMapMotion.h"
#include "Core/ModMenuLicense.h"
#include <stdexcept>
#include <cstdio>
static void Check(bool value){if(!value)throw std::runtime_error("Mod Menu 2 gate / HUD regression");}
int main(int argc,char** argv){namespace fs=std::filesystem;using namespace PS;
    if(argc==3&&std::string_view(argv[1])=="--read-runtime"){const auto root=RuntimeModPath::Resolve(fs::u8path(argv[2]));const auto blocked=ModMenuGate::Blocked(fs::u8path(argv[2]));std::puts(nlohmann::json{{"Root",RuntimeModPath::Text(root)},{"Blocked",blocked},{"ReadOnly",true}}.dump().c_str());return blocked.empty()?0:2;}
    Check(argc==3);const auto image=LoadLibraryExW(fs::u8path(argv[2]).c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);Check(image!=nullptr);
    Check(ModMenuLicense::Text(image)==ConfigFiles::Read(fs::u8path(argv[1])));FreeLibrary(image);
    const auto root=fs::temp_directory_path()/("mod-menu2-"+std::to_string(GetCurrentProcessId()))/"Mods/RuneSchema";fs::create_directories(root);
    try{const auto license=ModMenu2Key::Normalize(ConfigFiles::Read(fs::u8path(argv[1]))),key=std::string(ModMenu2Key::Hash)+"\n\n"+license+"\n";Check(ModMenu2Key::ValidText(key));Check(!ModMenu2Key::ValidText("bad\n\n"+license));Check(!ModMenu2Key::ValidText(std::string(ModMenu2Key::Hash)+"\n\nwrong license"));std::string crlf="\xef\xbb\xbf";for(char c:key){if(c=='\n')crlf+='\r';crlf+=c;}Check(ModMenu2Key::ValidText(crlf));
        Check(ModMenuGate::Blocked(root).find("locked")!=std::string::npos);ConfigFiles::Write(root/"plugins/Helpy/modmenu2.key",key);Check(ModMenuGate::Blocked(root).find("native")!=std::string::npos||ModMenuGate::Blocked(root).find("Native")!=std::string::npos);
        ConfigFiles::Write(root/"Helpy/plugins/RuneSchema.ModMenu2/plugin.json",R"({"Id":"RuneSchema.ModMenu2","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.ModMenu2.dll"})");ConfigFiles::Write(root/"Helpy/plugins/RuneSchema.ModMenu2/dll/RuneSchema.ModMenu2.dll","fixture");Check(ModMenuGate::Blocked(root).empty());
        const auto external=root.parent_path()/"ModMenu";ConfigFiles::Write(external/"Scripts/main.lua","return");ConfigFiles::Write(root.parent_path()/"mods.txt","ModMenu : 0\n");Check(!ModMenuGate::ExternalEnabled(root));ConfigFiles::Write(external/"enabled.txt","");Check(ModMenuGate::ExternalEnabled(root)&&!ModMenuGate::Blocked(root).empty());fs::remove(external/"enabled.txt");ConfigFiles::Write(root.parent_path()/"mods.txt","ModMenu : 1\n");Check(ModMenuGate::ExternalEnabled(root));ConfigFiles::Write(root.parent_path()/"mods.txt","ModMenu : 0\n");Check(ModMenuGate::Blocked(root).empty());
        ModMenu2Settings::Settings settings;settings.Enabled=false;settings.Hotkey=VK_F8;ModMenu2Settings::Save(root,settings);Check(!ModMenu2Settings::Read(root).Enabled&&ModMenu2Settings::Read(root).Hotkey==VK_F8&&!ModMenuGate::Blocked(root).empty());
        fs::remove(root/"plugins/Helpy/modmenu2.key");HelpyHudLayout::Settings hud;hud.Minimap=true;HelpyHudLayout::Save(root,hud);Check(HelpyHudLayout::Read(root).Minimap&&!ModMenu2Key::Present(root));
        hud.FollowPlayer=false;hud.MapPositionLocked=true;hud.MapZoom=12;hud.MapFocus={.2f,.7f};HelpyHudLayout::Save(root,hud);const auto saved=HelpyHudLayout::Read(root);Check(!saved.FollowPlayer&&saved.MapPositionLocked&&saved.MapZoom==12&&saved.MapFocus.X==.2f&&saved.MapFocus.Y==.7f);
        const auto map=HelpyHudLayout::Map(1920,1080,hud),coords=HelpyHudLayout::Coordinates(1920,1080,700,32,hud);Check(map.X>1500&&map.Y==16&&coords.Y==map.Y+map.H+8&&coords.X==map.X&&coords.W==map.W);for(float width:{160.f,760.f,1920.f}){const auto box=HelpyHudLayout::Coordinates(width,120,700,32,hud);Check(box.X>=0&&box.Y>=0&&box.X+box.W<=width&&box.Y+box.H<=120);}
        hud.Map.Y=1;const auto lowerMap=HelpyHudLayout::Map(760,560,hud),lowerLabel=HelpyHudLayout::Coordinates(760,560,700,56,hud);Check(lowerLabel.Y>=lowerMap.Y+lowerMap.H&&lowerLabel.Y+lowerLabel.H<=560);
        Check(HelpyHudLayout::CoordinateText("X 1   Y 2   Z 3   |   Pitch 4   Yaw 5",hud)=="X 1   Y 2   Z 3\nPitch 4   Yaw 5");
        HelpyMapMotion::Smoother motion;motion.Update({0,0,350},1);motion.Update({100,200,10},1.1);auto middle=motion.At(1.15);Check(std::abs(middle.X-50)<.001&&std::abs(middle.Y-100)<.001&&std::abs(std::remainder(middle.Yaw,360.))<.001);Check(motion.At(1.3).X==100);motion.Update({500000,0,90},1.4);Check(motion.At(1.4).X==500000);motion.Reset();motion.Update({50,50,45},1.5);Check(motion.At(1.5).X==50);
        const auto moved=HelpyHudLayout::Drag(1920,1080,280,280,400,300);const auto box=HelpyHudLayout::Place(1920,1080,280,280,moved);Check(std::abs(box.X-400)<.01f&&std::abs(box.Y-300)<.01f);
    }catch(...){fs::remove_all(root.parent_path().parent_path());throw;}fs::remove_all(root.parent_path().parent_path());
}
