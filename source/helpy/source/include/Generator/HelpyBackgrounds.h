#pragma once
#include "Core/BackgroundPolicy.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HelpyPaths.h"
#include "Core/HelpyPreviewHooks.h"
#include <d3d11.h>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <set>
#include <vector>
namespace PS::HelpyBackgrounds {
struct Pixels {unsigned Width=0,Height=0;std::vector<unsigned char> Data;std::string Error;};
struct Image {ID3D11ShaderResourceView* Texture=nullptr;unsigned Width=0,Height=0;};
inline std::function<Pixels(const std::filesystem::path&)> Decode;
inline std::function<ID3D11ShaderResourceView*(const Pixels&)> Upload;
inline std::future<Pixels> Worker;
inline std::string Pending,Selection,Error;
inline std::map<std::string,Image> Images;
inline std::set<std::string> Failed;
inline std::vector<std::string> Files;
inline std::vector<ID3D11ShaderResourceView*> Retired;
inline bool Listed=false,DraftReady=false,Preview=false,SettingsVisible=false;
inline float DraftStrength=.12f;
inline size_t Uploads=0;
inline std::filesystem::path Folder(){return HelpyPaths::PluginRoot()/"assets"/"backgrounds";}
inline std::filesystem::path Resolve(const std::string& name){
    const auto custom=Folder()/std::filesystem::u8path(name);std::error_code error;if(std::filesystem::is_regular_file(custom,error))return custom;
    if(name=="RuneSchema.png")return HelpyPaths::Asset("RuneSchema.png");
    if(name=="Dragonwilds.png"||name=="dragonwilds.png")return HelpyPaths::Asset("dragonwilds.png");
    return custom;
}
inline const std::string& Effective(){return Preview?Selection:HelpyImGuiTheme::Current.backgroundFile;}
inline bool Opaque(){return BackgroundPolicy::Filename(Effective());}
inline void Refresh(){
    Files.clear();Error.clear();Failed.clear();Listed=true;
    try{const auto folder=Folder();
        for(const auto* builtin:{"RuneSchema.png","Dragonwilds.png"})if(std::filesystem::is_regular_file(Resolve(builtin)))Files.push_back(builtin);
        if(!std::filesystem::is_directory(folder))return;
        if(GetFileAttributesW(folder.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Background folder links are not supported.");
        size_t inspected=0;for(const auto& file:std::filesystem::directory_iterator(folder)){
            if(++inspected>512){Error="Folder listing limited to 512 entries.";break;}
            const auto utf8=file.path().filename().u8string();const std::string name(utf8.begin(),utf8.end());
            if(!BackgroundPolicy::Filename(name)||!file.is_regular_file()||(GetFileAttributesW(file.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT))continue;
            if(file.file_size()>20*1024*1024)continue;Files.push_back(name);if(Files.size()==128){Error="Showing the first 128 background images.";break;}
        }std::sort(Files.begin(),Files.end());Files.erase(std::unique(Files.begin(),Files.end()),Files.end());
        for(const auto& [name,image]:Images)if(image.Texture)Retired.push_back(image.Texture);Images.clear();
    }catch(const std::exception& e){Error=e.what();}
}
inline void Pump(){
    HelpyImGuiTheme::PictureBackingActive=Opaque();
    for(auto* texture:Retired)texture->Release();Retired.clear();
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto pixels=Worker.get();
        if(pixels.Error.empty()&&Upload){if(auto* texture=Upload(pixels)){Images[Pending]={texture,pixels.Width,pixels.Height};++Uploads;}else pixels.Error="Could not upload background texture.";}
        if(!pixels.Error.empty()){Failed.insert(Pending);Error=pixels.Error;}Pending.clear();
    }
    const auto active=Effective();const std::set<std::string> wanted{active,SettingsVisible?Selection:std::string{}};
    while(Images.size()>2){const auto old=std::find_if(Images.begin(),Images.end(),[&](const auto& entry){return !wanted.contains(entry.first);});if(old==Images.end())break;if(old->second.Texture)old->second.Texture->Release();Images.erase(old);}
    if(!Worker.valid()&&Decode){for(const auto& name:{active,SettingsVisible?Selection:std::string{}}){
        if(!BackgroundPolicy::Filename(name)||Images.contains(name)||Failed.contains(name))continue;
        Pending=name;const auto file=Resolve(name);const auto decoder=Decode;
        Worker=std::async(std::launch::async,[file,decoder]{try{return decoder(file);}catch(const std::exception& e){return Pixels{0,0,{},e.what()};}catch(...){return Pixels{0,0,{},"Could not decode background."};}});break;
    }}SettingsVisible=false;
}
inline const Image* Find(const std::string& name){const auto found=Images.find(name);return found==Images.end()?nullptr:&found->second;}
inline void DrawWindow(){
    if(!Opaque())return;const auto* image=Find(Effective());
    const auto strength=BackgroundPolicy::Strength(Preview?DraftStrength:HelpyImGuiTheme::Current.backgroundStrength);
    const auto a=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,{a.x+size.x,a.y+size.y},true);
    const auto base=HelpyImGuiTheme::Current.window;
    draw->AddRectFilled(a,{a.x+size.x,a.y+size.y},ImGui::GetColorU32(ImVec4(base.x,base.y,base.z,1)));
    if(image&&image->Texture){
        const auto crop=BackgroundPolicy::Cover(float(image->Width),float(image->Height),size.x,size.y);
        draw->AddImage(ImTextureRef(image->Texture),a,{a.x+size.x,a.y+size.y},{crop.Left,crop.Top},{crop.Right,crop.Bottom},ImGui::GetColorU32(ImVec4(1,1,1,1)));
        draw->AddRectFilled(a,{a.x+size.x,a.y+size.y},ImGui::GetColorU32(ImVec4(base.x,base.y,base.z,1-strength)));
    }draw->PopClipRect();
#if defined(HELPY_PREVIEW)
    ++HelpyPreviewHooks::BackgroundDraws;
#endif
}
inline void Render(){
    SettingsVisible=true;auto& theme=HelpyImGuiTheme::Current;
    if(!DraftReady){DraftReady=true;Selection=theme.backgroundFile;DraftStrength=theme.backgroundStrength;}
    if(!Listed)Refresh();
    ImGui::TextWrapped("Pictures sit behind Helpy's dark tint and block windows underneath. Choose Clear glass to restore RuneSchema's transparent style. Your own artwork stays local.");
    ImGui::SetNextItemWidth(std::min(theme.X(360),ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo("Picture",Selection.empty()?"Clear glass (RuneSchema default)":Selection.c_str())){
        if(ImGui::Selectable("Clear glass (RuneSchema default)",Selection.empty()))Selection.clear();
        for(const auto& name:Files)if(ImGui::Selectable(name.c_str(),Selection==name)){Selection=name;Error.clear();}
        ImGui::EndCombo();
    }
    if(const auto* image=Find(Selection)){
        const auto width=std::min(theme.X(440),ImGui::GetContentRegionAvail().x),height=theme.Y(200);
        const auto scale=std::min(width/image->Width,height/image->Height);
        ImGui::Image(ImTextureRef(image->Texture),{image->Width*scale,image->Height*scale});
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BackgroundPicture=HelpyPreviewHooks::LastRect();
#endif
    }else if(!Selection.empty())ImGui::TextDisabled(Failed.contains(Selection)?"Preview unavailable":"Loading picture preview...");
    ImGui::SliderFloat("Image strength",&DraftStrength,0,.3f,"%.2f");
    ImGui::Checkbox("Preview in Helpy (not saved)",&Preview);
    if(HelpyImGuiTheme::SoftButton("Apply background")){theme.backgroundFile=Selection;theme.backgroundStrength=BackgroundPolicy::Strength(DraftStrength);Preview=false;HelpyImGuiTheme::SaveCurrent();}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel preview")){Preview=false;Selection=theme.backgroundFile;DraftStrength=theme.backgroundStrength;}
    if(HelpyImGuiTheme::SoftButton("Open backgrounds folder")){std::error_code ec;std::filesystem::create_directories(Folder(),ec);if(ec)Error=ec.message();else ShellExecuteW(nullptr,L"open",Folder().c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Refresh pictures"))Refresh();
    if(!Error.empty())ImGui::TextWrapped("%s",Error.c_str());
    ImGui::TextDisabled("Local only. Up to 128 images, 20 MiB each; maximum decoded size 4096 x 4096.");
}
inline void Shutdown(){
    HelpyImGuiTheme::PictureBackingActive=false;
    if(Worker.valid())Worker.get();
    for(auto& [name,image]:Images)if(image.Texture)image.Texture->Release();Images.clear();
    for(auto* texture:Retired)texture->Release();Retired.clear();Decode={};Upload={};Pending.clear();Failed.clear();
}
}
