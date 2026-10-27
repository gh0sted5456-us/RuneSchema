#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/CacheBundleStore.h"
#include "Core/ItemSource.h"
#include "Generator/HelpyBackgrounds.h"
#include "Runtime/HostServices.h"
#include <commdlg.h>
namespace PS::SourceBanners {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline PS::ProcessExitFuture<std::map<std::string,HelpyBackgrounds::Pixels>> Worker;
inline std::map<std::string,HelpyBackgrounds::Image> Images;
inline std::map<std::string,std::string> Digests;
inline std::set<std::string> Wanted;
inline std::chrono::steady_clock::time_point Next{};
// Presentation-only manifest images for unmounted/disabled mod placards.
// These are never imported as verified source-cache records or item banners.
inline std::map<std::string,fs::path> ModCardFiles;
inline void QueueModCard(const std::string& owner,const fs::path& file){
    const auto key="mod-card:"+owner;
    if(!ModCardFiles.contains(key)||ModCardFiles[key]!=file){ModCardFiles[key]=file;Next={};}
    if(Wanted.insert(key).second)Next={};
}
inline std::string Status;
inline std::string DropOwner,ArtFile;
inline int DropFrame=-100;
inline bool SavingArt=false;
inline bool Busy(){return Worker.valid()&&SavingArt;}
inline HelpyBackgrounds::Pixels Thumbnail(HelpyBackgrounds::Pixels input){
    if(!input.Error.empty())throw std::runtime_error(input.Error);
    if(!input.Width||!input.Height||input.Data.size()!=size_t(input.Width)*input.Height*4)throw std::runtime_error("Invalid banner image");
    HelpyBackgrounds::Pixels out{SourceBanner::Width,SourceBanner::Height};out.Data.resize(out.Width*out.Height*4);
    const double scale=std::max(double(out.Width)/input.Width,double(out.Height)/input.Height);
    const double left=(input.Width-out.Width/scale)*.5,top=(input.Height-out.Height/scale)*.5;
    for(unsigned y=0;y<out.Height;++y)for(unsigned x=0;x<out.Width;++x){
        const auto sx=std::min(input.Width-1,unsigned(left+(x+.5)/scale)),sy=std::min(input.Height-1,unsigned(top+(y+.5)/scale));
        std::copy_n(input.Data.data()+(size_t(sy)*input.Width+sx)*4,4,out.Data.data()+(size_t(y)*out.Width+x)*4);
    }return out;
}
inline std::string Path(const std::string& owner,const HelpySourceCache::Snapshot& snapshot){
    // Resolve a presentation identity inside a source actually owned by this
    // active pack. Never attribute a mod's borrowed /Game dependency to it.
    for(const auto& [source,key]:snapshot.SourceKeys){const auto path="/"+source+"/__Helpy__/SourceBanner_"+HelpySourceCache::Hex(HelpySourceCache::Hash(owner))+".SourceBanner";
        CacheBundle::Record row{"art",owner,source,path,"",Json::object()};if(CacheBundleStore::ActiveOwner(row,snapshot))return path;
    }return {};
}
inline CacheBundle::Record Record(const std::string& owner,const std::string& path,const HelpyBackgrounds::Pixels& pixels){
    return CacheBundle::Parse({{"Kind","art"},{"Owner",owner},{"Path",path},{"Data",{{"Name","Source banner"},{"SourceBannerPixels",SourceBanner::Encode(pixels.Data)}}}});
}
inline void EnsureBuiltins(const fs::path& cache,const fs::path& root,const HelpySourceCache::Snapshot& snapshot,const fs::path& assets,const std::function<HelpyBackgrounds::Pixels(const fs::path&)>& decoder){
    if(!decoder)return;std::set<std::string> existing;for(const auto& row:CacheBundleStore::Read(cache,root,snapshot,CacheBundleStore::Locale(),"art"))if(row.at("Data").contains("SourceBannerPixels"))existing.insert(row.at("Owner").get<std::string>());
    const auto choices=ContentSource::Choices(CacheBundleStore::Plan(snapshot));std::vector<CacheBundle::Record> records;
    for(const auto& owner:choices){if(existing.contains(owner))continue;const auto name=SourceBanner::Builtin(owner);if(!name[0])continue;const auto path=Path(owner,snapshot);if(path.empty()||!fs::is_regular_file(assets/name))continue;
        records.push_back(Record(owner,path,Thumbnail(decoder(assets/name))));
    }if(!records.empty())CacheBundleStore::Commit(cache,root,snapshot,CacheBundleStore::Locale(),records);
}
inline std::string Owner(const Json& row){
    return SourceBanner::OwnerForItem(row);
}
inline void DrawKey(const std::string& owner,ImVec2 a,ImVec2 b,unsigned char opacity=43){
    if(owner.empty())return;if(Wanted.insert(owner).second)Next={};const auto found=Images.find(owner);if(found==Images.end()||!found->second.Texture)return;
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,b,true);const auto crop=BackgroundPolicy::Cover(256,144,b.x-a.x,b.y-a.y);
    draw->AddImage(ImTextureRef(found->second.Texture),a,b,{crop.Left,crop.Top},{crop.Right,crop.Bottom},IM_COL32(255,255,255,opacity));
    draw->AddRectFilledMultiColor(a,b,IM_COL32(0,0,0,35),IM_COL32(0,0,0,35),IM_COL32(0,0,0,150),IM_COL32(0,0,0,150));draw->PopClipRect();
}
inline void Draw(const Json& row,ImVec2 a,ImVec2 b){
    const auto owner=Owner(row);const auto found=Images.find(owner);
    if(owner.empty()||found==Images.end()||!found->second.Texture)DrawKey("ui:home-banner.png",a,b);
    DrawKey(owner,a,b);
}
inline void DetailsBackground(const Json& row){const auto a=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();Draw(row,a,{a.x+size.x,a.y+size.y});}
inline void DrawCard(const std::string& choice,ImVec2 a,ImVec2 b){if(choice.starts_with("source:"))DrawKey(choice.substr(7),a,b,80);else if(choice.starts_with("background:")||choice=="ui:home-banner.png")DrawKey(choice,a,b,80);}
inline void Pump(){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto pixels=Worker.get();if(HelpyBackgrounds::Upload)for(auto& [owner,image]:pixels){const auto digest=CacheBundleIO::Hash(std::string_view(reinterpret_cast<const char*>(image.Data.data()),image.Data.size()));if(Digests[owner]==digest)continue;if(auto* texture=HelpyBackgrounds::Upload(image)){if(Images[owner].Texture)Images[owner].Texture->Release();Images[owner]={texture,image.Width,image.Height};Digests[owner]=digest;}}if(SavingArt)Status="Artwork ready. It will be included with this source's exported cache.";}catch(const std::exception& e){Status=e.what();}SavingArt=false;}
    if(Worker.valid()||std::chrono::steady_clock::now()<Next||!HelpyBackgrounds::Decode)return;
    Next=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    const bool cacheReady=HelpySourceCache::Ready()&&HelpySourceCache::Current.Authoritative;
    const auto snapshot=HelpySourceCache::Current;const auto cache=HelpySourceCache::Directory(false),root=HostServices::ModDirectory(),assets=HelpyPaths::Asset("source-base-game.png").parent_path();const auto decoder=HelpyBackgrounds::Decode;
    const auto wanted=Wanted;const auto modCards=ModCardFiles;std::set<std::string> loaded;for(const auto& [key,image]:Images)loaded.insert(key);
    Worker=std::async(std::launch::async,[snapshot,cache,root,assets,decoder,wanted,loaded,cacheReady,modCards]{std::map<std::string,HelpyBackgrounds::Pixels> result;
        if(cacheReady){EnsureBuiltins(cache,root,snapshot,assets,decoder);
            for(const auto& record:CacheBundleStore::Read(cache,root,snapshot,CacheBundleStore::Locale(),"art")){const auto& data=record.at("Data");if(!data.contains("SourceBannerPixels"))continue;if(result.size()>=64)break;result[record.at("Owner").get<std::string>()]={256,144,SourceBanner::Decode(data["SourceBannerPixels"]),{}};}}
        for(const auto& key:wanted){if(loaded.contains(key)||result.contains(key))continue;fs::path file;
            if(key.starts_with("mod-card:")&&modCards.contains(key)){
                file=modCards.at(key);bool safe=true;
                for(auto current=fs::absolute(file);!current.empty();){const auto attrs=GetFileAttributesW(current.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){safe=false;break;}const auto parent=current.parent_path();if(parent==current)break;current=parent;}
                std::error_code error;if(!safe||!fs::is_regular_file(file,error)||error||fs::file_size(file,error)>20*1024*1024||error)continue;
            }
            else if(key=="ui:home-banner.png")file=HelpyPaths::Asset("home-banner.png");
            else if(const auto* builtin=SourceBanner::Builtin(key);builtin[0])file=HelpyPaths::Asset(builtin);
            else if(key.starts_with("background:")&&BackgroundPolicy::Filename(key.substr(11)))file=HelpyBackgrounds::Resolve(key.substr(11));else continue;
            try{result[key]=Thumbnail(decoder(file));}catch(...){}if(result.size()>=64)break;}return result;});
}
inline void SetArtFile(const std::string& owner,const fs::path& selected){
    if(owner.empty()||!ContentSource::Valid(owner)){Status="Select one registered source first.";return;}
    if(Worker.valid()){Status="Banner cache is updating; retry shortly.";return;}if(!HelpySourceCache::Ready()||!HelpyBackgrounds::Decode)return;
    const auto snapshot=HelpySourceCache::Current;const auto cache=HelpySourceCache::Directory(false),root=HostServices::ModDirectory();const auto decoder=HelpyBackgrounds::Decode;const auto path=Path(owner,snapshot);if(path.empty()){Status="This source needs an active owned namespace before art can be exported. Its banner association is retained; borrowed native assets are not reattributed.";return;}
    ArtFile=selected.filename().string();SavingArt=true;Worker=std::async(std::launch::async,[snapshot,cache,root,decoder,selected,owner,path]{const auto image=Thumbnail(decoder(selected));CacheBundleStore::Commit(cache,root,snapshot,CacheBundleStore::Locale(),{Record(owner,path,image)});return std::map<std::string,HelpyBackgrounds::Pixels>{{owner,image}};});Next={};Status="Preparing source artwork…";
}
inline void Choose(const std::string& owner){
    if(Worker.valid()){Status="Banner cache is updating; retry shortly.";return;}
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Banner image\0*.png;*.jpg;*.jpeg;*.webp\0\0";dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&dialog))SetArtFile(owner,fs::path(file));
}
inline void ExportArt(const std::string& owner,bool disabled){
    if(owner.empty()){ImGui::TextDisabled("Choose one source to attach its artwork.");return;}
    ImGui::SeparatorText("Source artwork");DropOwner=disabled?"":owner;DropFrame=ImGui::GetFrameCount();
    const auto at=ImGui::GetCursorScreenPos();const float width=std::max(1.f,ImGui::GetContentRegionAvail().x),height=HelpyImGuiTheme::Current.Y(82);
    ImGui::BeginDisabled(disabled||Worker.valid());const bool browse=ImGui::InvisibleButton("##source-art-drop",{width,height});const bool hovered=ImGui::IsItemHovered();ImGui::EndDisabled();const ImVec2 end(at.x+width,at.y+height);auto* draw=ImGui::GetWindowDrawList();const auto& theme=HelpyImGuiTheme::Current;draw->AddRectFilled(at,end,ImGui::GetColorU32(theme.panelAlt),4);DrawKey(owner,at,end);draw->AddRect(at,end,ImGui::GetColorU32(hovered?theme.accent:theme.border),4);const auto label="Drop image or click to browse";const auto text=ImGui::CalcTextSize(label);draw->AddText({at.x+std::max(0.f,(width-text.x)*.5f),at.y+(height-text.y)*.5f},ImGui::GetColorU32(theme.text),label);if(browse)Choose(owner);
    ImGui::TextWrapped("Recommended: 1280 × 720 (16:9), PNG/JPEG. Up to 20 MiB / 4096 × 4096. Cache exports include a 256 × 144 preview, not the original file or its location.");
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
}
inline void Shutdown(){if(Worker.valid())try{Worker.get();}catch(...){}for(auto& [owner,image]:Images)if(image.Texture)image.Texture->Release();Images.clear();Digests.clear();Wanted.clear();Next={};}
}
