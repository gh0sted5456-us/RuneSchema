#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
namespace PS::RuneSchemaUpdateInfo {
inline constexpr char MainUrl[]="https://github.com/gh0sted5456-us/RuneSchema/tree/main";
inline constexpr char ReleasesUrl[]="https://github.com/gh0sted5456-us/RuneSchema/releases/latest";
inline constexpr char NexusUrl[]="https://www.nexusmods.com/runescapedragonwilds/mods/570";
struct Version {std::array<unsigned,4> Parts{};std::string Serial,Suffix;};
inline std::optional<Version> Parse(std::string_view text) {
    if(text.empty()||text.size()>96)return {};
    if(text.starts_with("RuneSchema-"))text.remove_prefix(11);
    if(text.starts_with('v'))text.remove_prefix(1);
    if(text.starts_with("1.0.0.J0")) {
        const auto serial=text.substr(6,6);
        if(serial.size()!=6||serial.substr(2).find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=text.npos)return {};
        text.remove_prefix(12);
        const auto suffix=text.starts_with('-')?text.substr(1):text;
        if(!text.empty()&&(suffix.size()!=2||(suffix[0]!='h'&&suffix[0]!='H')||(suffix[1]!='f'&&suffix[1]!='F')))return {};
        return Version{{1,0,0,0},std::string(serial),std::string(text)};
    }
    Version version;unsigned count=0;
    while(!text.empty()&&text.front()>='0'&&text.front()<='9') {
        if(count==4)return {};
        const auto end=text.find_first_not_of("0123456789");const auto number=text.substr(0,end);
        auto result=std::from_chars(number.data(),number.data()+number.size(),version.Parts[count++]);
        if(result.ec!=std::errc{}||version.Parts[count-1]>1000000)return {};
        if(end==text.npos){text={};break;}text.remove_prefix(end);
        if(text.front()!='.')break;
        text.remove_prefix(1);if(text.empty()||text.front()<'0'||text.front()>'9')return {};
    }
    if(count<2)return {};
    // Ignore semver build metadata. Legacy m/e build labels remain distinct,
    // rather than inventing an ordering for RuneSchema's historical flavors.
    if(const auto metadata=text.find('+');metadata!=text.npos)text=text.substr(0,metadata);
    for(unsigned char ch:text)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='.'))return {};
    version.Suffix=std::string(text);return version;
}
inline constexpr wchar_t NexusFilesUrl[]=L"https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=files";
inline constexpr wchar_t CurseForgeFilesUrl[]=L"https://www.curseforge.com/runescape-dragonwilds/ue4ss-mods/runeschema/files";
inline constexpr wchar_t GitHubLatestUrl[]=L"https://github.com/gh0sted5456-us/RuneSchema/releases/latest";
inline bool IsStableReleaseTag(std::string_view tag){
    const auto version=Parse(tag);
    return version&&(version->Suffix.empty()||version->Suffix=="m"||version->Suffix=="b");
}
enum class Comparison { Unknown, Current, Newer, Ahead, DifferentBuild };
inline std::optional<int> ComparePrerelease(std::string_view left,std::string_view right) {
    left.remove_prefix(1);right.remove_prefix(1);
    for(;;) {
        const auto le=left.find('.'),re=right.find('.');const auto a=left.substr(0,le),b=right.substr(0,re);
        if(a.empty()||b.empty())return {};
        const bool an=a.find_first_not_of("0123456789")==a.npos,bn=b.find_first_not_of("0123456789")==b.npos;
        if(a!=b){
            if(an&&bn){if(a.size()!=b.size())return a.size()<b.size()?-1:1;return a<b?-1:1;}
            if(an!=bn)return an?-1:1;return a<b?-1:1;
        }
        if(le==left.npos||re==right.npos)return le==left.npos?(re==right.npos?0:-1):1;
        left.remove_prefix(le+1);right.remove_prefix(re+1);
    }
}
inline Comparison Compare(std::string_view installed,std::string_view remote) {
    const auto local=Parse(installed),latest=Parse(remote);if(!local||!latest)return Comparison::Unknown;
    if(local->Parts<latest->Parts)return Comparison::Newer;
    if(local->Parts>latest->Parts)return Comparison::Ahead;
    if(local->Serial<latest->Serial)return Comparison::Newer;
    if(local->Serial>latest->Serial)return Comparison::Ahead;
    if(local->Suffix==latest->Suffix)return Comparison::Current;
    if(local->Suffix.empty()&&latest->Suffix=="b")return Comparison::Newer;
    if(local->Suffix=="b"&&latest->Suffix.empty())return Comparison::Ahead;
    if(local->Suffix.starts_with('-')&&latest->Suffix.empty())return Comparison::Newer;
    if(local->Suffix.empty()&&latest->Suffix.starts_with('-'))return Comparison::Ahead;
    if(local->Suffix.starts_with('-')&&latest->Suffix.starts_with('-')) {
        const auto order=ComparePrerelease(local->Suffix,latest->Suffix);
        if(!order)return Comparison::Unknown;
        return *order<0?Comparison::Newer:*order>0?Comparison::Ahead:Comparison::Current;
    }
    return Comparison::DifferentBuild;
}
inline std::optional<std::string> MainVersion(std::string_view source) {
    if(source.size()>65536)return {};
    const auto at=source.find("char Version[]");if(at==source.npos)return {};
    auto declaration=source.substr(at);const auto end=declaration.find(';');if(end==declaration.npos||end>160)return {};
    declaration=declaration.substr(0,end);const auto begin=declaration.find('"');if(begin==declaration.npos)return {};
    declaration.remove_prefix(begin+1);const auto quote=declaration.find('"');if(quote==declaration.npos)return {};
    const auto version=declaration.substr(0,quote);if(!Parse(version))return {};return std::string(version);
}
inline const char* Label(Comparison status) {
    switch(status){case Comparison::Current:return "Up to date";case Comparison::Newer:return "Update available";
    case Comparison::Ahead:return "Installed version is ahead";case Comparison::DifferentBuild:return "Build label differs; review source";
    default:return "Could not compare versions";}
}
}
