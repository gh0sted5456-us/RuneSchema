#pragma once
#include "Core/AdditionalLoggingInfo.h"
#include "Core/RuneSchemaSettingsEditor.h"
namespace PS::AdditionalLoggingConfiguration {
using Json=nlohmann::json;
inline unsigned Bits(const Json& d){namespace I=AdditionalLoggingInfo;return (d.at("enabled").get<bool>()?I::Enabled:0)|(d.at("filePaths").get<bool>()?I::FilePaths:0)|(d.at("region").get<bool>()?I::Region:0)|(d.at("networkRates").get<bool>()?I::NetworkRates:0)|(d.at("savePaths").get<bool>()?I::SavePaths:0)|(d.at("averageLatency").get<bool>()?I::AverageLatency:0);}
inline Json Present(Json snapshot){if(snapshot.value("Ok",false)){snapshot["Document"]=Json(snapshot.at("Document").at("additionalLogging"));snapshot.erase("Editable");}return snapshot;}
inline Json Read(const std::filesystem::path& directory){return Present(RuneSchemaSettingsEditor::Read(directory));}
inline Json Save(const std::filesystem::path& directory,const Json& request){
    const auto& document=request.at("Document");if(!document.is_object()||document.size()!=6)throw std::runtime_error("Additional logging needs exactly six boolean options");
    (void)Bits(document);Json patch=Json::object();
    for(const auto* key:{"enabled","filePaths","region","networkRates","savePaths","averageLatency"})patch[std::string("/additionalLogging/")+key]=document.at(key);
    return Present(RuneSchemaSettingsEditor::Save(directory,{{"ExpectedContents",request.at("ExpectedContents")},{"Patch",patch}}));
}
}
