#include "Core/SettingsFieldState.h"
#include <cassert>
#include <iostream>
// Settings fields report unsaved and non-default state against the core snapshot.
using namespace PS;using Json=nlohmann::json;
int main(){
    const Json snapshot={{"Editable",{{"/enabled",true},{"/helpyHotkey","F2"},{"/cache/limit",64},{"/list",Json::array()}}},
        {"Document",{{"enabled",false},{"helpyHotkey","F2"},{"cache",{{"limit",64}}},{"list",Json::array()}}}};
    auto draft=snapshot.at("Document");
    auto state=SettingsFieldState::For(snapshot,draft,"/enabled");
    assert(!state.Unsaved&&state.NonDefault);                      // Saved on disk, but not the shipped default.
    assert(!SettingsFieldState::For(snapshot,draft,"/helpyHotkey").NonDefault);
    draft["cache"]["limit"]=64.0;                                     // Same number in another JSON type.
    assert(!SettingsFieldState::For(snapshot,draft,"/cache/limit").Unsaved);
    draft["cache"]["limit"]=128;
    state=SettingsFieldState::For(snapshot,draft,"/cache/limit");
    assert(state.Unsaved&&state.NonDefault);
    assert(SettingsFieldState::CountNonDefault(snapshot,draft)==2);
    assert(!SettingsFieldState::For(snapshot,draft,"/not-editable").NonDefault);
    assert(!SettingsFieldState::For(Json::object(),draft,"/enabled").Unsaved);
    assert(SettingsFieldState::DefaultText(true,false)=="On");
    assert(SettingsFieldState::DefaultText("1234",true)=="hidden");
    assert(SettingsFieldState::DefaultText("",false)=="empty");
    assert(SettingsFieldState::DefaultText(Json::array(),false)=="empty list");
    assert(SettingsFieldState::DefaultText(std::string(80,'x'),false).size()==48);
    std::cout<<"Settings fields report unsaved and non-default state\n";
}
