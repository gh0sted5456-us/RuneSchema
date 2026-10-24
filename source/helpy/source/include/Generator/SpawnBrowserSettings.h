#pragma once
#include "Core/SpawnBrowserPreferences.h"
#include "Generator/PlayerPortraitPicker.h"
namespace PS::SpawnBrowserSettings {
using ChoicesType=SpawnBrowserPreferences::Choices;
struct Stored {ChoicesType Choices;std::string Baseline;};
inline ChoicesType Choices;
inline std::future<Stored> Load;
inline std::future<std::string> Save,Export;
inline std::future<ChoicesType> Import;
inline bool Initialized=false,Loaded=false,Dirty=false,SaveFailed=false;
inline uint64_t Revision=1,FilterRevision=1;
inline std::string Error,Notice,Baseline;
inline std::filesystem::path File(){return RuneSchemaProfile::Ready?RuneSchemaProfile::Data("spawn-browser.json"):std::filesystem::path{};}
inline ChoicesType ReadPreset(const std::filesystem::path& file){HelpyIconPack::Safe(file);return ChoicesType::Parse(HelpyIconPack::Json(ConfigFiles::Read(file,4*1024*1024)));}
inline Stored ReadStored(const std::filesystem::path& file,const std::filesystem::path& legacy){
    HelpyIconPack::Safe(file);RuneSchemaProfile::FileLock guard(file);
    if(!std::filesystem::exists(file)&&!legacy.empty()&&std::filesystem::exists(legacy)){(void)ReadPreset(legacy);RuneSchemaProfile::CopyNew(legacy,file,4*1024*1024);}
    const bool exists=std::filesystem::exists(file);const auto text=exists?ConfigFiles::Read(file,4*1024*1024):std::string{};
    return {exists?ChoicesType::Parse(HelpyIconPack::Json(text)):ChoicesType{},text};
}
inline void WriteStored(const std::filesystem::path& file,const std::string& text,const std::string& expected){
    HelpyIconPack::Safe(file);RuneSchemaProfile::FileLock guard(file);
    const auto current=std::filesystem::exists(file)?ConfigFiles::Read(file,4*1024*1024):std::string{};
    if(current!=expected)throw std::runtime_error("Browser choices changed in another window. Export your preset before restarting Helpy; the other copy was retained.");
    HelpyIconPack::Publish(file,{text.begin(),text.end()});
}
inline void ApplyPreset(const ChoicesType& preset){Choices=Choices.Merge(preset);Dirty=true;SaveFailed=false;Error.clear();++Revision;++FilterRevision;Notice="Preset merged: hidden entries kept; browser filters applied. Placement settings unchanged.";}
inline void Pump(){
    if(!Initialized&&RuneSchemaProfile::Ready){Initialized=true;const auto file=File(),legacy=HostServices::SavedDirectory()/"spawn-browser.json";Load=std::async(std::launch::async,[file,legacy]{return ReadStored(file,legacy);});}
    if(Load.valid()&&Load.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto stored=Load.get();Choices=std::move(stored.Choices);Baseline=std::move(stored.Baseline);Loaded=true;Error.clear();++Revision;++FilterRevision;}catch(const std::exception& e){Error=e.what();}
    }
    if(Save.valid()&&Save.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Baseline=Save.get();SaveFailed=false;}catch(const std::exception& e){Error=e.what();SaveFailed=true;Dirty=true;}}
    if(Import.valid()&&Import.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{ApplyPreset(Import.get());}catch(const std::exception& e){Error=e.what();}}
    if(Export.valid()&&Export.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{Notice=Export.get();}catch(const std::exception& e){Error=e.what();}}
    if(Loaded&&Dirty&&!SaveFailed&&!Save.valid()&&!File().empty()){
        Dirty=false;const auto file=File();const auto text=Choices.Json().dump(2),expected=Baseline;
        Save=std::async(std::launch::async,[file,text,expected]{WriteStored(file,text,expected);return text;});
    }
}
inline bool Set(const nlohmann::json& row,bool hidden){
    if(!Loaded)return false;
    if(!Choices.Set(row,hidden)){Error="Cannot hide this entry: missing stable identity or the 1,024-entry limit was reached.";return false;}
    Dirty=true;SaveFailed=false;Error.clear();++Revision;return true;
}
inline void SetFilter(const std::string& kind,const ChoicesType::Browser& filter){if(!Loaded)return;try{if(Choices.SetFilter(kind,filter)){Dirty=true;SaveFailed=false;}}catch(const std::exception& e){Error=e.what();}}
inline void Retry(){if(Loaded){SaveFailed=false;Dirty=true;}else Initialized=false;}
inline void PickPreset(bool importing){
    wchar_t name[32768]{};if(!importing)std::wcscpy(name,L"spawn-browser.rs-spawns.json");const auto folder=HostServices::ProfileDirectory().wstring();
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=name;dialog.nMaxFile=_countof(name);dialog.lpstrInitialDir=folder.c_str();
    dialog.lpstrFilter=L"Spawn browser presets (*.json)\0*.json\0\0";dialog.lpstrDefExt=L"json";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(importing?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(!(importing?GetOpenFileNameW(&dialog):GetSaveFileNameW(&dialog)))return;auto file=std::filesystem::path(name);
    if(importing)Import=std::async(std::launch::async,[file]{return ReadPreset(file);});
    else {const bool replace=SpawnCatalogSearch::Fold(file.filename().string()).ends_with(".rs-spawns.json");if(!replace)file+=".rs-spawns.json";const auto text=Choices.Json().dump(2);
        Export=std::async(std::launch::async,[file,text,replace]{HelpyIconPack::Publish(file,{text.begin(),text.end()},replace);return "Preset exported: "+file.string();});}
}
inline void PresetControls(){
    Pump();ImGui::TextDisabled("%zu hidden entries · browser filters only",Choices.Hidden.size());
    ImGui::BeginDisabled(!Loaded||Import.valid()||Export.valid());if(HelpyImGuiTheme::SoftButton("Export preset..."))PickPreset(false);ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Import & merge..."))PickPreset(true);ImGui::EndDisabled();
    if(!Notice.empty())ImGui::TextWrapped("%s",Notice.c_str());if(!Error.empty()){ImGui::TextWrapped("%s",Error.c_str());if(HelpyImGuiTheme::SoftButton("Retry preferences"))Retry();}
}
inline void Shutdown(){
    if(Load.valid())try{auto stored=Load.get();Choices=std::move(stored.Choices);Baseline=std::move(stored.Baseline);Loaded=true;}catch(...){}
    if(Save.valid())try{Baseline=Save.get();}catch(...){Dirty=true;}
    if(Import.valid())try{ApplyPreset(Import.get());}catch(...){}
    if(Export.valid())try{(void)Export.get();}catch(...){}
    if(Loaded&&Dirty&&!File().empty())try{WriteStored(File(),Choices.Json().dump(2),Baseline);Dirty=false;}catch(...){}
}
}
