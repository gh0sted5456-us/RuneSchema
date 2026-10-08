#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
static void Require(bool value,const char* what){if(!value)throw std::runtime_error(std::string("F2 quick-menu regression: ")+what);}

int main(int argc,char** argv) {
    if(argc!=4)throw std::runtime_error("Expected dllmain.cpp, InGameQuickMenu.cpp and ActorHelper files bundle");
    const auto dll=Read(argv[1]);const auto menu=Read(argv[2]);const auto actor=Read(argv[3]);
    Require(dll.find("register_keydown_event(Input::Key::F2")!=std::string::npos,"F2 binding missing");
    Require(dll.find("Input::Key::LEFT_ARROW")!=std::string::npos && dll.find("Input::Key::RIGHT_ARROW")!=std::string::npos,
        "tab navigation key bindings missing");
    Require(dll.find("m_quickMenu.Initialize()")!=std::string::npos && dll.find("m_quickMenu.Shutdown()")!=std::string::npos,
        "quick-menu lifecycle missing");
    Require(menu.find("GameViewportClient::PostRender")!=std::string::npos
        && menu.find("UGameViewportClient::VTableLayoutMap")!=std::string::npos,
        "GameViewportClient PostRender canvas hook missing");
    Require(menu.find("/Script/Engine.HUD:ReceiveDrawHUD")==std::string::npos,
        "obsolete HUD ReceiveDrawHUD dependency returned");
    Require(menu.find("/Script/Engine.Canvas:K2_DrawTexture")!=std::string::npos
        && menu.find("/Script/Engine.Canvas:K2_DrawText")!=std::string::npos,
        "direct UCanvas drawing functions missing");
    Require(menu.find("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")!=std::string::npos,
        "stable engine white texture is not used for menu rectangles");
    Require(menu.find("/Engine/EngineFonts/Roboto.Roboto")!=std::string::npos
        && menu.find("if (!canvas || !function || !font) return;")!=std::string::npos,
        "menu text does not fail closed around a real engine font");
    Require(menu.find("AI\", \"Resources\", \"Items\", \"Props")!=std::string::npos,"four quick-menu tabs missing");
    Require(menu.find("QuickMenuColumns = 5")!=std::string::npos && menu.find("QuickMenuRows = 5")!=std::string::npos,
        "5x5 quick-menu grid target missing");
    Require(menu.find("bIgnoreInput")!=std::string::npos
        && menu.find("m_previousIgnoreInput")!=std::string::npos
        && menu.find("SetIgnoreLookInput")!=std::string::npos
        && menu.find("SetIgnoreMoveInput")!=std::string::npos
        && menu.find("bShowMouseCursor")!=std::string::npos
        && menu.find("ReleaseViewportInput")!=std::string::npos,
        "menu no longer captures viewport + controller focus, camera and movement input");
    Require(menu.find("toggle #")==std::string::npos
        && menu.find("first GameViewportClient PostRender canvas observed")==std::string::npos
        && menu.find("first visible PostRender canvas draw accepted")==std::string::npos,
        "attempt/toggle enumeration diagnostics returned");
    Require(menu.find("render paused after error; it will retry")!=std::string::npos
        && menu.find("closed after render error")==std::string::npos,
        "transient render errors still auto-close the overlay");
    Require(menu.find("SpawnToolRequests::Read()")!=std::string::npos
        && menu.find("{\"Action\", \"Catalog\"}")!=std::string::npos
        && menu.find("item.value(\"Icon\"")!=std::string::npos
        && menu.find("DrawTexture(canvas, icon")!=std::string::npos,
        "in-game Items tab is not wired to the saved item catalog and native icon textures");
    Require(menu.find("std::format(\"{}\", slot)")==std::string::npos,
        "per-cell slot-number text returned to the render loop");
    Require(actor.find("FunctionCall& FunctionCall::JsonArg")!=std::string::npos,
        "reflected safe parameter writer missing");
    std::cout<<"PASS: F2 quick-menu shell uses PostRender, fixed 5x5 layout, controller focus/camera capture, retry-safe rendering and native item icons.\n";
}
