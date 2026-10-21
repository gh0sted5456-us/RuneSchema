#include "Runtime/HelpySettings.h"
#include <cassert>
#include <fstream>
#include <windows.h>
#include <iostream>
#undef assert
#define assert(x) do{if(!(x))throw std::runtime_error("line "+std::to_string(__LINE__)+": "+#x+"; "+PS::HelpySettings::Message());}while(false)
int main(){
    try{
    using namespace PS;namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("helpy-startup-preference-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root);
    HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart());
    assert(!HelpySettings::ScrapRefunds());assert(HelpySettings::SetScrapRefunds(true));HelpySettings::Configure(root);assert(HelpySettings::ScrapRefunds());
    assert(HelpySettings::SetOpenOnGameStart(true));HelpySettings::Configure(root);assert(HelpySettings::OpenOnGameStart());
    const auto file=root/"settings"/"settings.jsonc";
    auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);doc["ownerField"]="preserved";ConfigFiles::Write(file,doc.dump());
    // An external edit is never overwritten by an old in-memory draft.
    assert(!HelpySettings::SetOpenOnGameStart(false));assert(nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true)["ownerField"]=="preserved");
    HelpySettings::Configure(root);assert(HelpySettings::SetOpenOnGameStart(false));assert(HelpySettings::SetScrapRefunds(false));assert(!HelpySettings::ScrapRefunds());assert(nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true)["ownerField"]=="preserved");
    doc["enabled"]=false;doc["openOnGameStart"]=true;ConfigFiles::Write(file,doc.dump());HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart());
    ConfigFiles::Write(file,"invalid");HelpySettings::Configure(root);assert(!HelpySettings::OpenOnGameStart()&&!HelpySettings::SetOpenOnGameStart(true));assert(ConfigFiles::Read(file)=="invalid");
    fs::remove_all(root);
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

