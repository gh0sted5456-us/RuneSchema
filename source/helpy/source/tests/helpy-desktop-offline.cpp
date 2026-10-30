#include "Runtime/HelpyOfflineRuntime.h"
#include <cassert>
#include <iostream>
int main()try{
    namespace fs=std::filesystem;using Json=nlohmann::json;
    const auto root=fs::temp_directory_path()/("HelpyOffline-"+std::to_string(GetCurrentProcessId()));
    fs::create_directories(root/"settings");fs::create_directories(root/"mods/Example");
    struct Cleanup{fs::path Root;~Cleanup(){std::error_code error;fs::remove_all(Root,error);}} cleanup{root};
    PS::ConfigFiles::Write(root/"settings/settings.ini",PS::EncodeIniDocument(PS::SettingsDocument(PS::PSConfigSettings{})));
    auto settings=PS::HelpyOfflineRuntime::Call(root,{{"Action","SettingsRead"}});assert(settings.at("Ok")==true);
    auto changed=PS::HelpyOfflineRuntime::Call(root,{{"Action","SettingsSave"},{"ExpectedContents",settings.at("ExpectedContents")},{"Patch",{{"/helpyHotkey","F4"}}}});
    assert(changed.at("Ok")==true&&changed.at("Document").at("helpyHotkey")=="F4");
    auto mods=PS::HelpyOfflineRuntime::Call(root,{{"Action","ModsRead"}});assert(mods.at("Ok")==true&&mods.at("Rows").size()==1);
    const auto sources=root/"sources.json";PS::ConfigFiles::Write(sources,Json{{"Ready",true},{"Key","0123456789abcdef"},{"Owners",{{"example",Json::array({"example","removed"})}}},{"DeclaredOwners",{{"example",Json::array({"example","removed"})}}}}.dump());
    auto visible=PS::HelpyOfflineRuntime::Sources(root,sources);assert(visible.at("PresentationOnly")==true&&visible.at("Owners").at("example")==Json::array({"example"}));
    auto request=mods;request["Action"]="ModsSave";request["ExpectedRoot"]=mods.at("Root");request["Rows"][0]["Enabled"]=false;
    auto saved=PS::HelpyOfflineRuntime::Call(root,request);assert(saved.at("Rows")[0]["Enabled"]==false);
    const auto hidden=PS::HelpyOfflineRuntime::Sources(root,sources);assert(hidden.at("Owners").at("example").empty()&&hidden.at("Key")!=visible.at("Key"));
    bool refused=false;try{PS::HelpyOfflineRuntime::Call(root,request);}catch(...){refused=true;}assert(refused);
    assert(PS::HelpyOfflineRuntime::Call(root,{{"Action","Spawn"}}).at("Ok")==false);
    std::cout<<"Offline settings, mod order, stale-write and gameplay separation passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
