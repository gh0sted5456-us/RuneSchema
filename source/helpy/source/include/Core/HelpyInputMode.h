#pragma once
#include <string_view>
namespace PS::HelpyInputMode {
enum class Mode {Automatic,MouseKeyboard,Controller};
enum class Glyphs {Automatic,Xbox,PlayStation,Generic};
struct Selection {
    bool Controller=false;
    void Update(Mode mode,bool gamepad,bool keyboardMouse){if(mode==Mode::Controller)Controller=true;else if(mode==Mode::MouseKeyboard)Controller=false;else if(keyboardMouse)Controller=false;else if(gamepad)Controller=true;}
};
inline std::string_view Confirm(Glyphs glyphs){return glyphs==Glyphs::PlayStation?"Cross":glyphs==Glyphs::Generic?"South":"A";}
inline std::string_view Cancel(Glyphs glyphs){return glyphs==Glyphs::PlayStation?"Circle":glyphs==Glyphs::Generic?"East":"B";}
}
