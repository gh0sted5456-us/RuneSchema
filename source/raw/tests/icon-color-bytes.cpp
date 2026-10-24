#include "Core/IconColorBytes.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Icon color byte layout regression");}
int main(){
    const std::array<unsigned char,12> samples{3,2,1,4,99,99,7,6,5,8,99,99};
    const auto rgba=PS::IconCapture::ColorBytes(samples,2,6,{2,1,0,3});
    Check(rgba&&*rgba==std::vector<unsigned char>({1,2,3,4,5,6,7,8}));
    Check(!PS::IconCapture::ColorBytes(samples,3,6,{2,1,0,3}));
    Check(!PS::IconCapture::ColorBytes(samples,1,0,{2,1,0,3}));
    Check(!PS::IconCapture::ColorBytes(samples,1,65,{2,1,0,3}));
    Check(!PS::IconCapture::ColorBytes(samples,1,6,{-1,1,0,3}));
    Check(!PS::IconCapture::ColorBytes(samples,1,6,{6,1,0,3}));
    Check(!PS::IconCapture::ColorBytes(samples,1,6,{2,1,1,3}));
    Check(!PS::IconCapture::ColorBytes(samples,4097,1,{0,1,2,3}));
    Check(!PS::IconCapture::ColorBytes(samples,0,6,{2,1,0,3}));
}
