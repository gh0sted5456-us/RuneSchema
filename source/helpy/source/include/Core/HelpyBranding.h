#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PS::HelpyBranding {
inline constexpr char Name[]="RSMK";
inline constexpr char FullName[]="RuneSchema Modding Kit";
// Retained for task switching and accessibility; the image caption draws only the icon.
inline constexpr wchar_t WindowTitle[]=L"RuneSchema";
inline constexpr char Version[]="0.8.0.1b";
inline constexpr float RuntimeIconSize=36.f;
// Original 1920x1080 Resources/SplashScreen.png, never upscaled from an icon.
// Shared crop preserves its aspect ratio and the gap between the two lines.
struct GameWordmark {
    static constexpr unsigned Width=1920,Height=1080;
    static constexpr float Left=202.f/Width,Top=352.f/Height,
        Right=1726.f/Width,Bottom=730.f/Height,Split=480.f/Height;
    static constexpr float Aspect=378.f/1524.f;
};
inline constexpr char Credits[]="RuneSchema Modding Kit (RSMK) by the RuneSchema Community Team, NuLL and Jonesing.\n\nRuneSchema builds on the original work by Snorkles and the PalSchema foundation by Okaetsu.\n\nAbyss (RSDW Modding Community): initial corrupt-character bypass work on which RuneSchema's corrupt-save bypass was based.\n\nInterface: Dear ImGui. Data: nlohmann/json. Original copyright and component notices are included with RuneSchema.";
inline constexpr char Legal[]="RSMK shares RuneSchema's MIT License.\nCopyright (c) 2026 RSDW Modding Community\n\nPermission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\nThird-party components and artwork retain their respective licenses and ownership. Steam and the Steam logo belong to Valve; Epic Games and its logo belong to Epic Games; Xbox and the Xbox logo belong to Microsoft. RuneScape: Dragonwilds belongs to Jagex. Platform marks identify the detected runtime only; no endorsement or affiliation is implied.";
// Uploaded monochrome marks can have an opaque black background. Convert only
// their upload buffer to a white alpha mask; never alter the original asset.
inline void CommunityLayer(std::uint8_t* rgba,bool accent){
    // Nexus's orange is a separate neutral alpha mask. Tinting is performed
    // by the draw call, so changing themes never decodes or uploads it again.
    const int chroma=std::max(0,int(rgba[0])-int(rgba[2]));
    const unsigned coverage=std::clamp((chroma-12)*255/48,0,255);
    const unsigned layer=accent?coverage:255-coverage;
    const unsigned light=std::max({rgba[0],rgba[1],rgba[2]});
    rgba[3]=static_cast<std::uint8_t>((unsigned(rgba[3])*layer*light+32512)/65025);
    rgba[0]=rgba[1]=rgba[2]=255;
}
// Trim transparent/black padding after mask conversion, contain-fit each mark
// on the same square canvas. Originals on disk remain untouched.
inline void NormalizeMark(unsigned& width,unsigned& height,std::vector<std::uint8_t>& pixels){
    if(!width||!height||pixels.size()!=size_t(width)*height*4)return;
    unsigned left=width,top=height,right=0,bottom=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)if(pixels[(size_t(y)*width+x)*4+3]>16){left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1);}
    if(right<=left||bottom<=top)return;
    constexpr unsigned side=96,content=88;std::vector<std::uint8_t> result(side*side*4);
    const double scale=double(content)/std::max(right-left,bottom-top);const unsigned w=std::max(1u,unsigned((right-left)*scale)),h=std::max(1u,unsigned((bottom-top)*scale));
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const unsigned sx=left+std::min(right-left-1,unsigned(x/scale)),sy=top+std::min(bottom-top-1,unsigned(y/scale));
        std::copy_n(pixels.data()+(size_t(sy)*width+sx)*4,4,result.data()+((size_t(y+(side-h)/2)*side+x+(side-w)/2)*4));}
    width=height=side;pixels=std::move(result);
}
inline void MonochromeMask(std::uint8_t* rgba,bool darkForeground=false) {
    const unsigned luminance=(54u*rgba[0]+183u*rgba[1]+19u*rgba[2]+128u)/256u;
    const unsigned coverage=darkForeground?255u-luminance:luminance;
    rgba[3]=static_cast<std::uint8_t>((coverage*rgba[3]+127u)/255u);
    rgba[0]=rgba[1]=rgba[2]=255;
}
// Split only the central gold ornament into a neutral, tintable upload layer.
// Silver brackets, black edging and highlights remain in the original layer.
// The supplied PNG and the Windows application icon are never rewritten.
inline void LogoLayer(std::uint8_t* rgba,unsigned x,unsigned width,bool accent) {
    const bool central=width&&x>=width/4&&x<width*4/5;
    const bool gold=central&&rgba[0]>=rgba[1]&&rgba[1]>=rgba[2]
        &&(rgba[0]>unsigned(rgba[2])+6||rgba[1]>unsigned(rgba[2])+3);
    if(accent){if(!gold){rgba[3]=0;return;}rgba[1]=rgba[2]=rgba[0];}
    else if(gold)rgba[3]=0;
}
inline std::string BuildLabel(std::string_view version) {
    if(version.empty())return "Not reported";
    const auto marker=version.find("-CL-");
    if(marker!=std::string_view::npos) {
        const auto begin=marker+4,end=version.find_first_not_of("0123456789",begin);
        if(begin<version.size()&&end!=begin)return "CL "+std::string(version.substr(begin,end==std::string_view::npos?end:end-begin));
    }
    return std::string(version);
}
}
