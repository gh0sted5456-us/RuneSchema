#pragma once
#include "Generator/AuthoringClient.h"
#include "Generator/AuthoringForms.h"
#include "Generator/AuthoringViews.h"
#include <Windows.h>
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyEnvironment.h"
#include "Core/HelpyPreviewHooks.h"
#include <array>
#include <cstdio>
#include <set>
#include <random>
#include <imgui.h>

namespace PS::AuthoringPanel {
using Json=nlohmann::json;
enum class Section {Content,Presets,Traces,Inspector,Results,Npc,Cleanup,Effects,Session};
inline Section Current=Section::Content;
inline std::function<void()> SessionTools,OpenSpawns;
inline std::string ResultSelection;
inline constexpr const char* Sections[]{"Content & schemas","Capture presets","Traces & event rules","Inspector","Results & exports","NPC import","Offline save cleanup","Effect tests","Session cleanup"};
inline char Query[257]{},Loader[32]="assets",Name[97]="NewItem",Mod[97]="MyMod",Id[129]="new_item",Player[129]="*",Definition[97]="";
inline char Draft[65537]="{}",Fields[32769]="{}",Preset[16385]="{\"Name\":\"MyPlayer\",\"IncludePlayer\":true}",Trace[32769]="{\"Seconds\":30,\"MaxEvents\":1024,\"Categories\":15,\"Filter\":\"\",\"SuppressTicks\":true,\"TraceSelectedObject\":false,\"CaptureParameters\":false}";
inline char Profile[32769]="{\"Version\":1,\"Name\":\"MyTrace\",\"Trace\":{\"Seconds\":30,\"MaxEvents\":1024,\"Categories\":15,\"Filter\":\"\",\"SuppressTicks\":true,\"TraceSelectedObject\":false,\"CaptureParameters\":false}}";
inline char ObjectPath[4097]{},SavePath[4097]{},Marker[129]="Marker",ExportName[97]{},EventRule[16385]="{\"State\":\"Active\",\"Icon\":\"\",\"Player\":\"*\",\"Timeout\":3,\"Conditions\":[]}";
inline char Effect[16385]="{}";
inline Json Entry=Json::object(),Baseline=Json::object();
inline int Mode=0,Limit=100,Delay=3,Builder=0,ArmorSlots=7;
inline bool ProfileTarget=false,DelayCrosshair=true;
inline ImGuiTextFilter PresetFilter,TraceProfileFilter,ObjectFilter,PropertyFilter;
inline bool ReadValues=true,Extended=false,Offline=false,EraseProgress=false,PruneInvalid=false,Confirmed=false,Jsonc=false,IncludeTables=false;
inline float Range=10000,Location[3]{},Yaw=0;
inline std::set<std::string> Owners;
inline std::string CleanupToken;
inline uint64_t Applied=0,Generation=0;
inline bool CatalogueRequested=false;
inline void Copy(char* out,size_t capacity,const std::string& text){if(text.size()>=capacity)throw std::runtime_error("This document is too large for the editor; export it instead.");std::snprintf(out,capacity,"%s",text.c_str());}
inline Json Parse(const char* text){return Json::parse(text,nullptr,true,true);}
inline bool UiButton(const char* label){const bool clicked=HelpyImGuiTheme::SoftButton(label);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringButtons[label]=HelpyPreviewHooks::LastRect();
#endif
    return clicked;}
inline bool Button(const char* label,const char* operation,const Json& args=Json::object()){
    const bool clicked=UiButton(label);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::AuthoringButtons[label]=HelpyPreviewHooks::LastRect();
#endif
    if(clicked)AuthoringClient::Submit(operation,args);return clicked;
}
inline void Editor(const char* label,char* text,size_t size,ImVec2 dimensions){ImGui::TextUnformatted(label);const auto id=std::string("##")+label;ImGui::InputTextMultiline(id.c_str(),text,size,dimensions);}
template<class F>inline void Dispatch(const char* op,F arguments){try{AuthoringClient::Submit(op,arguments());}catch(const std::exception& error){AuthoringClient::Error=error.what();}}
inline const std::string& CachedJson(const Json& value,const char* id){
    struct Cached {uint64_t Revision=~uint64_t{};std::string Text;};
    static std::map<std::string,Cached> cache;
    if(cache.size()>64)cache.clear();auto& cached=cache[id];
    if(cached.Revision!=AuthoringClient::Revision){cached.Text=value.dump(2);cached.Revision=AuthoringClient::Revision;}
    return cached.Text;
}
inline void TextJson(const Json& value,const char* id,float height=190){
    const auto& text=CachedJson(value,id);ImGui::BeginChild(id,ImVec2(0,height),true,ImGuiWindowFlags_HorizontalScrollbar);ImGui::TextUnformatted(text.c_str());ImGui::EndChild();
}
inline void ApplyResult(){
    const auto& snap=AuthoringClient::Snapshot;const auto receipt=snap.value("RequestId",uint64_t{});if(!receipt||Applied==receipt)return;Applied=receipt;
    const auto op=snap.value("Operation",std::string{});const auto& data=AuthoringClient::Result(op.c_str());
    if(op!="Catalogue"&&op!="DiagnosticGuide")ResultSelection=op;
    if(data.contains("Error"))return;
    if(op=="AssetCapture"){Entry=data.value("Entry",Json::object());Copy(Fields,sizeof(Fields),data.value("Fields",Json::object()).dump(2));Baseline=Json::object();}
    if(op=="StarterBuild"||op=="PlayerBuild"||op=="NameplateBuild"||op=="EventRuleBuild"||op=="Schema"){
        if(data.contains("Document"))Copy(Draft,sizeof(Draft),data["Document"].dump(2));if(data.contains("Baseline"))Baseline=data["Baseline"];
    }
    if(op=="TraceTarget"||op=="TraceSelect"||op=="TraceTargetClear"){Copy(ObjectPath,sizeof(ObjectPath),data.value("TraceTarget",std::string{}));ProfileTarget=false;}
    if(op=="EventSuggestions"){auto rule=Parse(EventRule);rule["Event"]=data.at("Event");rule["Conditions"]=Json::array();Copy(EventRule,sizeof(EventRule),rule.dump(2));}
    if(op=="TraceProfileLoad"){if(data.contains("Options"))Copy(Trace,sizeof(Trace),data["Options"].dump(2));Copy(ObjectPath,sizeof(ObjectPath),data.value("TraceTarget",std::string{}));ProfileTarget=data.contains("Document")&&data["Document"].contains("Target");if(data.contains("Export")){Copy(ExportName,sizeof(ExportName),data["Export"].value("Name",std::string{}));Jsonc=data["Export"].value("Jsonc",false);}}
    if(op=="CleanupInspect"){Owners.clear();CleanupToken.clear();Confirmed=false;PruneInvalid=false;}
    if(op=="CleanupPreview"){CleanupToken=data.value("Token",std::string{});Confirmed=false;}
    if(op=="CleanupExport"){CleanupToken.clear();Confirmed=false;}
    if(op=="Clear"){AuthoringClient::Results.clear();Entry=Json::object();Baseline=Json::object();CleanupToken.clear();Confirmed=false;CatalogueRequested=false;}
}
inline void DraftEditor(const char* loader,bool schema=false,const char* kind="Starter"){
    ImGui::SeparatorText(schema?"Schema / draft":"Review draft");
    ImGui::InputTextMultiline("##authoring-draft",Draft,sizeof(Draft),ImVec2(-1,180));
    ImGui::InputText("Export name",ExportName,sizeof(ExportName));
    if(UiButton("Export reviewed JSONC"))AuthoringClient::Submit("ExportDocument",{{"Loader",loader},{"Mode",Mode},{"Kind",schema?"Report":kind},{"Name",ExportName},{"Jsonc",true},{"Document",Parse(Draft)}});
    ImGui::SameLine();if(UiButton("Copy draft"))ImGui::SetClipboardText(Draft);
    ImGui::TextDisabled("Exports create files for review. They do not install or activate a mod.");
}
inline void Content(){
    const auto& catalogue=AuthoringClient::Result("Catalogue");
    ImGui::SetNextItemWidth(220);if(ImGui::BeginCombo("Loader",Loader)){
        if(catalogue.contains("Loaders"))for(const auto& row:catalogue["Loaders"]){const auto name=row.at("Name").get<std::string>();if(ImGui::Selectable(row.value("Title",name).c_str(),name==Loader)){Copy(Loader,sizeof(Loader),name);Entry=Json::object();Copy(Fields,sizeof(Fields),"{}");Mode=0;Builder=0;AuthoringClient::Results.erase("AssetSearch");AuthoringClient::Results.erase("AssetCapture");Baseline=Json::object();}}
        ImGui::EndCombo();
    }
    const Json* capability=nullptr;if(catalogue.contains("Loaders"))for(const auto& row:catalogue["Loaders"])if(row.value("Name",std::string{})==Loader){capability=&row;break;}
    if(capability)ImGui::TextWrapped("%s",capability->value("Help",std::string{}).c_str());
    if(std::string(Loader)=="spawns"&&OpenSpawns&&UiButton("Open live spawn placement"))OpenSpawns();
    const bool focused=std::string(Loader)=="players"||std::string(Loader)=="nameplates";
    if(focused){ImGui::RadioButton("Search existing",&Builder,0);ImGui::SameLine();ImGui::RadioButton("Create new",&Builder,1);}
    if(focused&&Builder==1){
        ImGui::InputText("Rule / definition Id",Id,sizeof(Id));if(std::string(Loader)=="players"){ImGui::InputText("Player name (* for everyone)",Player,sizeof(Player));ImGui::InputText("Nameplate definition (optional)",Definition,sizeof(Definition));}
        ImGui::TextWrapped(std::string(Loader)=="nameplates"?"Edit Mode, Icon, Scale, Distance, audience, States, Events, SkillXP and timeout below.":"Edit health, stamina, scale, movement, capacity, attributes, appearance and nameplate overrides below.");
        // Parse only on demand so incomplete text never interrupts the form.
        auto controls=std::string(Loader)=="players"?AuthoringForms::Player(Json::object()):AuthoringForms::Nameplate(Json::object());
        Editor("Advanced fields JSONC",Fields,sizeof(Fields),ImVec2(-1,150));
        if(UiButton("Build create-new preview")){auto fields=Parse(Fields);if(!fields.is_object())throw std::runtime_error("Advanced fields must be an object");fields.merge_patch(controls);AuthoringClient::Submit(std::string(Loader)=="players"?"PlayerBuild":"NameplateBuild",{{"Id",Id},{"Player",Player},{"Definition",Definition},{"Fields",fields}});}
    }else{
        const bool enter=ImGui::InputTextWithHint("##authoring-search","Search loaded or authored records...",Query,sizeof(Query),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
        if(UiButton("Search")||enter)AuthoringClient::Submit("AssetSearch",{{"Loader",Loader},{"Query",Query}});
        ImGui::Checkbox("Read captured values",&ReadValues);ImGui::SameLine();Button("Export search results","ExportCaptured",{{"Kind","AssetSearch"}});
        const auto& found=AuthoringClient::Result("AssetSearch");
        if(found.contains("AssetMatches")&&found["AssetMatches"].is_array()){
            ImGui::BeginChild("##authoring-matches",ImVec2(0,145),true);int i=0;
            for(const auto& row:found["AssetMatches"]){const auto key=row.value("Key",row.value("Path",std::string{}));ImGui::PushID(i++);if(ImGui::Selectable(row.value("Name",row.value("Label",key)).c_str(),Entry.value("Key",std::string{})==key)){Entry=row;AuthoringClient::Submit("AssetCapture",{{"Loader",Loader},{"Entry",row},{"ReadValues",ReadValues}});}
#if defined(HELPY_PREVIEW)
                HelpyPreviewHooks::AuthoringButtons["Asset:"+key]=HelpyPreviewHooks::LastRect();
#endif
                ImGui::PopID();}ImGui::EndChild();
        }
        constexpr const char* formats[]{"Basic starter","Patch","Clone","Field reference"};ImGui::SetNextItemWidth(200);
        if(ImGui::BeginCombo("Format",formats[std::clamp(Mode,0,3)])){for(int i=0;i<4;++i){const bool allowed=capability&&(i!=1||capability->value("Patch",false))&&(i!=2||capability->value("Clone",false));ImGui::BeginDisabled(!allowed);if(ImGui::Selectable(formats[i],Mode==i))Mode=i;ImGui::EndDisabled();}ImGui::EndCombo();}
        ImGui::InputText("Mod name",Mod,sizeof(Mod));ImGui::InputText("New name",Name,sizeof(Name));ImGui::InputText("Persistence Id (clone)",Id,sizeof(Id));
        if(UiButton("Generate Id")){constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";std::random_device random;for(int i=0;i<21;++i)Id[i]=alphabet[random()&63];Id[21]=alphabet[(random()&3)<<4];Id[22]=0;}
        const auto& captured=AuthoringClient::Result("AssetCapture");
        if(captured.contains("AvailableFields")&&ImGui::CollapsingHeader("Available fields"))TextJson(captured["AvailableFields"],"##available-fields",120);
        if(captured.contains("Fields")&&captured["Fields"].is_object()&&ImGui::CollapsingHeader("Select captured fields")){
            auto chosen=Json::parse(Fields,nullptr,false,true);
            if(chosen.is_object())for(const auto& [key,value]:captured["Fields"].items()){
                bool enabled=chosen.contains(key);if(ImGui::Checkbox(key.c_str(),&enabled)){if(enabled)chosen[key]=value;else chosen.erase(key);Copy(Fields,sizeof(Fields),chosen.dump(2));}
            }else ImGui::TextDisabled("Fix the JSONC below to select fields.");
        }
        Editor("Selected fields JSONC",Fields,sizeof(Fields),ImVec2(-1,140));
        ImGui::BeginDisabled(Entry.empty());
        if(UiButton("Build preview"))Dispatch("StarterBuild",[&]{return Json{{"Loader",Loader},{"Entry",Entry},{"Fields",Mode==3?captured.value("AvailableFields",Json::object()):Parse(Fields)},{"Mode",Mode},{"Mod",Mod},{"Name",Name},{"Id",Id}};});
        ImGui::SameLine();Button("Export editable reference","AssetReference",{{"Loader",Loader},{"Entry",Entry}});ImGui::EndDisabled();
        ImGui::BeginDisabled(Baseline.empty());if(UiButton("Check changes and dependencies"))Dispatch("StarterCheck",[&]{return Json{{"Loader",Loader},{"Draft",Parse(Draft)},{"Baseline",Baseline}};});ImGui::EndDisabled();
    }
    if(ImGui::CollapsingHeader("Structural schemas")){
        Button("View schema","Schema",{{"Loader",Loader}});ImGui::SameLine();Button("Export loader schema","SchemaExport",{{"Loader",Loader},{"Name",ExportName}});
        ImGui::Checkbox("Include loaded tables in full schema export",&IncludeTables);Button("Export all loader schemas","SchemaExport",{{"Loader","all"},{"Name",ExportName},{"IncludeLoadedTables",IncludeTables}});
    }
    if(std::string(Loader)=="quests"&&ImGui::CollapsingHeader("Native quest status")){Button("Refresh native quest status","QuestStatus");TextJson(AuthoringClient::Result("QuestStatus"),"##quest-status");}
    DraftEditor(Loader,Mode==3);
}
inline void Guide(const char* section){
    if(!ImGui::CollapsingHeader("Diagnostic guide"))return;
    Button("Reload guide","DiagnosticGuide");const auto& result=AuthoringClient::Result("DiagnosticGuide");
    if(result.contains("Error"))ImGui::TextWrapped("%s",result["Error"].get_ref<const std::string&>().c_str());
    if(result.contains("Document"))for(const auto* name:{section,"Libraries"})if(result["Document"].contains(name))for(const auto& line:result["Document"][name])ImGui::TextWrapped("%s",line.get_ref<const std::string&>().c_str());
}
inline void Presets(){
    Guide("Presets");PresetFilter.Draw("Filter presets");Button("Refresh presets","PresetsList",{{"Extended",Extended}});const auto& rows=AuthoringClient::Result("PresetsList");
    if(rows.contains("Documents"))for(const auto& preset:rows["Documents"])if(PresetFilter.PassFilter((preset.value("Name",std::string{})+" "+preset.value("Description",std::string{})).c_str())&&ImGui::Selectable(preset.at("Name").get_ref<const std::string&>().c_str()))Copy(Preset,sizeof(Preset),preset.dump(2));
    if(rows.contains("Errors"))for(const auto& error:rows["Errors"])ImGui::TextWrapped("%s",error.get_ref<const std::string&>().c_str());
    ImGui::Checkbox("Allow deeper capture (up to 16)",&Extended);ImGui::InputText("Selected object (optional)",ObjectPath,sizeof(ObjectPath));
    Editor("Preset JSONC",Preset,sizeof(Preset),ImVec2(-1,220));
    if(UiButton("Run selected preset"))AuthoringClient::Submit("Preset",{{"Document",Parse(Preset)},{"Extended",Extended},{"SelectedObject",ObjectPath}});
    ImGui::SameLine();if(UiButton("Save preset"))AuthoringClient::Submit("PresetSave",{{"Document",Parse(Preset)},{"Extended",Extended}});
    ImGui::TextWrapped("Saving the same preset Name replaces that user preset. Captures export separate timestamped reports.");
}
inline void Traces(){
    Guide("Traces");
    if(ImGui::CollapsingHeader("Profiles")){
        TraceProfileFilter.Draw("Filter trace profiles");Button("Refresh trace profiles","TraceProfilesList");const auto& profiles=AuthoringClient::Result("TraceProfilesList");if(profiles.contains("Documents"))for(const auto& row:profiles["Documents"])if(TraceProfileFilter.PassFilter((row.value("Name",std::string{})+" "+row.value("Description",std::string{})).c_str())&&ImGui::Selectable(row.at("Name").get_ref<const std::string&>().c_str()))Copy(Profile,sizeof(Profile),row.dump(2));
        if(profiles.contains("Errors"))for(const auto& error:profiles["Errors"])ImGui::TextWrapped("%s",error.get_ref<const std::string&>().c_str());
        if(UiButton("Use current trace controls in profile")){
            auto profile=Parse(Profile);profile["Trace"]=Parse(Trace);profile["Export"]={{"Name",ExportName},{"Jsonc",Jsonc}};
            if(!ProfileTarget){if(ObjectPath[0])profile["Target"]={{"Mode","ExactPath"},{"Path",ObjectPath}};else profile.erase("Target");}Copy(Profile,sizeof(Profile),profile.dump(2));
        }
        Editor("Profile JSONC",Profile,sizeof(Profile),ImVec2(-1,150));
        if(UiButton("Load profile"))AuthoringClient::Submit("TraceProfileLoad",{{"Document",Parse(Profile)}});ImGui::SameLine();if(UiButton("Save profile"))AuthoringClient::Submit("TraceProfileSave",{{"Document",Parse(Profile)}});
    }
    ImGui::InputText("Find live target",Query,sizeof(Query));ImGui::SliderInt("Target search limit",&Limit,1,500);Button("Search target instances","TraceBrowse",{{"Query",Query},{"Limit",Limit}});
    const auto& matches=AuthoringClient::Result("TraceBrowse");if(matches.contains("TraceMatches"))for(const auto& row:matches["TraceMatches"]){const auto path=row.get<std::string>();if(ImGui::Selectable(path.c_str(),path==ObjectPath)){Copy(ObjectPath,sizeof(ObjectPath),path);AuthoringClient::Submit("TraceSelect",{{"Path",path}});}}
    if(ImGui::InputText("Selected live object",ObjectPath,sizeof(ObjectPath)))ProfileTarget=false;Button("Clear trace target","TraceTargetClear");
    ImGui::BeginDisabled(HelpyEnvironment::Server());Button("Pick crosshair target in 3 seconds","TraceTarget",{{"RangeCm",Range}});ImGui::EndDisabled();
    ImGui::SliderFloat("Target range (cm)",&Range,100,50000,"%.0f");
    if(ImGui::CollapsingHeader("Trace setup")){try{auto options=Parse(Trace);if(AuthoringViews::TraceControls(options))Copy(Trace,sizeof(Trace),options.dump(2));}catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}}
    Editor("Trace options JSONC",Trace,sizeof(Trace),ImVec2(-1,160));
    if(UiButton("Start trace"))AuthoringClient::Submit("PlayerTraceStart",{{"Options",Parse(Trace)},{"SelectedObject",ObjectPath},{"UseProfileTarget",ProfileTarget}});
    ImGui::SameLine();Button("Stop and export trace","PlayerTraceStop");ImGui::InputText("Marker",Marker,sizeof(Marker));Button("Add marker","PlayerTraceMarker",{{"Marker",Marker}});
    if(ImGui::CollapsingHeader("Appearance trace")){ImGui::BeginDisabled(HelpyEnvironment::Server());Button("Start appearance trace","TraceStart");ImGui::EndDisabled();ImGui::SameLine();Button("Stop and export appearance trace","TraceStop");}
    if(ImGui::CollapsingHeader("Event rules from captured parameters")){
        const auto& result=AuthoringClient::Result("PlayerTraceStop");
        if(result.contains("Events")&&result["Events"].is_object()&&result["Events"].contains("events")){
            ImGui::BeginChild("##captured-events",ImVec2(0,140),true);int i=0;
            for(const auto& event:result["Events"]["events"]){ImGui::PushID(i++);
                ImGui::BeginDisabled(!event.contains("parameters"));
                if(ImGui::Selectable(event.value("function",std::string("Captured event")).c_str()))AuthoringClient::Submit("EventSuggestions",{{"Event",event}});
                ImGui::EndDisabled();ImGui::PopID();}ImGui::EndChild();
        }
        const auto& suggestions=AuthoringClient::Result("EventSuggestions");
        if(suggestions.contains("Suggestions")){
            auto rule=Json::parse(EventRule,nullptr,false,true);
            if(rule.is_object()&&rule.contains("Conditions")&&rule["Conditions"].is_array()&&rule.value("Event",Json{})==suggestions.value("Event",Json{})){
                if(ImGui::BeginCombo("Add parameter condition","Choose captured value")){
                    int i=0;for(const auto& condition:suggestions["Suggestions"]){ImGui::PushID(i++);ImGui::BeginDisabled(rule["Conditions"].size()>=8);
                        if(ImGui::Selectable(condition.dump().c_str())){rule["Conditions"].push_back(condition);Copy(EventRule,sizeof(EventRule),rule.dump(2));}
                        ImGui::EndDisabled();ImGui::PopID();}ImGui::EndCombo();
                }
            }
        }
        Editor("State, icon and parameter conditions",EventRule,sizeof(EventRule),ImVec2(-1,170));if(UiButton("Build event rule"))AuthoringClient::Submit("EventRuleBuild",Parse(EventRule));DraftEditor("players",false,"Rule");
    }
}
inline void Inspector(){
    ImGui::TextWrapped("Capture loaded data and supported fields. This tool does not invoke arbitrary native functions.");
    ImGui::BeginDisabled(HelpyEnvironment::Server());Button("Inspect local player","Player",{{"DelaySeconds",Delay}});ImGui::SameLine();Button("Inspect crosshair target","Target",{{"RangeCm",Range},{"DelayCrosshair",DelayCrosshair}});ImGui::EndDisabled();
    ImGui::Checkbox("Delay crosshair capture by 3 seconds",&DelayCrosshair);
    ImGui::SliderInt("Player capture delay",&Delay,0,30);Button("Save player capture preset","PlayerPresetSave",{{"DelaySeconds",Delay}});ImGui::SliderFloat("Crosshair range (cm)",&Range,100,100000,"%.0f");
    ImGui::InputText("Search loaded objects",Query,sizeof(Query));ImGui::SliderInt("Maximum results",&Limit,1,500);Button("Search objects","Browse",{{"Query",Query},{"Limit",Limit}});
    ObjectFilter.Draw("Filter loaded object results");Button("Export object search results","ExportCaptured",{{"Kind","ObjectSearch"}});
    const auto& result=AuthoringClient::Result("Browse");if(result.contains("Matches")){ImGui::BeginChild("##object-matches",ImVec2(0,180),true);for(const auto& path:result["Matches"]){const auto text=path.get<std::string>();if(ObjectFilter.PassFilter(text.c_str())&&ImGui::Selectable(text.c_str(),text==ObjectPath)){Copy(ObjectPath,sizeof(ObjectPath),text);AuthoringClient::Submit("Object",{{"Path",text}});}}ImGui::EndChild();}
    ImGui::InputText("Object path",ObjectPath,sizeof(ObjectPath));Button("Inspect selected object","Object",{{"Path",ObjectPath}});
    if(ImGui::CollapsingHeader("Saved-state reports")){ImGui::BeginDisabled(HelpyEnvironment::Server());Button("Read saved items","SavedItems");ImGui::EndDisabled();ImGui::SameLine();Button("Read live building registry","SavedWorld");}
}
inline void Results(){
    const auto& snap=AuthoringClient::Snapshot;ImGui::TextWrapped("%s",snap.value("Status",std::string{}).c_str());
    auto& selected=ResultSelection;
    if(selected.empty())selected=snap.value("Operation",std::string{});
    ImGui::SetNextItemWidth(240);if(ImGui::BeginCombo("Result",selected.empty()?"Choose operation":selected.c_str())){for(const auto& [op,data]:AuthoringClient::Results)if(ImGui::Selectable(op.c_str(),selected==op))selected=op;ImGui::EndCombo();}
    const auto& data=AuthoringClient::Result(selected.c_str());
    const Json* report=&data;
    if(selected=="SavedItems"&&data.contains("SavedItems"))report=&data["SavedItems"];
    else if(selected=="SavedWorld"&&data.contains("SavedWorld"))report=&data["SavedWorld"];
    else if(data.contains("Snapshot")&&!data["Snapshot"].is_null())report=&data["Snapshot"];
    if(report->contains("object")){PropertyFilter.Draw("Filter captured fields");AuthoringViews::Snapshot(*report,PropertyFilter);}
    else if((selected=="SavedItems"||selected=="SavedWorld")&&report->is_object())AuthoringViews::SavedReport(*report,selected=="SavedWorld");
    else TextJson(*report,("##authoring-results-"+selected).c_str(),260);
    const auto& reportText=CachedJson(*report,("export-"+selected).c_str());
    if(data.value("Truncated",false)||reportText.size()>200*1024){
        const char* kind=selected=="SavedItems"?"SavedItems":selected=="SavedWorld"?"SavedWorld":selected=="PlayerTraceStop"?"Events":selected=="AssetSearch"?"AssetSearch":selected=="Browse"?"ObjectSearch":"Snapshot";
        Button("Export complete current capture","ExportCaptured",{{"Kind",kind}});
        ImGui::TextDisabled("Exports the latest native capture of this kind.");
    }
    else Button("Export result","ExportDocument",{{"Document",*report},{"Name",ExportName},{"Jsonc",Jsonc},{"Kind","Report"}});
    ImGui::SameLine();if(UiButton("Copy result"))ImGui::SetClipboardText(reportText.c_str());
    if(data.contains("File")){ImGui::TextWrapped("File: %s",data["File"].get_ref<const std::string&>().c_str());if(UiButton("Copy export path"))ImGui::SetClipboardText(data["File"].get_ref<const std::string&>().c_str());}
    if(snap.contains("History")&&ImGui::CollapsingHeader("Recent activity"))for(const auto& row:snap["History"])ImGui::BulletText("%s: %s",row.value("Operation",std::string{}).c_str(),row.value("Status",std::string{}).c_str());
}
inline void SaveInput(){
    Button("Refresh imported saves","Imports");const auto& files=AuthoringClient::Result("Imports");if(files.contains("Folder"))ImGui::TextWrapped("Shared input folder: %s",files["Folder"].get_ref<const std::string&>().c_str());
    if(ImGui::BeginCombo("Imported character",SavePath[0]?SavePath:"Select save")){if(files.contains("Files"))for(const auto& file:files["Files"])if(ImGui::Selectable(file.at("Name").get_ref<const std::string&>().c_str())){Copy(SavePath,sizeof(SavePath),file.at("Path"));CleanupToken.clear();Confirmed=false;}ImGui::EndCombo();}
    if(ImGui::InputText("Character JSON path",SavePath,sizeof(SavePath))){CleanupToken.clear();Confirmed=false;}
}
inline void Npc(){
    ImGui::TextWrapped("Import an offline character's appearance and equipped items. Load its item mods before resolving equipment. Exported NPCs start disabled.");SaveInput();Button("Read save and resolve equipment","NpcPreview",{{"Path",SavePath}});
    const auto& preview=AuthoringClient::Result("NpcPreview");if(preview.contains("NPC")){TextJson(preview,"##npc-preview",180);ImGui::InputText("NPC Id",Id,sizeof(Id));ImGui::InputFloat3("World position (cm)",Location);ImGui::InputFloat("Yaw",&Yaw);Button("Export disabled NPC","NpcExport",{{"Id",Id},{"Location",{Location[0],Location[1],Location[2]}},{"Yaw",Yaw}});}
}
inline void Cleanup(){
    ImGui::TextWrapped("Export a cleaned copy of an offline native character JSON save. The original and running world stay untouched. Binary world saves and WGS containers are unsupported.");SaveInput();
    ImGui::Checkbox("This character save is offline",&Offline);ImGui::BeginDisabled(!Offline);Button("Inspect save and scan mods","CleanupInspect",{{"Path",SavePath},{"Offline",Offline}});ImGui::EndDisabled();
    const auto& inspected=AuthoringClient::Result("CleanupInspect");
    if(inspected.contains("Owners")){
        for(const auto& row:inspected["Owners"]){const auto owner=row.at("Owner").get<std::string>();bool checked=Owners.contains(owner);if(ImGui::Checkbox(owner.c_str(),&checked)){if(checked)Owners.insert(owner);else Owners.erase(owner);CleanupToken.clear();Confirmed=false;}ImGui::SameLine();ImGui::TextDisabled("%zu records | %s",row.value("Count",size_t{}),row.value("Installed",false)?"installed folder":"absent / unknown");}
        if(ImGui::Checkbox("Erase progress for selected owners",&EraseProgress)){CleanupToken.clear();Confirmed=false;}
        ImGui::BeginDisabled(!inspected.value("RegistryReady",false));if(ImGui::Checkbox("Remove invalid item / recipe / quest IDs",&PruneInvalid)){CleanupToken.clear();Confirmed=false;}ImGui::EndDisabled();
        if(!inspected.value("RegistryReady",false))ImGui::TextDisabled("Registry cleanup requires a completely loaded native registry.");
        if(Button("Preview removals","CleanupPreview",{{"Owners",Owners},{"EraseProgress",EraseProgress},{"PruneInvalid",PruneInvalid}})){CleanupToken.clear();Confirmed=false;}
    }
    const auto& preview=AuthoringClient::Result("CleanupPreview");if(!CleanupToken.empty()&&preview.contains("Removed")){TextJson(preview["Removed"],"##cleanup-removals",160);ImGui::Checkbox("I reviewed these removals; installed mods may recreate them",&Confirmed);ImGui::BeginDisabled(!Confirmed||preview["Removed"].empty());Button("Export cleaned copy","CleanupExport",{{"Token",CleanupToken},{"Confirm",Confirmed}});ImGui::EndDisabled();}
}
inline void Effects(){
    ImGui::TextWrapped("Temporary native effect tests. Exported definitions remain separate from installed content.");
    Button("Refresh built-in and shared effects","NiagaraPresets");const auto& library=AuthoringClient::Result("NiagaraPresets");
    if(ImGui::BeginCombo("Preset","Choose a test")){if(library.contains("Documents")){int i=0;for(const auto& row:library["Documents"]){ImGui::PushID(i++);if(ImGui::Selectable(row.value("Name",std::string("Unnamed")).c_str()))Copy(Effect,sizeof(Effect),row.dump(2));ImGui::PopID();}}ImGui::EndCombo();}
    if(library.contains("Errors"))for(const auto& error:library["Errors"])ImGui::TextWrapped("%s",error.get_ref<const std::string&>().c_str());
    Editor("Niagara preset JSONC",Effect,sizeof(Effect),ImVec2(-1,230));
    ImGui::BeginDisabled(HelpyEnvironment::Server());if(UiButton("Attach preset"))Dispatch("NiagaraAttach",[&]{return Json{{"Document",Parse(Effect)}};});ImGui::EndDisabled();ImGui::SameLine();Button("Remove test","NiagaraRemove");
    Button("Capture attachment and export","NiagaraInspect");ImGui::SliderInt("Armor slot mask",&ArmorSlots,1,7);ImGui::BeginDisabled(HelpyEnvironment::Server());Button("Start armor fire and smoke","NiagaraArmorStart",{{"ArmorSlots",ArmorSlots}});ImGui::EndDisabled();ImGui::SameLine();Button("Stop armor effect","NiagaraArmorStop");
    if(UiButton("Export effect preset"))AuthoringClient::Submit("ExportDocument",{{"Document",Parse(Effect)},{"Kind","Niagara"},{"Jsonc",true},{"Name",ExportName}});
}
inline void Render(){
    AuthoringClient::Poll();try{ApplyResult();}catch(const std::exception& error){AuthoringClient::Error=error.what();}
    const auto& snap=AuthoringClient::Snapshot;
    const auto generation=snap.value("Generation",uint64_t{});if(generation!=Generation){Generation=generation;Applied=0;ResultSelection.clear();Entry=Json::object();Baseline=Json::object();CleanupToken.clear();Confirmed=false;CatalogueRequested=false;}
    if(snap.value("Active",false)&&!AuthoringClient::Busy()&&!CatalogueRequested){CatalogueRequested=AuthoringClient::Submit("Catalogue");}
    if(!snap.value("Enabled",false)||!snap.value("Ready",false)){ImGui::TextWrapped("%s",snap.value("Status",std::string("Authoring requires a matching RuneSchema bridge.")).c_str());}
    if(!snap.value("Active",false)){
        ImGui::TextWrapped("Activation lasts for this session. These tools use your existing RuneSchema profiles, presets and export folders.");ImGui::BeginDisabled(!snap.value("Enabled",false)||!snap.value("Ready",false));if(UiButton("Activate authoring"))AuthoringClient::Activate();ImGui::EndDisabled();
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::AuthoringButtons["Activate authoring"]=HelpyPreviewHooks::LastRect();
#endif
    }
    if(HelpyEnvironment::Server())ImGui::TextDisabled("Dedicated server: local-player and crosshair captures are unavailable.");
    if(ImGui::CollapsingHeader("Export options")){ImGui::InputText("Export name (optional, no extension)",ExportName,sizeof(ExportName));ImGui::Checkbox("Export reports as JSONC (otherwise JSON)",&Jsonc);ImGui::TextWrapped("Reports, traces and presets use the existing RuneSchema folders. Existing exports are kept.");}
    AuthoringClient::ExportOptions={{"Name",ExportName},{"Jsonc",Jsonc}};
    ImGui::SetNextItemWidth(260);int section=static_cast<int>(Current);if(ImGui::Combo("Workflow",&section,Sections,IM_ARRAYSIZE(Sections)))Current=static_cast<Section>(section);
    ImGui::SameLine();Button("Refresh authoring","Catalogue");
    ImGui::Separator();ImGui::BeginDisabled(!snap.value("Active",false)||AuthoringClient::Busy());
    try{switch(Current){case Section::Content:Content();break;case Section::Presets:Presets();break;case Section::Traces:Traces();break;case Section::Inspector:Inspector();break;case Section::Results:Results();break;case Section::Npc:Npc();break;case Section::Cleanup:Cleanup();break;case Section::Effects:Effects();break;case Section::Session:if(SessionTools)SessionTools();else ImGui::TextDisabled("Session tools unavailable.");break;}}
    catch(const std::exception& error){AuthoringClient::Error=error.what();}
    ImGui::EndDisabled();ImGui::Separator();
    if(AuthoringClient::Busy()){ImGui::TextDisabled("Working on the game thread...");Button("Cancel pending capture","Cancel");}
    ImGui::TextWrapped("%s",AuthoringClient::Error.empty()?snap.value("Status",std::string{}).c_str():AuthoringClient::Error.c_str());
    if(ImGui::CollapsingHeader("Session cleanup")){ImGui::TextWrapped("Stop traces and clear captured results; exported files remain.");ImGui::BeginDisabled(!snap.value("Active",false)||AuthoringClient::Busy());Button("Stop traces and clear results","Clear");ImGui::EndDisabled();}
}
}
