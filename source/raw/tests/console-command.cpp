#include "Core/ConsoleCommand.h"
#include <cassert>
#include <string>
#include <utility>
using namespace PS::Console;
int main(){
    assert(Parse(L"rs")==Command::Help);
    assert(Parse(L" \tRuNeScHeMa HeLP \t")==Command::Help);
    assert(Parse(L"rs status")==Command::Status);
    assert(Parse(L"rs NETWORK")==Command::Network);
    assert(Parse(L"rs mappings")==Command::Mappings);
    assert(Parse(L"rs services")==Command::Services);
    assert(Parse(L"rs vision")==Command::Invalid);
    assert(Parse(L"rs menu")==Command::Menu);
    assert(Parse(L"runeschema EXITMENU")==Command::Menu);
    assert(Parse(L"rs menu quit")==Command::Invalid);
    const std::pair<const wchar_t*,Command> menus[]={{L"menu",Command::Menu},{L"pause",Command::Menu},{L"exitmenu",Command::Menu},
        {L"inventory",Command::Inventory},{L"spellbook",Command::Spellbook},{L"map",Command::Map},
        {L"journal",Command::Journal},{L"skills",Command::Skills},{L"settings",Command::Settings},{L"buildmenu",Command::BuildMenu}};
    for(const auto& [name,kind]:menus){
        const auto prefix=std::wstring(L"rs ")+name;assert(Parse(prefix)==kind&&RequestedMenuAction(prefix)==MenuAction::Open);
        for(const auto& [word,action]:{std::pair{L"open",MenuAction::Open},std::pair{L"close",MenuAction::Close},std::pair{L"toggle",MenuAction::Toggle}}){
            const auto text=prefix+L" "+word;assert(Parse(text)==kind&&RequestedMenuAction(text)==action);
            assert(Parse(text+L" extra")==Command::Invalid);
        }
        for(const auto* bad:{L" quit",L" close;quit",L" open\nquit",L" close | quit",L" unknown"})assert(Parse(prefix+bad)==Command::Invalid);
        assert(MenuWidgetName(kind)!=nullptr);
    }
    assert(Parse(L"RuNeScHeMa InVeNtOrY ClOsE")==Command::Inventory&&RequestedMenuAction(L"RuNeScHeMa InVeNtOrY ClOsE")==MenuAction::Close);
    assert(std::wstring_view(MenuWidgetName(Command::Spellbook))==L"SpellBook");
    assert(std::wstring_view(MenuWidgetName(Command::BuildMenu))==L"Building");
    for(const auto kind:{Command::NotOurs,Command::Invalid,Command::Help,Command::Status,Command::Network,Command::Mappings,Command::Services})assert(MenuWidgetName(kind)==nullptr);
    assert(Parse(L"rs status close")==Command::Invalid&&Parse(L"rs close inventory")==Command::Invalid);
    assert(Parse(L"rs closeconsole")==Command::CloseConsole&&Parse(L"runeschema CLOSECONSOLE")==Command::CloseConsole);
    assert(Parse(L"rs closeconsole open")==Command::Invalid&&Parse(L"rs closeconsole;quit")==Command::Invalid);
    assert(MenuWidgetName(Command::CloseConsole)==nullptr);
    for(const auto* other:{L"",L"pause",L"rsdwt_help",L"rs.status",L"runeschema_other",L"showdebug enhancedinput"})
        assert(Parse(other)==Command::NotOurs);
    for(const auto* bad:{L"rs unknown",L"rs vision on",L"rs status | quit",L"rs status;quit",L"rs status\nquit",L"rs exec",L"rs delete",L"rs \u03b1"})
        assert(Parse(bad)==Command::Invalid);
    assert(Parse(L"rs "+std::wstring(513,L'x'))==Command::Invalid);
    const std::wstring nul(L"rs status\0quit",14);
    assert(Parse(nul)==Command::Invalid);
}
