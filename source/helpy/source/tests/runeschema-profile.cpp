#include "Core/RuneSchemaProfile.h"
#include <cassert>
using namespace PS;using Json=nlohmann::json;
int main(int argc,char** argv){
    assert(argc==2);namespace P=RuneSchemaProfile;const auto root=std::filesystem::path(argv[1])/std::to_string(GetCurrentProcessId());const auto legacy=root/"HELPY";
    const auto labels=Json{{"Format",1},{"Entries",Json::object()}}.dump();ConfigFiles::Write(legacy/"personalization.json",labels);
    ConfigFiles::Write(legacy/"runtime/saved/portraits/1234567890abcdef1234567890abcdef.rsicon","old-picture");ConfigFiles::Write(legacy/"runtime/Helpy/kits/kit.json","kit");
    assert(P::Load(legacy));P::Shutdown();const auto doc=P::Read();const auto id=doc.at("ProfileId").get<std::string>();assert(HelpyPersonalization::Guid(id)==id);
    assert(ConfigFiles::Read(P::Data("labels.json"))==labels&&ConfigFiles::Read(legacy/"personalization.json")==labels);
    assert(ConfigFiles::Read(P::Data("loadouts")/"kit.json")=="kit");assert(ConfigFiles::Read(P::Data("portraits")/"1234567890abcdef1234567890abcdef.rsicon")=="old-picture");
    assert(P::Save("Account Name",{{"Steam","https://steamcommunity.com/id/my-name"},{"Nexus","https://www.nexusmods.com/users/123"},{"Discord","my.user"}}));
    assert(!P::Save("Another Name",Json::object())&&P::Read().at("ProfileName")=="Account Name");
    Json player={{"IsLocal",true},{"Guid","11111111222222223333333344444444"},{"Name","Character One"},{"Platform","Game Pass"}};P::Observe(player);assert(P::Name(player)=="Account Name (Character One)");
    player["Name"]="Character Two";player["Platform"]="Steam";P::Observe(player);assert(P::Name(player)=="Account Name (Character Two)"&&P::Read().at("Platforms").size()==2);
    player["PlatformAccountName"]="Steam Persona";P::Observe(player);assert(P::Read().at("PlatformAccounts").at("Steam")=="Steam Persona");
    player["PlatformAccountName"]="";P::Observe(player);assert(P::Read().at("PlatformAccounts").at("Steam")=="Steam Persona");
    player["Platform"]="Game Pass";player["PlatformAccountName"]="Xbox Gamertag";P::Observe(player);assert(P::Read().at("PlatformAccounts").size()==2);
    player["PlatformAccountName"]="bad\nname";P::Observe(player);assert(P::Read().at("PlatformAccounts").at("Game Pass")=="Xbox Gamertag");
    player["PlatformAccountName"]="Xbox Gamertag";
    const auto revision=P::Revision.load();P::Observe(player);assert(P::Revision==revision);player["IsLocal"]=false;player["Guid"]="aaaaaaaa222222223333333344444444";P::Observe(player);assert(P::Read().at("Characters").size()==1&&P::Name(player)=="Character Two");
    assert(P::Read().at("PlatformAccounts").size()==2); // A remote row must not overwrite locally observed accounts.
    const auto publicProfile=P::Public();assert(publicProfile.size()==4&&!publicProfile.contains("Characters")&&!publicProfile.contains("Permissions")&&!publicProfile.contains("PlatformAccounts"));
    const auto remote=player.at("Guid").get<std::string>();P::ReceivePublic(remote,publicProfile);assert(P::Name(player)=="Account Name (Character Two)"&&P::PublicFor(remote)==publicProfile);
    auto bad=publicProfile;bad["Characters"]=P::Read().at("Characters");bool rejected=false;try{P::ReceivePublic(remote,bad);}catch(...){rejected=true;}assert(rejected&&P::PublicFor(remote)==publicProfile);
    bad=publicProfile;bad["ProfileId"]="";rejected=false;try{P::ValidatePublic(bad);}catch(...){rejected=true;}assert(rejected);
    bad=publicProfile;bad["Social"]["Steam"]="https://steamcommunity.com.evil/id/steal";rejected=false;try{P::ReceivePublic(remote,bad);}catch(...){rejected=true;}assert(rejected);
    P::ReceivePublic(remote,nullptr);assert(P::PublicFor(remote).is_null());P::ReceivePublic(remote,publicProfile);P::ClearShared();assert(P::PublicFor(remote).is_null());
    auto malformed=player;malformed["IsLocal"]=true;malformed["Name"]=99;P::Observe(malformed);
    assert(!P::Link("Steam","https://steamcommunity.com.evil/id/a")&&!P::Link("Steam","https://steamcommunity.com/id/a?x=1")&&!P::Link("Nexus","file:///C:/private")&&!P::Link("Discord","bad\nname"));
    const auto contents=ConfigFiles::Read(P::Directory/"profile.json");ConfigFiles::Write(P::Directory/"profile.json",contents+" ");assert(!P::Save("Account Name",{{"Discord","new-name"}}));assert(ConfigFiles::Read(P::Directory/"profile.json")==contents+" ");
    assert(P::Load(legacy)&&P::Read().at("ProfileId")==id&&P::Read().at("ProfileName")=="Account Name");assert(P::Read().at("PlatformAccounts").at("Steam")=="Steam Persona");P::Shutdown();
    ConfigFiles::Write(P::Data("labels.json"),"current-labels");ConfigFiles::Write(legacy/"personalization.json",labels);assert(P::Load(legacy));P::Shutdown();assert(ConfigFiles::Read(P::Data("labels.json"))=="current-labels");
    ConfigFiles::Write(P::Directory/"profile.json","invalid");assert(!P::Load(legacy));assert(ConfigFiles::Read(P::Directory/"profile.json")=="invalid");
}
