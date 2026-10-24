#include "Core/RuneSchemaSettingsEditor.h"
#include <chrono>
#include <stdexcept>
#include <iostream>
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main()try{
    using namespace PS;using json=nlohmann::json;
    const auto root=std::filesystem::temp_directory_path()/("runeschema-settings-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);const auto file=root/"settings.ini";
    auto document=SettingsDocument(PSConfigSettings{});document["futureSection"]={{"keep","unchanged"}};
    ConfigFiles::Write(file,EncodeIniDocument(document));
    auto initial=RuneSchemaSettingsEditor::Read(root);Need(initial.at("Ok").get<bool>());
    Need(initial["Editable"]["/archiveLogs"]==false&&initial["Document"]["archiveLogs"]==false);
    Need(!initial["Editable"].contains("/futureSection/keep"));
    Need(!initial["Editable"].contains("/persistence/recovery")&&!initial["Editable"].contains("/worldSaveCleanup/backupBeforePrune"));
    const auto original=ConfigFiles::Read(file);
    const auto save=[&](const json& patch,const json& expected){return RuneSchemaSettingsEditor::Save(root,{{"Patch",patch},{"ExpectedContents",expected}});};
    for(const auto patch:{json{{"/futureSection/keep","overwrite"}},json{{"/advancedLogging","true"}},json{{"/archiveLogs","true"}},json{{"/loaders/recipes",nullptr}},json{{"/helpyAuthority/maximumItemCount",-1}},json{{"/persistence/recovery",true}},json{{"/worldSaveCleanup/backupBeforePrune",false}}}){
        Need(!save(patch,original).at("Ok").get<bool>());Need(ConfigFiles::Read(file)==original);
    }
    Need(!save(json{{"/advancedLogging",true}},"stale").at("Ok").get<bool>());
    const auto saved=save(json{{"/advancedLogging",true},{"/archiveLogs",true},{"/persistence/allowCorruptCharacterEntry",true}},original);
    Need(saved.at("Ok").get<bool>()&&saved.at("RestartRequired").get<bool>());
    Need(ConfigFiles::Read(saved.at("Backup").get<std::string>())==original);
    Need(saved["Document"]["futureSection"]["keep"]=="unchanged");
    Need(saved["Document"]["advancedLogging"]==true&&saved["Document"]["persistence"]["allowCorruptCharacterEntry"]==true);
    Need(saved["Document"]["defaultRecovery"]["enabled"]==false&&saved["Document"]["defaultRecovery"]["progress"]==false);
    Need(saved["Document"]["archiveLogs"]==true);
    Need(saved["Document"]["persistence"]["recovery"]==false);
    const auto current=ConfigFiles::Read(file);Need(!save(json{{"/advancedLogging",false}},original).at("Ok").get<bool>());Need(ConfigFiles::Read(file)==current);
    ConfigFiles::Write(file,"broken settings");Need(!RuneSchemaSettingsEditor::Read(root).at("Ok").get<bool>());
    Need(!save(json{{"/advancedLogging",false}},"broken settings").at("Ok").get<bool>());Need(ConfigFiles::Read(file)=="broken settings");
    // Only the exact test-created temporary directory is removed.
    std::filesystem::remove_all(root);
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
