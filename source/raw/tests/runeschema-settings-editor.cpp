#include "Core/RuneSchemaSettingsEditor.h"
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <fstream>
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(int argc,char** argv)try{
    using namespace PS;using json=nlohmann::json;
    const auto root=std::filesystem::temp_directory_path()/("runeschema-settings-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);const auto file=root/"settings.ini";
    auto document=SettingsDocument(PSConfigSettings{});document["futureSection"]={{"keep","unchanged"}};
    ConfigFiles::Write(file,EncodeIniDocument(document));
    auto initial=RuneSchemaSettingsEditor::Read(root);Need(initial.at("Ok").get<bool>());
    Need(initial["Editable"]["/archiveLogs"]==false&&initial["Document"]["archiveLogs"]==false);
    Need(initial["Editable"]["/logsInUE4SS"]==false&&initial["Document"]["logsInUE4SS"]==false);
    Need(!initial["Editable"].contains("/futureSection/keep"));
    Need(!initial["Editable"].contains("/persistence/recovery")&&!initial["Editable"].contains("/worldSaveCleanup/backupBeforePrune"));
    Need(initial["Editable"]["/persistence/blanketCleanup"]==false
        &&initial["Editable"]["/persistence/cleanupOnWorldEntry"]==false);
    if(argc>1){
        std::ifstream file(argv[1]);const auto menu=json::parse(file);
        Need(menu.at("config").at("file")=="settings/settings.ini"&&menu.at("config").at("format")=="lua");
        for(const auto& field:menu.at("settings"))if(field.contains("key")){
            const auto key=field.at("key").get<std::string>();size_t matches=0;
            for(const auto& [path,value]:initial.at("Editable").items())if(path.substr(path.find_last_of('/')+1)==key){
                ++matches;Need(value==field.at("default"));
                if(value.is_boolean())Need(field.at("type")=="toggle");
                else {Need(field.at("type")=="choice");for(const auto& option:field.at("options"))Need(HelpyShortcut::Find(option.get<std::string>()));}
            }
            Need(matches==1);
        }
    }
    const auto original=ConfigFiles::Read(file);
    const auto save=[&](const json& patch,const json& expected){return RuneSchemaSettingsEditor::Save(root,{{"Patch",patch},{"ExpectedContents",expected}});};
    for(const auto patch:{json{{"/futureSection/keep","overwrite"}},json{{"/advancedLogging","true"}},json{{"/archiveLogs","true"}},json{{"/loaders/recipes",nullptr}},json{{"/helpyAuthority/maximumItemCount",-1}},json{{"/persistence/recovery",true}},json{{"/worldSaveCleanup/backupBeforePrune",false}}}){
        Need(!save(patch,original).at("Ok").get<bool>());Need(ConfigFiles::Read(file)==original);
    }
    Need(!save(json{{"/advancedLogging",true}},"stale").at("Ok").get<bool>());
    Need(!save(json{{"/helpyHotkey","not-a-key"}},original).at("Ok").get<bool>());Need(ConfigFiles::Read(file)==original);
    Need(initial["Document"]["helpyHotkey"]=="F2"&&initial["Editable"].contains("/helpyHotkey"));
    const auto saved=save(json{{"/advancedLogging",true},{"/archiveLogs",true},{"/logsInUE4SS",true},{"/persistence/allowCorruptCharacterEntry",true},{"/persistence/blanketCleanup",false}},original);
    Need(saved.at("Ok").get<bool>()&&saved.at("RestartRequired").get<bool>());
    Need(ConfigFiles::Read(saved.at("Backup").get<std::string>())==original);
    Need(saved["Document"]["futureSection"]["keep"]=="unchanged");
    Need(saved["Document"]["advancedLogging"]==true&&saved["Document"]["persistence"]["allowCorruptCharacterEntry"]==true);
    Need(saved["Document"]["defaultRecovery"]["enabled"]==false&&saved["Document"]["defaultRecovery"]["progress"]==false);
    Need(saved["Document"]["archiveLogs"]==true);
    Need(saved["Document"]["logsInUE4SS"]==true&&DecodeSettings(DecodeIniDocument(ConfigFiles::Read(file)).dump()).logsInUE4SS);
    Need(saved["Document"]["persistence"]["recovery"]==false);
    Need(saved["Document"]["persistence"]["blanketCleanup"]==false
        &&!DecodeSettings(DecodeIniDocument(ConfigFiles::Read(file)).dump()).persistence.blanketCleanup);
    const auto current=ConfigFiles::Read(file);Need(!save(json{{"/advancedLogging",false}},original).at("Ok").get<bool>());Need(ConfigFiles::Read(file)==current);
    const auto shortcut=save(json{{"/helpyHotkey","F5"}},current);Need(shortcut.at("Ok").get<bool>());
    Need(shortcut["Document"]["helpyHotkey"]=="F5"&&DecodeSettings(DecodeIniDocument(ConfigFiles::Read(file)).dump()).helpyHotkey=="F5");
    Need(ConfigFiles::Read(shortcut.at("Backup").get<std::string>())==current);
    const auto entrySave=save(json{{"/persistence/cleanupOnWorldEntry",true}},ConfigFiles::Read(file));
    Need(entrySave.at("Ok")==true&&entrySave.at("RestartRequired")==false);
    const auto launchSave=save(json{{"/persistence/cleanupOnWorldEntry",false}},ConfigFiles::Read(file));
    Need(launchSave.at("Ok")==true&&launchSave.at("RestartRequired")==true);
    auto legacy=DecodeIniDocument(ConfigFiles::Read(file));legacy["persistence"].erase("blanketCleanup");legacy["persistence"].erase("cleanupOnWorldEntry");
    legacy.erase("helpyHotkey");legacy.erase("archiveLogs");legacy["persistence"].erase("allowCorruptCharacterEntry");
    ConfigFiles::Write(file,EncodeIniDocument(legacy));const auto legacyText=ConfigFiles::Read(file);
    const auto prepared=RuneSchemaSettingsEditor::PrepareModMenuFields(root);Need(prepared.at("Ok")==true);
    Need(ConfigFiles::Read(prepared.at("Backup").get<std::string>())==legacyText);
    const auto seeded=DecodeIniDocument(ConfigFiles::Read(file));
    Need(seeded.at("persistence").at("blanketCleanup")==false&&seeded.at("persistence").at("cleanupOnWorldEntry")==false);
    Need(seeded.at("futureSection").at("keep")=="unchanged");
    Need(seeded.at("helpyHotkey")=="F2"&&seeded.at("archiveLogs")==false
        &&seeded.at("persistence").at("allowCorruptCharacterEntry")==false);
    Need(seeded.at("advancedLogging")==true&&seeded.at("autoUpdateMappings")==true);
    auto explicitBlanket=seeded;explicitBlanket["persistence"]["blanketCleanup"]=true;
    ConfigFiles::Write(file,EncodeIniDocument(explicitBlanket));
    const auto seededText=ConfigFiles::Read(file);Need(RuneSchemaSettingsEditor::PrepareModMenuFields(root).at("Ok")==true&&ConfigFiles::Read(file)==seededText);
    ConfigFiles::Write(file,"broken settings");Need(!RuneSchemaSettingsEditor::Read(root).at("Ok").get<bool>());
    Need(!save(json{{"/advancedLogging",false}},"broken settings").at("Ok").get<bool>());Need(ConfigFiles::Read(file)=="broken settings");
    // Only the exact test-created temporary directory is removed.
    std::filesystem::remove_all(root);
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
