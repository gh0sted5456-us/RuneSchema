#pragma once
#include "Core/RuneSchemaSettingsEditor.h"
#include "Core/QuickLoadSettings.h"
namespace PS::QuickLaunchConfiguration {
using Json=nlohmann::json;
inline QuickLoadSettings::Config Selection(const Json& document){
    return QuickLoadSettings::Normalize({document.at("enabled").get<bool>(),document.at("characterName").get<std::string>(),document.at("worldName").get<std::string>(),document.at("softLoading").get<bool>(),document.at("mode").get<std::string>()});
}
inline Json Present(Json snapshot){
    if(!snapshot.value("Ok",false))return snapshot;
    const auto selection=Selection(snapshot.at("Document").at("quickLaunch"));
    snapshot["Document"]=QuickLoadSettings::Document(selection);
    snapshot.erase("Editable");return snapshot;
}
inline Json Read(const std::filesystem::path& directory){return Present(RuneSchemaSettingsEditor::Read(directory));}
inline Json Save(const std::filesystem::path& directory,const Json& request){
    const auto& document=request.at("Document");
    if(document.at("Mode").get<std::string>().empty())throw std::runtime_error("Select a loading mode");
    const auto next=QuickLoadSettings::Normalize({false,document.at("CharacterName").get<std::string>(),document.at("WorldName").get<std::string>(),false,document.at("Mode").get<std::string>()});
    (void)QuickLoadSettings::Encode(next); // Native names and bounds; no INI injection.
    const auto result=RuneSchemaSettingsEditor::Save(directory,{{"ExpectedContents",request.at("ExpectedContents")},
        {"Patch",{{"/quickLaunch/mode",next.Mode},{"/quickLaunch/enabled",next.Enabled},{"/quickLaunch/characterName",next.CharacterName},{"/quickLaunch/worldName",next.WorldName},{"/quickLaunch/softLoading",next.SoftLoading}}}});
    auto response=Present(result);
    if(response.value("Ok",false))response["Status"]="Launch options saved in settings.ini for the next launch. Save Bypass and the running world are unchanged.";
    return response;
}
}
