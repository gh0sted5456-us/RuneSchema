#pragma once
#include "Core/RuneSchemaSettingsEditor.h"
#include "Core/ModArchiveImport.h"
#include <cstdio>

namespace PS::HelpyOfflineRuntime {
inline nlohmann::json SourceDocument;
inline std::filesystem::path SourceRoot,SourceFile;
inline std::filesystem::file_time_type SourceStamp{};
inline void InvalidateSources(){SourceDocument={};}
inline nlohmann::json Sources(const std::filesystem::path& assigned,const std::filesystem::path& file){
    const auto root=RuntimeModPath::Resolve(assigned);const auto stamp=std::filesystem::last_write_time(file);
    if(SourceDocument.is_object()&&SourceRoot==root&&SourceFile==file&&SourceStamp==stamp)return SourceDocument;
    if(std::filesystem::file_size(file)>2*1024*1024)throw std::runtime_error("Saved sources exceed the offline presentation bound");
    std::ifstream input(file,std::ios::binary);auto value=nlohmann::json::parse(input,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>32)throw std::runtime_error("Saved source nesting exceeds its bound");return true;});
    const auto settings=RuneSchemaSettingsEditor::Read(root/"settings");if(!settings.value("Ok",false))return {{"Ready",false}};
    nlohmann::json mods;{std::lock_guard lock(ModManager::Gate);mods=ModManager::Read(root,settings.at("Document").at("loadOrder").at("enabled").get<bool>());}
    if(!mods.value("Ok",false))return {{"Ready",false}};
    std::set<std::string> active;for(const auto& row:mods.at("Rows"))if(row.value("Enabled",false)&&row.value("Present",false)){auto owner=row.at("Name").get<std::string>();std::transform(owner.begin(),owner.end(),owner.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});active.insert(std::move(owner));}
    for(const auto* field:{"Owners","DeclaredOwners"})if(value.contains(field))for(auto& owners:value.at(field)){if(!owners.is_array())throw std::runtime_error("Invalid saved source owners");std::erase_if(owners.get_ref<nlohmann::json::array_t&>(),[&](const auto& owner){return !owner.is_string()||!active.contains(owner.template get<std::string>());});}
    uint64_t identity=14695981039346656037ull;for(unsigned char c:value.at("Key").get<std::string>()+value.value("Owners",nlohmann::json::object()).dump()+value.value("DeclaredOwners",nlohmann::json::object()).dump()){identity^=c;identity*=1099511628211ull;}
    char key[17]{};std::snprintf(key,sizeof(key),"%016llx",static_cast<unsigned long long>(identity));value["Key"]=key;
    value["PresentationOnly"]=true;SourceRoot=root;SourceFile=file;SourceStamp=stamp;SourceDocument=value;return value;
}
inline nlohmann::json Call(const std::filesystem::path& assigned,const nlohmann::json& request){
    const auto root=RuntimeModPath::Resolve(assigned);const auto action=request.at("Action").get<std::string>();
    if(action=="SettingsRead")return RuneSchemaSettingsEditor::Read(root/"settings");
    if(action=="SettingsSave"){auto result=RuneSchemaSettingsEditor::Save(root/"settings",request);InvalidateSources();return result;}
    const auto settings=RuneSchemaSettingsEditor::Read(root/"settings");
    if(!settings.value("Ok",false))return {{"Ok",false},{"Status","Read this runtime's settings.ini before editing its mod order."}};
    const auto enabled=settings.at("Document").at("loadOrder").at("enabled").get<bool>();
    nlohmann::json result;
    if(action=="ModsRead"){std::lock_guard lock(ModManager::Gate);result=ModManager::Read(root,enabled);}
    else if(action=="ModsSave"){RuntimeModPath::Expected(root,request.value("ExpectedRoot",std::string{}));result=ModManager::Save(root,enabled,request);}
    else if(action=="ModsImport")result=ModManager::Import(request,root,enabled);
    else return {{"Ok",false},{"Status","This operation needs a connected game."}};
    InvalidateSources(); // Refreshing mod order also reconciles offline cached owners.
    for(auto& row:result["Rows"]){row["RegistryDeclarations"]=nlohmann::json::array();row["AcceptedDeclarations"]=0;row["RejectedDeclarations"]=0;}
    result["Root"]=RuntimeModPath::Text(root);result["ModsDirectory"]=RuntimeModPath::Text(root/"mods");result["Runtime"]="Assigned runtime (offline)";return result;
}
}
