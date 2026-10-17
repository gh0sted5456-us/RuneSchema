#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace PS::HelpyBranding {
inline constexpr char Name[]="RSMK";
inline constexpr char FullName[]="RuneSchema Modding Kit";
inline constexpr wchar_t WindowTitle[]=L"RSMK (RuneSchema Modding Kit)";
inline constexpr char Version[]="0.8.0.0";
inline constexpr char Credits[]="RuneSchema Modding Kit (RSMK) by the RuneSchema Community Team, NuLL and Jonesing.\n\nRuneSchema builds on the original work by Snorkles and the PalSchema foundation by Okaetsu.\n\nInterface: Dear ImGui. Data: nlohmann/json. Original copyright and component notices are included with RuneSchema.";
inline constexpr char Legal[]="RSMK shares RuneSchema's MIT License.\nCopyright (c) 2026 RSDW Modding Community\n\nPermission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\nThird-party components and artwork retain their respective licenses and ownership. Steam and the Steam logo belong to Valve; Epic Games and its logo belong to Epic Games; Xbox and the Xbox logo belong to Microsoft. RuneScape: Dragonwilds belongs to Jagex. Platform marks identify the detected runtime only; no endorsement or affiliation is implied.";
// Uploaded monochrome marks can have an opaque black background. Convert only
// their upload buffer to a white alpha mask; never alter the original asset.
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
    const bool gold=central&&rgba[0]>=rgba[1]&&rgba[0]>unsigned(rgba[2])+10
        &&rgba[1]>unsigned(rgba[2])+6;
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
