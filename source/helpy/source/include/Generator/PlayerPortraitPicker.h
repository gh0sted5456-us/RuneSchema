#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/PlayerPortraits.h"
#include "Core/RuneSchemaProfile.h"
#include "Core/ItemPresentation.h"
#include "Generator/ItemIconCache.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HelpyWikiIcons.h"
#include "Generator/LazyCatalog.h"
#include "Generator/NativeJournalCache.h"
#include "Generator/PortraitArt.h"
#include "Generator/HelpyBackgrounds.h"
#include <commdlg.h>
#include <future>
namespace PS::PlayerPortraitPicker {
inline HelpyBackgrounds::Pixels Draft;
inline ID3D11ShaderResourceView* DraftTexture=nullptr;
inline PlayerPortraits::Crop Crop;
inline PS::ProcessExitFuture<HelpyBackgrounds::Pixels> ImportWorker;
inline PS::ProcessExitFuture<std::string> CropWorker;
inline std::string CropPlayer,DiskPlayer;
inline uint64_t PictureRevision=0,DiskRevision=0;
inline PS::ProcessExitFuture<HelpyBackgrounds::Pixels> DiskWorker;
struct Picture {ID3D11ShaderResourceView* Texture=nullptr;int Frame=0;};
inline std::map<std::string,Picture> Pictures;
inline std::set<std::string> Missing;
inline ImVec2 DropMin,DropMax;
inline int DropFrame=-10;
inline std::vector<std::pair<ID3D11ShaderResourceView*,int>> Retired;
struct Listing {std::vector<std::string> Icons;std::string Error;};
inline PlayerPortraits::Choices Choices;
inline PS::ProcessExitFuture<PlayerPortraits::Choices> Load;
inline PS::ProcessExitFuture<Listing> Scan;
inline PS::ProcessExitFuture<std::string> Save;
inline std::vector<std::string> Icons,Filtered;
inline std::string Player,Name,Error,LastFilter;
inline std::map<std::string,std::string> Labels;
inline uint64_t JournalRevision=0;
inline char Search[129]{};
inline bool Initialized=false,Loaded=false,SaveWanted=false,OpenWanted=false;
inline std::atomic<bool> CancelScan=false;
inline std::filesystem::path File(){return RuneSchemaProfile::Ready?RuneSchemaProfile::Data("portraits.json"):HostServices::SavedDirectory()/"player-portraits.json";}
inline std::filesystem::path PortraitFile(const std::string& guid){const auto current=File().parent_path()/"portraits"/(guid+".rsicon");return current;}
inline void ClearDraft(){if(DraftTexture)Retired.emplace_back(DraftTexture,ImGui::GetFrameCount());DraftTexture=nullptr;Draft={};Crop={};}
inline void Import(const std::filesystem::path& file){
    if(ImportWorker.valid()||CropWorker.valid()){Error="Finish the current portrait first.";return;}
    if(!HelpyBackgrounds::Decode){Error="Image decoder unavailable.";return;}ClearDraft();Error.clear();const auto decode=HelpyBackgrounds::Decode;
    ImportWorker=std::async(std::launch::async,[file,decode]{try{HelpyIconPack::Safe(file);return decode(file);}catch(const std::exception& e){return HelpyBackgrounds::Pixels{0,0,{},e.what()};}});
}
inline bool CanDrop(ImVec2 point){return ImGui::GetCurrentContext()&&ImGui::GetFrameCount()-DropFrame<=2&&point.x>=DropMin.x&&point.y>=DropMin.y&&point.x<=DropMax.x&&point.y<=DropMax.y;}
inline void ChooseFile(){wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.webp)\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&dialog))Import(file);}
inline void SaveCrop(){
    if(!Loaded||!DraftTexture||CropWorker.valid()||Player.empty())return;
    if(Choices.Icons.size()>=256&&!Choices.Icons.contains(Player)){Error="Portrait preference limit reached.";return;}
    ++PictureRevision;
    CropPlayer=Player;const auto file=PortraitFile(CropPlayer);const auto pixels=PlayerPortraits::Cropped(Draft.Data,Draft.Width,Draft.Height,Crop);
    CropWorker=std::async(std::launch::async,[file,pixels]{try{if(pixels.empty())throw std::runtime_error("Invalid image dimensions");std::vector<char> bytes(16+pixels.size());std::memcpy(bytes.data(),"RSI3",4);const uint32_t header[]{256,256,uint32_t(pixels.size())};std::memcpy(bytes.data()+4,header,12);std::memcpy(bytes.data()+16,pixels.data(),pixels.size());HelpyIconPack::Publish(file,bytes);return std::string{};}catch(const std::exception& e){return std::string("Portrait save failed: ")+e.what();}});
}
inline void ReleaseTextures(){ClearDraft();for(auto& [id,picture]:Pictures)if(picture.Texture)picture.Texture->Release();Pictures.clear();for(auto [texture,frame]:Retired)texture->Release();Retired.clear();Missing.clear();}
inline void Pump(){
    const auto frame=ImGui::GetFrameCount();
    for(auto it=Retired.begin();it!=Retired.end();)if(it->second<frame){it->first->Release();it=Retired.erase(it);}else ++it;
    if(ImportWorker.valid()&&ImportWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){Draft=ImportWorker.get();Error=Draft.Error;if(Error.empty()&&HelpyBackgrounds::Upload){DraftTexture=HelpyBackgrounds::Upload(Draft);if(!DraftTexture)Error="Could not display the image.";}}
    if(CropWorker.valid()&&CropWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto error=CropWorker.get();if(error.empty()){
        if(const auto it=Pictures.find(CropPlayer);it!=Pictures.end()){Retired.emplace_back(it->second.Texture,frame);Pictures.erase(it);}Missing.erase(CropPlayer);
        if(Choices.Set(CropPlayer,"/HelpyPortrait/"+CropPlayer+"."+CropPlayer))SaveWanted=true;else Error="Portrait preference limit reached.";
        if(Player==CropPlayer)ClearDraft();
    }else Error=error;}
    if(DiskWorker.valid()&&DiskWorker.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto pixels=DiskWorker.get();if(DiskRevision==PictureRevision){auto* texture=pixels.Error.empty()&&HelpyBackgrounds::Upload?HelpyBackgrounds::Upload(pixels):nullptr;if(texture)Pictures[DiskPlayer]={texture,frame};else Missing.insert(DiskPlayer);}DiskPlayer.clear();}
    while(Pictures.size()>32){auto oldest=Pictures.end();for(auto it=Pictures.begin();it!=Pictures.end();++it)if(it->second.Frame<frame&&(oldest==Pictures.end()||it->second.Frame<oldest->second.Frame))oldest=it;if(oldest==Pictures.end())break;oldest->second.Texture->Release();Pictures.erase(oldest);}
    if(!Initialized){Initialized=true;const auto file=File();Load=std::async(std::launch::async,[file]{try{HelpyIconPack::Safe(file);if(std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=1024*1024){std::ifstream input(file);return PlayerPortraits::Choices::Parse(nlohmann::json::parse(input));}}catch(...){}return PlayerPortraits::Choices{};});}
    if(Load.valid()&&Load.wait_for(std::chrono::seconds(0))==std::future_status::ready){Choices=Load.get();Loaded=true;}
    if(Scan.valid()&&Scan.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=Scan.get();std::set<std::string> merged(Icons.begin(),Icons.end());merged.insert(result.Icons.begin(),result.Icons.end());Icons.assign(merged.begin(),merged.end());Error=result.Error;LastFilter="\1";}
    if(Save.valid()&&Save.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto error=Save.get();if(!error.empty())Error=error;}
    if(SaveWanted&&!Save.valid()&&Loaded){SaveWanted=false;const auto file=File();const auto text=Choices.Json().dump(2);
        Save=std::async(std::launch::async,[file,text]{try{HelpyIconPack::Publish(file,{text.begin(),text.end()});return std::string{};}catch(const std::exception& e){return std::string("Portrait save failed: ")+e.what();}});}
}
inline void Refresh(){
    if(Scan.valid())return;CancelScan=false;const auto sources=HelpySourceCache::Current;const auto wiki=HelpyWikiIcons::Portraits();
    std::set<std::string> seed(wiki.begin(),wiki.end());Labels.clear();
    for(const auto& glyph:PortraitGlyphs::All){const auto icon=std::string("/HelpyGlyph/")+glyph.Id+"."+glyph.Id;seed.insert(icon);Labels[icon]=glyph.Label;}
    const auto add=[&](const nlohmann::json& rows){if(!rows.is_array())return;for(const auto& row:rows){const auto icon=ItemPresentation::Text(row,"Icon");if(PlayerPortraits::Icon(icon)){seed.insert(icon);Labels[icon]=ItemPresentation::Text(row,"Name");}}};
    add(LazyCatalog::For(0).Rows);add(NativeJournalCache::Entries);add(NativeJournalCache::BundleArt);
    for(const auto& [icon,texture]:ItemIconCache::detail::State().Textures)if(texture&&PlayerPortraits::Icon(icon))seed.insert(icon);
    Icons.assign(seed.begin(),seed.end());LastFilter="\1";
    Scan=std::async(std::launch::async,[sources,seed]{Listing result;std::set<std::string> icons=seed;size_t inspected=0;
        try{for(const bool live:{false,true}){const auto base=HelpySourceCache::Directory(live);HelpyIconPack::Safe(base);std::error_code ec;if(!std::filesystem::is_directory(base,ec))continue;
            for(std::filesystem::recursive_directory_iterator it(base,ec),end;!ec&&it!=end;it.increment(ec)){
                if(CancelScan.load())return result;
                if(++inspected>150000){result.Error="Cache listing reached its safety limit.";break;}
                const auto attrs=GetFileAttributesW(it->path().c_str());if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){it.disable_recursion_pending();continue;}
                if(it->path().extension()!=L".json"||!it->is_regular_file(ec)||it->file_size(ec)>8192)continue;
                try{std::ifstream input(it->path());const auto document=nlohmann::json::parse(input);const auto icon=document.value("Icon",std::string{});if(!PlayerPortraits::Icon(icon))continue;
                    auto pixels=it->path();pixels.replace_extension();const auto expected=sources.Authoritative?HelpyIconPack::Target(base,sources,icon):base/(HelpySourceCache::Hex(HelpySourceCache::Hash(sources.Key+"|"+icon))+".rsicon");
                    if(pixels.lexically_normal()!=expected.lexically_normal())continue;
                    int width=0,height=0;std::vector<unsigned char> bytes;if(ItemIconCache::detail::ReadThumbnail(pixels,width,height,bytes))icons.insert(icon);
                }catch(...){}if(icons.size()>=32768)break;
            }if(inspected>150000)break;
        }}catch(const std::exception& e){result.Error=e.what();}
        result.Icons.assign(icons.begin(),icons.end());return result;});
}
inline std::string PictureId(const nlohmann::json& row){return row.value("IsLocal",false)&&RuneSchemaProfile::Ready?RuneSchemaProfile::Id():row.value("Guid",std::string{});}
inline std::string Icon(const nlohmann::json& row){auto saved=Choices.Get(PictureId(row));if(saved.empty())saved=Choices.Get(row.value("Guid",std::string{}));return saved.empty()?row.value("Icon",std::string{}):saved;}
inline void Request(const nlohmann::json& row){Pump();if(ImportWorker.valid()||CropWorker.valid())return;ClearDraft();Player=PlayerPortraits::Identity(PictureId(row));Name=row.value("Name",std::string{});if(Player.empty())return;OpenWanted=true;Search[0]=0;Error.clear();Refresh();}
inline bool DrawCached(const std::string& icon,ImVec2 a,ImVec2 b,ImU32 tint=0){
    if(icon.empty()||!ImGui::IsRectVisible(a,b))return false;
    const bool colored=tint!=0;if(!colored)tint=IM_COL32_WHITE;
    if(icon.starts_with("/HelpyPortrait/")){
        const auto guid=PlayerPortraits::Identity(icon.substr(15,32));if(guid.empty()||icon!="/HelpyPortrait/"+guid+"."+guid)return false;
        if(auto it=Pictures.find(guid);it!=Pictures.end()){it->second.Frame=ImGui::GetFrameCount();ImGui::GetWindowDrawList()->AddImageRounded(ImTextureRef(it->second.Texture),a,b,{0,0},{1,1},tint,(b.x-a.x)*.5f);return true;}
        if(!DiskWorker.valid()&&!Missing.contains(guid)){DiskPlayer=guid;DiskRevision=PictureRevision;auto file=PortraitFile(guid);if(!std::filesystem::exists(file)){const auto legacy=HostServices::SavedDirectory()/"portraits"/(guid+".rsicon");if(std::filesystem::exists(legacy))file=legacy;}DiskWorker=std::async(std::launch::async,[file]{HelpyBackgrounds::Pixels result;try{HelpyIconPack::Safe(file);int w=0,h=0;if(!ItemIconCache::detail::ReadThumbnail(file,w,h,result.Data))throw std::runtime_error("Portrait unavailable");result.Width=w;result.Height=h;}catch(const std::exception& e){result.Error=e.what();}return result;});}return false;
    }
    if(const auto kind=PortraitArt::Kind(icon);kind>=0){const auto color=colored?tint:ImGui::GetColorU32(HelpyImGuiTheme::Current.accentHover);auto* draw=ImGui::GetWindowDrawList();draw->AddCircleFilled({(a.x+b.x)*.5f,(a.y+b.y)*.5f},std::min(b.x-a.x,b.y-a.y)*.5f,ImGui::GetColorU32(HelpyImGuiTheme::Current.panelAlt));draw->AddCircle({(a.x+b.x)*.5f,(a.y+b.y)*.5f},std::min(b.x-a.x,b.y-a.y)*.5f,color);PortraitArt::Draw(kind,{a.x+(b.x-a.x)*.1f,a.y+(b.y-a.y)*.1f},{b.x-(b.x-a.x)*.1f,b.y-(b.y-a.y)*.1f},color);return true;}
    if(icon.starts_with("/HelpyWiki/"))return HelpyWikiIcons::Draw(icon,a,b,tint,true);
    // Never queue native capture from a portrait or its picker.
    ItemIconCache::detail::TryInstallDisk(icon);auto* texture=ItemIconCache::Get(icon);if(!texture)return false;
    ItemIconCache::detail::State().WantedFrame[icon]=ImGui::GetFrameCount();
    if(texture->Status!=ImTextureStatus_OK)return false;ImGui::GetWindowDrawList()->AddImageRounded(texture->GetTexRef(),a,b,{0,0},{1,1},tint,std::min(b.x-a.x,b.y-a.y)*.5f);return true;
}
inline void Render(){
    Pump();if(OpenWanted){ImGui::OpenPopup("Player portrait");OpenWanted=false;}const auto& t=HelpyImGuiTheme::Current;
    const auto area=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(t.X(620),area.x-t.X(24)),std::min(t.Y(500),area.y-t.Y(24))},ImGuiCond_Appearing);
    if(!HelpyImGuiTheme::BeginCenteredModal("Player portrait",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    NativeJournalCache::Poll();if(JournalRevision!=NativeJournalCache::Revision){
        JournalRevision=NativeJournalCache::Revision;std::set<std::string> merged(Icons.begin(),Icons.end());
        const auto add=[&](const nlohmann::json& rows){for(const auto& row:rows){const auto icon=ItemPresentation::Text(row,"Icon");if(PlayerPortraits::Icon(icon)){merged.insert(icon);Labels[icon]=ItemPresentation::Text(row,"Name");}}};
        add(NativeJournalCache::Entries);add(NativeJournalCache::BundleArt);Icons.assign(merged.begin(),merged.end());LastFilter="\1";
    }
    DropMin=ImGui::GetWindowPos();const auto size=ImGui::GetWindowSize();DropMax={DropMin.x+size.x,DropMin.y+size.y};DropFrame=ImGui::GetFrameCount();
    ImGui::Text("Portrait · %s",Name.c_str());ImGui::TextDisabled("Drop an image here, or choose a cached icon. Local to this character.");
    ImGui::BeginDisabled(ImportWorker.valid()||CropWorker.valid());if(HelpyImGuiTheme::SoftButton("Choose image…"))ChooseFile();ImGui::EndDisabled();
    if(ImportWorker.valid())ImGui::TextDisabled("Reading image…");
    if(DraftTexture){
        const float side=std::min(t.Y(240),ImGui::GetContentRegionAvail().x);const auto a=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##face-crop",{side,side});
        const auto bounds=PlayerPortraits::Frame(Draft.Width,Draft.Height,Crop);
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){const auto delta=ImGui::GetIO().MouseDelta;Crop.X-=delta.x/side*(bounds.Right-bounds.Left);Crop.Y-=delta.y/side*(bounds.Bottom-bounds.Top);const auto limited=PlayerPortraits::Frame(Draft.Width,Draft.Height,Crop);Crop.X=(limited.Left+limited.Right)*.5f;Crop.Y=(limited.Top+limited.Bottom)*.5f;}
        ImGui::GetWindowDrawList()->AddImage(ImTextureRef(DraftTexture),a,{a.x+side,a.y+side},{bounds.Left,bounds.Top},{bounds.Right,bounds.Bottom});
        ImGui::SameLine();ImGui::Dummy({t.Y(72),t.Y(72)});const auto preview=ImGui::GetItemRectMin();ImGui::GetWindowDrawList()->AddImageRounded(ImTextureRef(DraftTexture),preview,{preview.x+t.Y(72),preview.y+t.Y(72)},{bounds.Left,bounds.Top},{bounds.Right,bounds.Bottom},IM_COL32_WHITE,t.Y(36));
        ImGui::SliderFloat("Zoom",&Crop.Zoom,1,8,"%.1fx");ImGui::TextDisabled("Drag to frame the face. Saves a 256px crop; original unchanged.");
        ImGui::BeginDisabled(!Loaded||CropWorker.valid());if(HelpyImGuiTheme::SoftButton("Save portrait"))SaveCrop();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel crop"))ClearDraft();ImGui::EndDisabled();
        if(CropWorker.valid())ImGui::TextDisabled("Saving portrait…");
        if(!Error.empty())ImGui::TextWrapped("%s",Error.c_str());ImGui::EndPopup();return;
    }
    ImGui::BeginDisabled(!Loaded);if(HelpyImGuiTheme::SoftButton("Use default")){Choices.Set(Player,"");SaveWanted=true;ImGui::CloseCurrentPopup();}ImGui::EndDisabled();ImGui::SameLine();
    if(HelpyImGuiTheme::SoftButton("Refresh cache list"))Refresh();ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Close"))ImGui::CloseCurrentPopup();
    ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##portrait-search","Search icon path / mod...",Search,sizeof(Search));
    if(Scan.valid())ImGui::TextDisabled("Checking disk cache; shared Item Bay and journal artwork is already available.");
    if(!Error.empty())ImGui::TextWrapped("%s",Error.c_str());
    if(LastFilter!=Search){LastFilter=Search;auto query=HelpyCacheSources::Lower(LastFilter);Filtered.clear();for(const auto& icon:Icons)if(HelpyCacheSources::Lower((Labels.contains(icon)?Labels.at(icon):HelpyWikiIcons::Label(icon))+" "+icon).find(query)!=std::string::npos)Filtered.push_back(icon);}
    ImGui::TextDisabled("%zu cached pictures",Filtered.size());
    ImGui::BeginChild("##portrait-icons",{0,0},true);const float side=t.X(64),cell=side+t.X(12);const int columns=std::max(1,int(ImGui::GetContentRegionAvail().x/cell));
    ImGuiListClipper clip;clip.Begin(int((Filtered.size()+columns-1)/columns),side+t.Y(12));
    while(clip.Step())for(int line=clip.DisplayStart;line<clip.DisplayEnd;++line){for(int col=0;col<columns;++col){const size_t i=size_t(line)*columns+col;if(i>=Filtered.size())break;if(col)ImGui::SameLine();ImGui::PushID(int(i));
        const auto a=ImGui::GetCursorScreenPos();const auto& icon=Filtered[i];ImGui::BeginDisabled(!Loaded);
        if(ImGui::Selectable("##cached-portrait",Choices.Get(Player)==icon,0,{side,side})){if(Choices.Set(Player,icon)){SaveWanted=true;ImGui::CloseCurrentPopup();}}
        ImGui::EndDisabled();if(!DrawCached(icon,a,{a.x+side,a.y+side})&&!icon.starts_with("/HelpyWiki/"))ItemIconCache::Draw(icon,a,{a.x+side,a.y+side},true);
        if(ImGui::IsItemHovered())PS::HelpyImGuiTheme::Tooltip("%s",Labels.contains(icon)?Labels.at(icon).c_str():HelpyWikiIcons::Label(icon).c_str());ImGui::PopID();
    }}ImGui::EndChild();ImGui::EndPopup();
}
inline void Shutdown(){
    CancelScan=true;if(Load.valid())try{Choices=Load.get();Loaded=true;}catch(...){}
    if(Save.valid())try{const auto error=Save.get();if(!error.empty())Error=error;}catch(...){}
    if(CropWorker.valid())try{const auto error=CropWorker.get();if(error.empty()){Choices.Set(CropPlayer,"/HelpyPortrait/"+CropPlayer+"."+CropPlayer);SaveWanted=true;}else Error=error;}catch(...){}
    if(SaveWanted&&Loaded)try{const auto text=Choices.Json().dump(2);HelpyIconPack::Publish(File(),{text.begin(),text.end()});SaveWanted=false;}catch(const std::exception& e){Error=e.what();}
    for(auto* future:{&ImportWorker,&DiskWorker})if(future->valid())try{future->get();}catch(...){}if(Scan.valid())try{Scan.get();}catch(...){}ReleaseTextures();
}
}
