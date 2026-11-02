#include "Core/HelpyActivity.h"
#include <stdexcept>
void Need(bool value){if(!value)throw std::runtime_error("Helpy hidden-work regression");}
int main(){
    using namespace PS::HelpyActivity;
    Need(!Visible.load()&&!BackgroundAllowed());
    Need(BackgroundAllowed(true));
    Visible=true;Need(BackgroundAllowed()&&BackgroundAllowed(true));
    Visible=false;Need(!BackgroundAllowed()&&BackgroundAllowed(true));
    CaptureInput=true;Need(!WantsInputCapture());Visible=true;Need(WantsInputCapture());CaptureInput=false;Need(!WantsInputCapture());
    InputCapture capture;int move=3,look=2;
    const auto apply=[&](bool moving,bool enabled){(moving?move:look)+=enabled?1:-1;};
    capture.Update(true,apply);capture.Update(true,apply);Need(move==4&&look==3);
    Visible=false;capture.Update(WantsInputCapture(),apply);Need(move==3&&look==2&&!capture.Applied());
    bool failed=false;try{capture.Update(true,[&](bool moving,bool enabled){if(!moving)throw std::runtime_error("look unavailable");apply(moving,enabled);});}catch(...){failed=true;}
    Need(failed&&capture.Move&&!capture.Look&&move==4&&look==2);
    capture.Update(true,apply);Need(move==4&&look==3);capture.Update(false,apply);Need(move==3&&look==2);
}
