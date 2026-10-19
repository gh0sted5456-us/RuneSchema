#pragma once
#include "Core/PlayerPortraits.h"
#include "Core/ItemPresentation.h"
#include "Generator/ItemIconCache.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Runtime/HelpyWikiIcons.h"
#include "Generator/LazyCatalog.h"
#include "Generator/NativeJournalCache.h"
#include "Generator/PortraitArt.h"
#include <future>
namespace PS::PlayerPortraitPicker {
struct Listing {std::vector<std::string> Icons;std::string Error;};
inline PlayerPortraits::Choices Choices;
inline std::future<PlayerPortraits::Choices> Load;
inline std::future<Listing> Scan;
inline std::future<std::string> Save;
inline std::vector<std::string> Icons,Filtered;
inline std::string Player,Name,Error,LastFilter;
inline std::map<std::string,std::string> Labels;
inline uint64_t JournalRevision=0;
inline char Search[129]{};
inline bool Initialized=false,Loaded=false,SaveWanted=false,OpenWanted=false;
inline std::atomic<bool> CancelScan=false;
inline std::filesystem::path File(){return HostServices::SavedDirectory()/"player-portraits.json";}
inline void Pump(){
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
inline std::string Icon(const nlohmann::json& row){const auto guid=row.value("Guid",std::string{});const auto saved=Choices.Get(guid);return saved.empty()?row.value("Icon",std::string{}):saved;}
inline void Request(const nlohmann::json& row){Pump();Player=PlayerPortraits::Identity(row.value("Guid",std::string{}));Name=row.value("Name",std::string{});if(Player.empty())return;OpenWanted=true;Search[0]=0;Error.clear();Refresh();}
inline bool DrawCached(const std::string& icon,ImVec2 a,ImVec2 b){
    if(icon.empty()||!ImGui::IsRectVisible(a,b))return false;
    if(const auto kind=PortraitArt::Kind(icon);kind>=0){PortraitArt::Draw(kind,a,b,ImGui::GetColorU32(HelpyImGuiTheme::Current.accentHover));return true;}
    if(icon.starts_with("/HelpyWiki/"))return HelpyWikiIcons::Draw(icon,a,b);
    // Never queue native capture from a portrait or its picker.
    ItemIconCache::detail::TryInstallDisk(icon);auto* texture=ItemIconCache::Get(icon);if(!texture)return false;
    ItemIconCache::detail::State().WantedFrame[icon]=ImGui::GetFrameCount();
    if(texture->Status!=ImTextureStatus_OK)return false;ImGui::GetWindowDrawList()->AddImage(texture->GetTexRef(),a,b);return true;
}
inline void Render(){
    Pump();if(OpenWanted){ImGui::OpenPopup("Player portrait");OpenWanted=false;}const auto& t=HelpyImGuiTheme::Current;
    const auto area=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(t.X(620),area.x-t.X(24)),std::min(t.Y(500),area.y-t.Y(24))},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Player portrait",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    NativeJournalCache::Poll();if(JournalRevision!=NativeJournalCache::Revision){
        JournalRevision=NativeJournalCache::Revision;std::set<std::string> merged(Icons.begin(),Icons.end());
        const auto add=[&](const nlohmann::json& rows){for(const auto& row:rows){const auto icon=ItemPresentation::Text(row,"Icon");if(PlayerPortraits::Icon(icon)){merged.insert(icon);Labels[icon]=ItemPresentation::Text(row,"Name");}}};
        add(NativeJournalCache::Entries);add(NativeJournalCache::BundleArt);Icons.assign(merged.begin(),merged.end());LastFilter="\1";
    }
    ImGui::Text("Portrait for %s",Name.c_str());ImGui::TextWrapped("Choose Item Bay icons, native journal artwork or optional cached Wiki portraits. Saved locally for this character, not everyone with the same name.");
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
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Labels.contains(icon)?Labels.at(icon).c_str():HelpyWikiIcons::Label(icon).c_str());ImGui::PopID();
    }}ImGui::EndChild();ImGui::EndPopup();
}
inline void Shutdown(){CancelScan=true;for(auto* future:{&Save})if(future->valid())try{future->get();}catch(...){}if(Scan.valid())try{Scan.get();}catch(...){}if(Load.valid())try{Load.get();}catch(...){} }
}
