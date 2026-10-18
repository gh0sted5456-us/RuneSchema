#include "Core/HelpyPopupInput.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static void Require(bool value){if(!value)throw std::runtime_error("Helpy popup input regression");}
static std::string Read(const char* path){std::ifstream input(path);Require(input.good());return {std::istreambuf_iterator<char>(input),{}};}
int main(int argc,char** argv){
    using namespace PS::HelpyPopupInput;
    for(unsigned flags=0;flags<4;++flags){
        Require(CanToggle(flags&1,flags&2)==(flags!=0));
        Require(ReturnToGame(flags&1,flags&2)==(flags==3));
    }
    HotkeyLatch latch;Require(latch.Poll(true));Require(!latch.Poll(true));Require(!latch.Press());
    Require(!latch.Poll(false));Require(latch.Press());Require(!latch.Poll(true));
    latch.Release();Require(latch.Poll(true));
    Require(argc==2);const auto source=Read(argv[1]);
    Require(source.find("CreateWindowExW(WS_EX_APPWINDOW")!=std::string::npos);
    Require(source.find("WS_OVERLAPPEDWINDOW")!=std::string::npos);
    Require(source.find("WS_EX_NOACTIVATE")==std::string::npos);
    Require(source.find("WS_EX_TOPMOST")==std::string::npos);
    Require(source.find("SetWindowsHookExW(")==std::string::npos);
    Require(source.find("GWLP_HWNDPARENT")==std::string::npos);
    Require(source.find("SetForegroundWindow(m_hwnd)")!=std::string::npos);
    Require(source.find("if(returnToGame)SetForegroundWindow(m_gameWindow)")!=std::string::npos);
    Require(source.find("WH_KEYBOARD_LL")==std::string::npos);
    Require(source.find("ClipCursor(")==std::string::npos&&source.find("SetKeyboardState(")==std::string::npos);
    Require(source.find("if(Hotkey.Press())ClosePopup()")!=std::string::npos);
    Require(source.find("CanToggle(GameWindow(foreground),foreground==m_publishedWindow.load())")!=std::string::npos);
    Require(source.find("if(m_visible.load()&&GetForegroundWindow()!=m_gameWindow)ClosePopup()")==std::string::npos);
}
