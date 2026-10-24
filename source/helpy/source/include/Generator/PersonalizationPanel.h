#pragma once
#include "Core/HelpyPersonalization.h"
#include "Core/ItemPresentation.h"
#include "Generator/HelpyImGuiTheme.h"
#include <cmath>
namespace PS::PersonalizationPanel {
using Json=nlohmann::json;
inline bool Open=false,Item=false,Favorite=false,Locked=false;
inline std::string Key,Original,Message;
inline char Nickname[193]{},Tags[589]{};
inline void Edit(const std::string& key,const std::string& original,bool item){
    Key=key;Original=original;Item=item;const auto data=HelpyPersonalization::Get(key);
    std::snprintf(Nickname,sizeof(Nickname),"%s",data.value("Nickname",std::string{}).c_str());std::string tags;
    if(data.contains("Tags"))for(const auto& tag:data["Tags"]){if(!tags.empty())tags+=", ";tags+=tag.get<std::string>();}
    std::snprintf(Tags,sizeof(Tags),"%s",tags.c_str());Favorite=data.value("Favorite",false);Locked=data.value("Locked",false);Message.clear();Open=true;
}
inline void Menu(const Json& row){
    if(!ImGui::BeginMenu("Organize"))return;
    const auto key=HelpyPersonalization::TypeKey(row);ImGui::BeginDisabled(key.empty()||row.value("MarksReadOnly",false));
    if(ImGui::MenuItem("Nickname & tags..."))Edit(key,ItemPresentation::OriginalName(row),true);
    const bool favorite=HelpyPersonalization::Favorite(row),locked=HelpyPersonalization::Locked(row),junk=HelpyPersonalization::Tagged(row,"junk");
    const auto type=HelpyPersonalization::Get(key);const bool typeFavorite=type.value("Favorite",false),typeLocked=type.value("Locked",false);
    bool typeJunk=false;for(const auto& tag:type.value("Tags",Json::array()))if(HelpyPersonalization::Fold(tag.get<std::string>())=="junk")typeJunk=true;
    if(ImGui::MenuItem("Favorite item type",nullptr,typeFavorite))HelpyPersonalization::Set(key,{{"Favorite",!typeFavorite}});
    if(ImGui::MenuItem("Lock item type against Helpy removal",nullptr,typeLocked))HelpyPersonalization::Set(key,{{"Locked",!typeLocked}});
    if(ImGui::MenuItem("Junk item type",nullptr,typeJunk)){auto tags=type.value("Tags",Json::array());Json next=Json::array();for(const auto& tag:tags)if(HelpyPersonalization::Fold(tag.get<std::string>())!="junk")next.push_back(tag);if(!typeJunk)next.push_back("junk");HelpyPersonalization::Set(key,{{"Tags",next}});}
    const auto stackKey=HelpyPersonalization::StackKey(row);
    if(!stackKey.empty()&&ImGui::BeginMenu("This stack")){
        if(ImGui::MenuItem("Favorite",nullptr,favorite))HelpyPersonalization::Set(stackKey,{{"Favorite",!favorite}});
        if(ImGui::MenuItem("Junk",nullptr,junk)){auto tags=HelpyPersonalization::Effective(row).value("Tags",Json::array());Json next=Json::array();for(const auto& tag:tags)if(HelpyPersonalization::Fold(tag.get<std::string>())!="junk")next.push_back(tag);if(!junk)next.push_back("junk");HelpyPersonalization::Set(stackKey,{{"Tags",next}});}
        if(ImGui::MenuItem("Use item-type marks"))HelpyPersonalization::Set(stackKey,{{"Favorite",nullptr},{"Locked",nullptr},{"Tags",nullptr}});
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();ImGui::EndMenu();
}
inline bool Matches(const Json& row,bool favoriteOnly,const std::string& tag){return (!favoriteOnly||HelpyPersonalization::Favorite(row))&&(tag.empty()||HelpyPersonalization::Tagged(row,tag));}
inline bool Filters(bool& favorites,std::string& tag){
    bool changed=false;if(HelpyImGuiTheme::SoftButton("Favorites / tags"))ImGui::OpenPopup("##personal-filters");
    if(ImGui::BeginPopup("##personal-filters")){changed|=ImGui::Checkbox("Favorites only",&favorites);ImGui::Separator();
        if(ImGui::Selectable("All tags",tag.empty())){tag.clear();changed=true;}
        std::set<std::string> tags;{std::shared_lock lock(HelpyPersonalization::Gate);for(const auto& [key,row]:HelpyPersonalization::Entries.items())if((key.starts_with("item:")||HelpyPersonalization::TypeKeyValid(key)||HelpyPersonalization::StackKeyValid(key))&&row.contains("Tags"))for(const auto& value:row["Tags"])tags.insert(value.get<std::string>());}
        for(const auto& value:tags)if(ImGui::Selectable(value.c_str(),value==tag)){tag=value;changed=true;}
        if(ImGui::MenuItem("Reload saved labels"))HelpyPersonalization::Load(HelpyPersonalization::File);
        ImGui::TextDisabled("Locks protect every copy of an item type in Helpy.");ImGui::EndPopup();}
    if(favorites||!tag.empty()){ImGui::SameLine();ImGui::TextDisabled("%s%s%s",favorites?"Favorites":"",favorites&&!tag.empty()?" · ":"",tag.c_str());}return changed;
}
inline void Marks(const Json& row,ImVec2 a,ImVec2 b){
    const auto& theme=HelpyImGuiTheme::Current;auto* draw=ImGui::GetWindowDrawList();const float radius=theme.Y(6);float x=a.x+theme.X(10),y=b.y-theme.Y(12);
    if(HelpyPersonalization::Favorite(row)){ImVec2 points[10];for(int i=0;i<10;++i){const float angle=-1.5707963f+i*.6283185f,r=i%2?radius*.43f:radius;points[i]={x+std::cos(angle)*r,y+std::sin(angle)*r};}draw->AddConcavePolyFilled(points,10,ImGui::GetColorU32(theme.accentHover));x+=theme.X(20);}
    if(HelpyPersonalization::Locked(row)){const float s=theme.Y(5);draw->AddRectFilled({x-s,y-s*.15f},{x+s,y+s*1.05f},ImGui::GetColorU32(theme.text),theme.X(1));draw->AddRect({x-s*.65f,y-s},{x+s*.65f,y+s*.25f},ImGui::GetColorU32(theme.text),theme.X(3),0,theme.X(1.5f));}
}
inline void Italic(ImDrawList* draw,ImFont* font,float size,ImVec2 pos,ImU32 color,const std::string& text){
    const int start=draw->VtxBuffer.Size;draw->AddText(font,size,pos,color,text.c_str());
    for(int i=start;i<draw->VtxBuffer.Size;++i)draw->VtxBuffer[i].pos.x+=(pos.y+size-draw->VtxBuffer[i].pos.y)*.16f;
}
inline void Render(){
    if(!Open)return;const auto& theme=HelpyImGuiTheme::Current;HelpyImGuiTheme::Scope skin(theme);ImGui::SetNextWindowSize({theme.X(460),0},ImGuiCond_Appearing);
    if(ImGui::Begin("Helpy labels###personalization",&Open,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("Original: {%s}",Original.c_str());ImGui::SetNextItemWidth(theme.X(380));ImGui::InputText("Nickname",Nickname,sizeof(Nickname));
        if(Item){ImGui::SetNextItemWidth(theme.X(380));ImGui::InputText("Tags",Tags,sizeof(Tags));ImGui::TextDisabled("Comma-separated tags; use junk for removal review.");ImGui::Checkbox("Favorite",&Favorite);ImGui::Checkbox("Protect every copy from Helpy removal",&Locked);}
        ImGui::TextWrapped("Display labels only. Game IDs, folders and saves are not renamed.");
        if(HelpyImGuiTheme::SoftButton("Save")){try{Json patch={{"Nickname",std::string(Nickname)}};if(Item){Json tags=Json::array();std::string text=Tags;size_t begin=0;while(begin<text.size()){const auto end=text.find(',',begin);auto tag=HelpyPersonalization::Trim(text.substr(begin,end==text.npos?text.npos:end-begin));if(!tag.empty())tags.push_back(tag);if(end==text.npos)break;begin=end+1;}patch["Tags"]=tags;patch["Favorite"]=Favorite;patch["Locked"]=Locked;}
            if(HelpyPersonalization::Set(Key,patch))Open=false;else Message=HelpyPersonalization::Status;}catch(const std::exception& error){Message=error.what();}}
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Use original name")){if(HelpyPersonalization::Set(Key,{{"Nickname",""}}))Open=false;else Message=HelpyPersonalization::Status;}
        if(!Message.empty())ImGui::TextWrapped("%s",Message.c_str());
    }ImGui::End();
}
}
