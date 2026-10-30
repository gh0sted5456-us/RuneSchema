#include "Core/HelpyActivity.h"
#include <stdexcept>
void Need(bool value){if(!value)throw std::runtime_error("Helpy hidden-work regression");}
int main(){
    using namespace PS::HelpyActivity;
    Need(!Visible.load()&&!BackgroundAllowed());
    Need(BackgroundAllowed(true));
    Visible=true;Need(BackgroundAllowed()&&BackgroundAllowed(true));
    Visible=false;Need(!BackgroundAllowed()&&BackgroundAllowed(true));
}
