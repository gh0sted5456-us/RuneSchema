#pragma once
#include "Core/SpawnBrowserPreferences.h"
#include "Generator/PlayerPortraitPicker.h"
namespace PS::SpawnBrowserSettings {
inline SpawnBrowserPreferences::Choices Choices;
inline std::future<SpawnBrowserPreferences::Choices> Load;
inline std::future<std::string> Save;
inline bool Initialized=false,Loaded=false,Dirty=false,SaveFailed=false;
inline uint64_t Revision=1;
inline std::string Error;
inline std::filesystem::path File(){return HostServices::SavedDirectory()/"spawn-browser.json";}
inline void Pump(){
    if(!Initialized){Initialized=true;const auto file=File();Load=std::async(std::launch::async,[file]{
        HelpyIconPack::Safe(file);
        if(!std::filesystem::exists(file))return SpawnBrowserPreferences::Choices{};
        if(!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)>4*1024*1024)throw std::runtime_error("Spawn preferences exceed file limits; existing file retained");
        std::ifstream input(file);return SpawnBrowserPreferences::Choices::Parse(nlohmann::json::parse(input));
    });}
    if(Load.valid()&&Load.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{Choices=Load.get();Loaded=true;Error.clear();++Revision;}catch(const std::exception& e){Error=e.what();}
    }
    if(Save.valid()&&Save.wait_for(std::chrono::seconds(0))==std::future_status::ready){Error=Save.get();SaveFailed=!Error.empty();if(SaveFailed)Dirty=true;}
    if(Loaded&&Dirty&&!SaveFailed&&!Save.valid()){
        Dirty=false;const auto file=File();const auto text=Choices.Json().dump(2);
        Save=std::async(std::launch::async,[file,text]{try{HelpyIconPack::Publish(file,{text.begin(),text.end()});return std::string{};}catch(const std::exception& e){return std::string("Hidden-result preferences could not be saved: ")+e.what();}});
    }
}
inline bool Set(const nlohmann::json& row,bool hidden){
    if(!Loaded)return false;
    if(!Choices.Set(row,hidden)){Error="Cannot hide this entry: missing stable identity or the 1,024-entry limit was reached.";return false;}
    Dirty=true;SaveFailed=false;Error.clear();++Revision;return true;
}
inline void Retry(){if(Loaded){SaveFailed=false;Dirty=true;}else Initialized=false;}
inline void Shutdown(){
    if(Load.valid())try{Choices=Load.get();Loaded=true;}catch(...){}
    if(Save.valid())try{if(!Save.get().empty())Dirty=true;}catch(...){Dirty=true;}
    if(Loaded&&Dirty)try{const auto text=Choices.Json().dump(2);HelpyIconPack::Publish(File(),{text.begin(),text.end()});Dirty=false;}catch(...){}
}
}
