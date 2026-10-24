#include "Core/QuickLaunchConfiguration.h"
#include <chrono>
#include <stdexcept>
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){
    using namespace PS;using Json=nlohmann::json;
    const auto defaults=DecodeSettings("{}");
    Need(defaults.quickLaunch.mode=="lessRestrictive"&&defaults.quickLaunch.softLoading&&!defaults.quickLaunch.enabled);
    Need(!defaults.persistence.allowCorruptCharacterEntry&&defaults.worldSaveCleanup.backupBeforePrune);
    const auto root=std::filesystem::temp_directory_path()/("quick-launch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/"settings.ini";
    auto document=SettingsDocument(PSConfigSettings{});document["futureSection"]={{"keep","unchanged"}};
    for(bool bypass:{false,true})for(const auto* mode:{"vanilla","lessRestrictive","quickLaunch"}){
        document["persistence"]["allowCorruptCharacterEntry"]=bypass;ConfigFiles::Write(file,EncodeIniDocument(document));
        const auto before=ConfigFiles::Read(file);const auto snapshot=QuickLaunchConfiguration::Read(root);Need(snapshot.at("Ok")==true);
        auto selection=Json{{"Mode",mode},{"CharacterName","BREWER"},{"WorldName","World #2; New"}};
        const auto save=[&](const Json& value,const Json& expected){return QuickLaunchConfiguration::Save(root,{{"Document",value},{"ExpectedContents",expected}});};
        const auto saved=save(selection,snapshot.at("ExpectedContents"));Need(saved.at("Ok")==true);
        Need(ConfigFiles::Read(saved.at("Backup").get<std::string>())==before);
        const auto stored=DecodeIniDocument(ConfigFiles::Read(file));Need(stored.at("persistence").at("allowCorruptCharacterEntry")==bypass);
        Need(stored.at("futureSection").at("keep")=="unchanged");Need(stored.at("quickLaunch").at("mode")==mode);
        Need(stored.at("quickLaunch").at("enabled")==(std::string(mode)=="quickLaunch"));
        Need(stored.at("quickLaunch").at("softLoading")==(std::string(mode)!="vanilla"));
        Need(stored.at("quickLaunch").at("worldName")=="World #2; New");
        Need(saved.at("Document")==selection);const auto committed=ConfigFiles::Read(file);
        Need(save(selection,before).at("Ok")==false);Need(ConfigFiles::Read(file)==committed);
        selection["Mode"]="vanilla";const auto normal=save(selection,committed);Need(normal.at("Ok")==true);
        Need(normal.at("Document").at("CharacterName")=="BREWER"&&normal.at("Document").at("WorldName")=="World #2; New");
        selection["Mode"]="quickLaunch";selection["WorldName"]="";bool rejected=false;
        const auto preserved=ConfigFiles::Read(file);try{(void)save(selection,preserved);}catch(...){rejected=true;}
        Need(rejected&&ConfigFiles::Read(file)==preserved);
    }
    // Unset legacy mode inherits Less Restrictive; explicit Vanilla stays opt-in.
    for(bool enabled:{false,true})for(bool softLoading:{false,true}){
        auto legacy=SettingsDocument(PSConfigSettings{});legacy["quickLaunch"].erase("mode");
        legacy["quickLaunch"]["enabled"]=enabled;legacy["quickLaunch"]["softLoading"]=softLoading;
        ConfigFiles::Write(file,EncodeIniDocument(legacy));const auto snapshot=QuickLaunchConfiguration::Read(root);
        Need(snapshot.at("Document").at("Mode")== (enabled?"quickLaunch":"lessRestrictive"));
    }
    for(const auto* mode:{"vanilla","lessRestrictive"}){
        const auto snapshot=QuickLaunchConfiguration::Read(root);
        const auto saved=QuickLaunchConfiguration::Save(root,{{"ExpectedContents",snapshot.at("ExpectedContents")},{"Document",{{"Mode",mode},{"CharacterName",""},{"WorldName",""}}}});
        Need(saved.at("Ok")==true); // Only automatic launch requires selected slots.
    }
    bool invalid=false;try{(void)DecodeSettings(R"({"quickLaunch":{"mode":"unknown"}})");}catch(...){invalid=true;}Need(invalid);
    // Remove only this exact, uniquely created test fixture; never game data.
    std::filesystem::remove_all(root);
}
