#pragma once
#include <string_view>
#include <array>
#include <optional>

namespace PS::Console {
enum class Command { NotOurs, Invalid, Help, Status, Network, Mappings, Services, Menu, Inventory, Spellbook, Map, Journal, Skills, Settings, BuildMenu, CloseConsole, Mod };
enum class MenuAction { Open, Close, Toggle };
constexpr const wchar_t* MenuWidgetName(Command command){
    switch(command){
        case Command::Menu:return L"Pause";case Command::Inventory:return L"Inventory";
        case Command::Spellbook:return L"SpellBook";case Command::Map:return L"Map";
        case Command::Journal:return L"Journal";case Command::Skills:return L"Skills";
        case Command::Settings:return L"Settings";case Command::BuildMenu:return L"Building";
        default:return nullptr;
    }
}
inline constexpr std::wstring_view Help =
    L"RuneSchema console:\n"
    L"  rs help       - list commands\n"
    L"  rs status     - runtime and version\n"
    L"  rs network    - observed world/network role\n"
    L"  rs mappings   - active mapping (does not regenerate)\n"
    L"  rs services   - loaded plugin services/connections\n"
    L"  rs menu       - native pause menu (aliases: pause, exitmenu)\n"
    L"  rs inventory  - native inventory\n"
    L"  rs spellbook  - native spellbook\n"
    L"  rs map        - native world map\n"
    L"  rs journal    - native journal\n"
    L"  rs skills     - native skills menu\n"
    L"  rs settings   - native settings menu\n"
    L"  rs buildmenu  - native building menu\n"
    L"  rs closeconsole - close the console without changing game menus\n"
    L"  rs mod Owner:Id [args] - mod-owned local host command\n"
    L"Menus accept open (default), close, or toggle. Example: rs inventory toggle\n"
    L"Alias: runeschema. Diagnostics are read-only. No Exit action or save edit.";

constexpr bool Space(wchar_t c) { return c==L' '||c==L'\t'; }
constexpr bool Equal(std::wstring_view value,std::wstring_view expected) {
    if(value.size()!=expected.size())return false;
    for(size_t i=0;i<value.size();++i){
        auto c=value[i];if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
        if(c!=expected[i])return false;
    }
    return true;
}
constexpr std::wstring_view Word(std::wstring_view& value) {
    while(!value.empty()&&Space(value.front()))value.remove_prefix(1);
    const auto end=value.find_first_of(L" \t");
    const auto word=value.substr(0,end);
    value.remove_prefix(word.size());return word;
}
struct ModRequest {
    std::wstring_view Owner,Id;
    std::array<std::wstring_view,8> Arguments{};
    size_t Count=0;
};
constexpr bool ModToken(std::wstring_view value){
    if(value.empty()||value.size()>96||value==L"."||value==L"..")return false;
    for(auto c:value)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-'||c==L'.'))return false;
    return true;
}
constexpr bool ModArgument(std::wstring_view value){
    size_t bytes=0;
    for(size_t i=0;i<value.size();++i){
        auto c=static_cast<unsigned long>(value[i]);
        if(c<32||c==127||c==L'"'||c==L'\''||c==L'\\')return false;
        if(c>=0xd800&&c<=0xdbff){
            if(++i==value.size()||value[i]<0xdc00||value[i]>0xdfff)return false;
            bytes+=4;
        }else if((c>=0xdc00&&c<=0xdfff)||c>0x10ffff)return false;
        else bytes+=c<0x80?1:c<0x800?2:c<0x10000?3:4;
        if(bytes>64)return false;
    }
    return !value.empty();
}
constexpr std::optional<ModRequest> ParseMod(std::wstring_view value){
    if(value.size()>=1024)return {};
    const auto prefix=Word(value);
    if((!Equal(prefix,L"rs")&&!Equal(prefix,L"runeschema"))||!Equal(Word(value),L"mod"))return {};
    const auto target=Word(value);const auto colon=target.find(L':');
    if(colon==target.npos||!ModToken(target.substr(0,colon))||!ModToken(target.substr(colon+1)))return {};
    ModRequest result{target.substr(0,colon),target.substr(colon+1)};
    for(auto argument=Word(value);!argument.empty();argument=Word(value)){
        if(result.Count==result.Arguments.size()||!ModArgument(argument))return {};
        result.Arguments[result.Count++]=argument;
    }
    return result;
}
constexpr Command Parse(std::wstring_view value) {
    const auto original=value;
    const auto prefix=Word(value);
    if(!Equal(prefix,L"rs")&&!Equal(prefix,L"runeschema"))return Command::NotOurs;
    auto check=value;if(Equal(Word(check),L"mod"))return ParseMod(original)?Command::Mod:Command::Invalid;
    if(value.size()>256)return Command::Invalid;
    const auto command=Word(value);
    const auto action=Word(value);if(!Word(value).empty())return Command::Invalid;
    Command menu=Command::Invalid;
    if(Equal(command,L"menu")||Equal(command,L"exitmenu")||Equal(command,L"pause"))menu=Command::Menu;
    else if(Equal(command,L"inventory"))menu=Command::Inventory;
    else if(Equal(command,L"spellbook"))menu=Command::Spellbook;
    else if(Equal(command,L"map"))menu=Command::Map;
    else if(Equal(command,L"journal"))menu=Command::Journal;
    else if(Equal(command,L"skills"))menu=Command::Skills;
    else if(Equal(command,L"settings"))menu=Command::Settings;
    else if(Equal(command,L"buildmenu"))menu=Command::BuildMenu;
    if(menu!=Command::Invalid)return action.empty()||Equal(action,L"open")||Equal(action,L"close")||Equal(action,L"toggle")?menu:Command::Invalid;
    if(!action.empty())return Command::Invalid;
    if(command.empty()||Equal(command,L"help"))return Command::Help;
    if(Equal(command,L"status"))return Command::Status;
    if(Equal(command,L"network"))return Command::Network;
    if(Equal(command,L"mappings"))return Command::Mappings;
    if(Equal(command,L"services"))return Command::Services;
    if(Equal(command,L"closeconsole"))return Command::CloseConsole;
    return Command::Invalid;
}
// Only used after Parse accepted an allowlisted menu command and action.
constexpr MenuAction RequestedMenuAction(std::wstring_view value){
    Word(value);Word(value);const auto action=Word(value);
    return Equal(action,L"close")?MenuAction::Close:Equal(action,L"toggle")?MenuAction::Toggle:MenuAction::Open;
}
}
