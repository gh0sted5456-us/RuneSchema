#include "Core/HelpyPopupInput.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static void Require(bool value){if(!value)throw std::runtime_error("Helpy popup input regression");}
static std::string Read(const char* path){std::ifstream input(path);Require(input.good());return {std::istreambuf_iterator<char>(input),{}};}
int main(int argc,char** argv){
    using namespace PS::HelpyPopupInput;
    for(unsigned flags=0;flags<16;++flags)Require(Route(flags&1,flags&2,flags&4,flags&8)==(flags==15));
    HotkeyLatch latch;Require(latch.Poll(true));Require(!latch.Poll(true));Require(!latch.Press());
    Require(!latch.Poll(false));Require(latch.Press());Require(!latch.Poll(true));
    latch.Release();Require(latch.Poll(true));
    Require(argc==2);const auto source=Read(argv[1]);
    Require(source.find("WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE")!=std::string::npos);
    Require(source.find("WM_MOUSEACTIVATE)return MA_NOACTIVATE")!=std::string::npos);
    Require(source.find("SetWindowsHookExW(WH_GETMESSAGE,PopupInputHook,nullptr,thread)")!=std::string::npos);
    Require(source.find("pid!=GetCurrentProcessId()")!=std::string::npos);
    Require(source.find("SetForegroundWindow(")==std::string::npos);
    Require(source.find("WH_KEYBOARD_LL")==std::string::npos);
    Require(source.find("ReleaseInputRoute();")!=std::string::npos);
    Require(source.find("ApplyUiModifierSnapshot(mods)")!=std::string::npos&&source.find("SetKeyboardState(state)")!=std::string::npos);
    Require(source.find("!self->m_visible.load()||GetForegroundWindow()!=self->m_gameWindow")!=std::string::npos);
    Require(source.find("if(m_visible.load()&&GetForegroundWindow()!=m_gameWindow)ClosePopup()")!=std::string::npos);
}
