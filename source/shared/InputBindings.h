#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace PS::InputBindings {
using Keys=std::vector<uint16_t>;
struct NamedKey {std::string_view Name;uint16_t Code;};
inline constexpr NamedKey SpecialKeys[]{
    {"Ctrl",0x11},{"Alt",0x12},{"Shift",0x10},{"Esc",0x1b},{"Tab",9},{"Enter",13},{"Space",32},{"Backspace",8},
    {"CapsLock",0x14},{"Insert",0x2d},{"Delete",0x2e},{"Home",0x24},{"End",0x23},{"PageUp",0x21},{"PageDown",0x22},
    {"Left",0x25},{"Up",0x26},{"Right",0x27},{"Down",0x28},{"PrintScreen",0x2c},{"ScrollLock",0x91},{"Pause",0x13},
    {"Backtick",0xc0},{"Minus",0xbd},{"Equals",0xbb},{"LeftBracket",0xdb},{"RightBracket",0xdd},{"Backslash",0xdc},
    {"Semicolon",0xba},{"Apostrophe",0xde},{"Comma",0xbc},{"Period",0xbe},{"Slash",0xbf},
    {"NumLock",0x90},{"NumDivide",0x6f},{"NumMultiply",0x6a},{"NumSubtract",0x6d},{"NumAdd",0x6b},{"NumDecimal",0x6e}
};
inline uint16_t Normalize(uint16_t key){if(key==0xa0||key==0xa1)return 0x10;if(key==0xa2||key==0xa3)return 0x11;if(key==0xa4||key==0xa5)return 0x12;return key;}
inline bool Modifier(uint16_t key){return key==0x10||key==0x11||key==0x12;}
inline std::string Fold(std::string_view value){std::string result;for(unsigned char c:value)if(c!=' '&&c!='_'&&c!='-')result+=char(c>='a'&&c<='z'?c-'a'+'A':c);return result;}
inline std::string Name(uint16_t key){
    key=Normalize(key);for(const auto& item:SpecialKeys)if(item.Code==key)return std::string(item.Name);
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return std::string(1,char(key));
    if(key>=0x70&&key<=0x87)return "F"+std::to_string(key-0x6f);
    if(key>=0x60&&key<=0x69)return "Num"+std::to_string(key-0x60);
    return {};
}
inline std::optional<uint16_t> Code(std::string_view name){
    const auto value=Fold(name);for(uint16_t key=1;key<256;++key)if(!Name(key).empty()&&Fold(Name(key))==value)return Normalize(key);
    if(value=="CONTROL"||value=="LCTRL"||value=="RCTRL")return 0x11;
    if(value=="LSHIFT"||value=="RSHIFT")return 0x10;if(value=="LALT"||value=="RALT")return 0x12;
    if(value=="ESCAPE")return 0x1b;if(value=="RETURN")return 13;
    return {};
}
inline std::vector<std::string> Tokens(std::string_view value){
    std::vector<std::string> result;if(value.empty()||Fold(value)=="NONE"||Fold(value)=="DISABLED")return result;
    if(value.size()>192)throw std::runtime_error("The binding is too long.");
    size_t begin=0;for(;;){const auto end=value.find('+',begin);auto token=std::string(value.substr(begin,end==value.npos?value.size()-begin:end-begin));if(Fold(token).empty())throw std::runtime_error("Choose a key for each part of the combination.");result.push_back(std::move(token));if(end==value.npos)break;begin=end+1;}
    return result;
}
inline Keys Validate(Keys keys){
    for(auto& key:keys){key=Normalize(key);if(Name(key).empty())throw std::runtime_error("This key cannot be used for a HUD shortcut.");}
    std::sort(keys.begin(),keys.end(),[](auto a,auto b){const auto rank=[](auto key){return key==0x11?0:key==0x10?1:key==0x12?2:1000+int(key);};return rank(a)<rank(b);});
    if(std::adjacent_find(keys.begin(),keys.end())!=keys.end())throw std::runtime_error("A key is selected more than once.");
    if(keys.size()>6)throw std::runtime_error("Choose at most six keys for one combination.");
    if(!keys.empty()&&std::all_of(keys.begin(),keys.end(),Modifier))throw std::runtime_error("Add a regular key to the modifier keys.");
    const auto has=[&](uint16_t key){return std::find(keys.begin(),keys.end(),key)!=keys.end();};
    if((has(0x12)&&(has(9)||has(0x73)))||(has(0x11)&&has(0x1b))||(has(0x11)&&has(0x12)&&has(0x2e)))throw std::runtime_error("That combination is reserved by Windows.");
    return keys;
}
inline Keys Parse(std::string_view value){Keys result;for(const auto& token:Tokens(value)){const auto key=Code(token);if(!key)throw std::runtime_error("Unknown keyboard key: "+token);result.push_back(*key);}return Validate(std::move(result));}
inline std::string Text(const Keys& keys){std::string text;for(auto key:keys){if(!text.empty())text+='+';text+=Name(key);}return text.empty()?"None":text;}
inline uint16_t Primary(const Keys& keys){for(auto i=keys.rbegin();i!=keys.rend();++i)if(!Modifier(*i))return *i;return 0;}
template<class Read>bool Down(const Keys& keys,Read read){
    if(keys.empty()||read(0x5b)||read(0x5c))return false;
    for(auto key:keys)if(!read(key))return false;
    for(uint16_t key:{0x10,0x11,0x12})if(read(key)!=(std::find(keys.begin(),keys.end(),key)!=keys.end()))return false;
    return true;
}
inline bool Overlap(const Keys& a,const Keys& b){
    if(a.empty()||b.empty())return false;
    for(uint16_t key:{0x10,0x11,0x12})if((std::find(a.begin(),a.end(),key)!=a.end())!=(std::find(b.begin(),b.end(),key)!=b.end()))return false;
    const auto subset=[](const Keys& x,const Keys& y){return std::all_of(x.begin(),x.end(),[&](auto key){return std::find(y.begin(),y.end(),key)!=y.end();});};return subset(a,b)||subset(b,a);
}
struct PadButton{std::string_view Name;uint32_t Bit;};
inline constexpr uint32_t Guide=0x10000,LeftTrigger=0x20000,RightTrigger=0x40000,Touchpad=0x80000;
inline constexpr PadButton PadButtons[]{
    {"Up",1},{"Down",2},{"Left",4},{"Right",8},{"Menu",0x10},{"View",0x20},{"L3",0x40},{"R3",0x80},
    {"LB",0x100},{"RB",0x200},{"South",0x1000},{"East",0x2000},{"West",0x4000},{"North",0x8000},
    {"Guide",Guide},{"LT",LeftTrigger},{"RT",RightTrigger},{"Touchpad",Touchpad}
};
inline uint32_t ParsePad(std::string_view value){
    uint32_t result=0;for(const auto& token:Tokens(value)){const auto found=std::find_if(std::begin(PadButtons),std::end(PadButtons),[&](const auto& key){return Fold(key.Name)==Fold(token);});if(found==std::end(PadButtons))throw std::runtime_error("Unknown controller button: "+token);if(result&found->Bit)throw std::runtime_error("A controller button is selected more than once.");result|=found->Bit;}return result;
}
inline std::string PadText(uint32_t mask){
    std::string result;for(const auto& key:PadButtons)if(mask&key.Bit){if(!result.empty())result+='+';result+=key.Name;mask&=~key.Bit;}if(mask)throw std::runtime_error("Unknown controller binding bits.");return result.empty()?"None":result;
}
inline bool Overlap(uint32_t a,uint32_t b){return a&&b&&((a&b)==a||(a&b)==b);}
struct Press {
    bool Held=false,Allowed=false;std::string Binding;
    bool Poll(std::string_view binding,bool down,bool allowed){const bool changed=Binding!=binding||Allowed!=allowed;Binding=binding;Allowed=allowed;const bool fire=!changed&&allowed&&down&&!Held;Held=down;return fire;}
    void Reset(){Held=false;Allowed=false;Binding.clear();}
};
}
