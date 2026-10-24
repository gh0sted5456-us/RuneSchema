#pragma once
#include <string>
#include <utility>
namespace PS {
struct NativeInventoryMarkClick {
    bool Down=true;
    std::string Pressed;
    void Reset(){Down=true;Pressed.clear();}
    std::string Step(bool down,const std::string& hovered){
        std::string clicked;
        if(down&&!Down)Pressed=hovered;
        else if(down&&Pressed!=hovered)Pressed.clear();
        else if(!down&&Down){if(!Pressed.empty()&&Pressed==hovered)clicked=std::move(Pressed);Pressed.clear();}
        Down=down;return clicked;
    }
};
}
