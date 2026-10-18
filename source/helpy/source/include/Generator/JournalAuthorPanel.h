#pragma once
#include "Core/JournalWorkspace.h"
#include "Core/HelpyIconPack.h"
#include "Runtime/HostServices.h"
#include "Generator/HelpyImGuiTheme.h"
#include <future>
#include <cstdio>
namespace PS::JournalAuthorPanel {
using Json=nlohmann::json;
inline char Id[129]="my_mod_lore",Title[1025]{},Image[2049]{},Category[2049]{},Text[65537]{};
inline Json Pages=Json::array({""});
inline int Page=0,Type=0;
inline bool OpenRequested=false;
inline std::string Status;
inline std::future<std::string> Exporter;
inline void SetPage(int page){Page=std::clamp(page,0,int(Pages.size())-1);std::snprintf(Text,sizeof(Text),"%s",Pages[Page].get<std::string>().c_str());}
inline void Template(const Json& row){
    std::snprintf(Title,sizeof(Title),"%s",row.value("Name",std::string{}).c_str());std::snprintf(Image,sizeof(Image),"%s",row.value("Icon",std::string{}).c_str());
    Pages=row.value("Pages",Json::array());if(Pages.empty())Pages=Json::array({row.value("Description",std::string{})});SetPage(0);
    const auto type=row.value("Type",std::string{});Type=type=="People"?1:type=="Place"?2:type=="Treasure"?3:type=="World"?4:0;
    Status="Template copied. Choose a NEW mod-prefixed ID; the original entry and persistence identity are not edited.";
    OpenRequested=true;
}
inline void Shutdown(){if(Exporter.valid())Exporter.wait();}
inline void Render(){
    if(Exporter.valid()&&Exporter.wait_for(std::chrono::seconds(0))==std::future_status::ready)Status=Exporter.get();
    ImGui::TextWrapped("Create a new RuneSchema journal/lore definition. Export writes a draft only: no installed mod or character save changes.");
    const char* types[]{"Lore","People","Place","Treasure","World"};ImGui::Combo("Entry type",&Type,types,5);
    ImGui::TextUnformatted("Unique mod-prefixed ID");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-id",Id,sizeof(Id));
    ImGui::TextUnformatted("Title");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-title",Title,sizeof(Title));
    ImGui::TextUnformatted("Cooked image asset");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-image",Image,sizeof(Image));
    ImGui::TextUnformatted("Native subcategory asset");ImGui::SetNextItemWidth(-1);ImGui::InputText("##entry-category",Category,sizeof(Category));
    ImGui::Text("Page %d / %d",Page+1,int(Pages.size()));
    if(ImGui::Button("Previous page")&&Page>0){Pages[Page]=Text;SetPage(Page-1);}ImGui::SameLine();
    if(ImGui::Button("Next page")&&Page+1<int(Pages.size())){Pages[Page]=Text;SetPage(Page+1);}ImGui::SameLine();
    if(ImGui::Button("Add page")&&Pages.size()<128){Pages[Page]=Text;Pages.push_back("");SetPage(int(Pages.size())-1);}
    if(ImGui::Button("Remove page")&&Pages.size()>1){Pages.erase(Page);SetPage(std::min(Page,int(Pages.size())-1));}
    ImGui::InputTextMultiline("##journal-page",Text,sizeof(Text),{-1,HelpyImGuiTheme::Current.Y(210)});
    ImGui::TextWrapped("Unlock defaults to false. Placement does not grant discovery. Empty image/category fields are omitted for later authoring. Validate cooked references and placement in game before packaging.");
    ImGui::BeginDisabled(Exporter.valid());
    if(ImGui::Button("Export new journal/lore draft"))try{Pages[Page]=Text;const auto doc=JournalWorkspace::Draft(Id,types[Type],Title,Pages,Image,Category);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();const auto file=HostServices::ExportsDirectory()/"journal-drafts"/(std::string(Id)+"-"+std::to_string(stamp)+".json");
        Exporter=std::async(std::launch::async,[file,doc](){try{const auto text=doc.dump(2);HelpyIconPack::Publish(file,{text.begin(),text.end()},false);return "Draft exported: "+file.string();}catch(const std::exception& e){return std::string(e.what());}});
    }catch(const std::exception& e){Status=e.what();}ImGui::EndDisabled();
    if(!Status.empty())ImGui::TextWrapped("%s",Status.c_str());
}
}
