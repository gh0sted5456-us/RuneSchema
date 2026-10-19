#pragma once
#include "Generator/AuthoringPanel.h"
#include "Generator/LoaderTemplate.h"
#include "Generator/LoaderCapabilities.h"
#include "Core/CharacterSaveEditor.h"
namespace PS {
// Isolated render fixture: real admission/client/build code with explicitly
// synthetic loaded records. It cannot inspect or mutate a running game.
struct AuthoringPreviewFixture {
    HelpyAuthoring::Session Host;
    using Json=nlohmann::json;
    nlohmann::json Last;
    CharacterSaveEditor::Session Editor;
    Json Character={{"GameProgress",{{"Health",100},{"Version",86},{"Inventory",{{"MaxSlotIndex",8},{"0",{{"ItemData","example-modded-id"},{"GUID","fixture-instance-1"},{"Count",5}}},{"8",{{"ItemData","example-vanilla-id"},{"GUID","fixture-instance-2"}}}}}}},{"UnknownModSection",{{"Keep",true}}}};
    static Json Event(){return {{"function","/Preview/Player:OnGatheredResource"},{"parameters",{{"Amount",3},{"IsRare",false}}}};}
    static Json Profile(){return {{"Version",1},{"Name","Gathering"},{"Trace",{{"Seconds",20},{"MaxEvents",128},{"Categories",4},{"Filter","OnGatheredResource"},{"CaptureParameters",true},{"TraceSelectedObject",true}}},{"Target",{{"Mode","ClassAndName"},{"Class","/Preview/Player"},{"Name","Player_1"}}},{"Export",{{"Name","GatheringTrace"},{"Jsonc",true}}}};}
    static Json Report(bool world){return {{"Coverage",world?"Sample live buildings":"Sample saved item identities"},{"Warnings",Json::array({"Synthetic preview data only."})},{"Rows",Json::array({{{"DisplayName","Iron ore"},{"InternalName","IronOre"},{"PersistenceID","preview-ore"},{"Owner","Preview player"},{"State",{{"Active",true}}},{"Location","Backpack"},{"Origin","Saved inventory"}}})}};}
    void Seed(){
        for(const auto* op:{"PresetsList","TraceProfilesList","TraceProfileLoad","PlayerTraceStop","EventSuggestions","SavedItems","SavedWorld","Imports","NpcPreview","CleanupInspect","CleanupPreview","NiagaraPresets"}){
            Json args=Json::object();const std::string operation=op;
            if(operation=="TraceProfileLoad")args["Document"]=Profile();
            if(operation=="EventSuggestions")args["Event"]=Event();
            if(operation=="NpcPreview"||operation=="CleanupInspect")args={{"Path","preview-character.json"},{"Offline",true}};
            if(operation=="CleanupPreview")args={{"Owners",Json::array({"RemovedExampleMod"})},{"PruneInvalid",false},{"EraseProgress",false}};
            if(!AuthoringClient::Submit(op,args))throw std::runtime_error("Preview admission failed for "+operation+": "+AuthoringClient::Error);AuthoringClient::Poll(true);AuthoringPanel::ApplyResult();
            if(AuthoringClient::Result(op).contains("Error"))throw std::runtime_error("Preview workflow response failed");
        }
        if(!AuthoringPanel::ProfileTarget||std::string(AuthoringPanel::ExportName)!="GatheringTrace"||!AuthoringPanel::Jsonc)throw std::runtime_error("Trace profile lost target resolver or export settings");
        if(AuthoringPanel::Parse(AuthoringPanel::EventRule)["Event"]!=Event())throw std::runtime_error("Event selection did not populate rule editor");
        AuthoringPanel::ResultSelection="SavedWorld";
    }
    AuthoringPreviewFixture(){
        Character["meta_data"]={{"char_name","Example character"},{"char_guid","fixture-owner"}};
        for(const auto& field:CharacterWorkbench::Fields)Character["Customization"]["CustomizationData"][field.Key]={{"dataTable","/Script/Engine.DataTable'"+CharacterWorkbench::Table(field)+"'"},{"rowName","ExampleRow"}};
        Host.Configure(true,true);
        AuthoringClient::Remote=[this](const nlohmann::json& request,nlohmann::json& reply){
            const auto action=request.at("Action");
            if(action=="AuthoringActivate"){reply=Host.Activate();return true;}
            if(action=="AuthoringSubmit"){Last=request.at("Request");reply=Host.Submit(Last);return true;}
            if(action!="AuthoringSnapshot")return false;
            Host.TickActivation();
            if(auto work=Host.Take()){
                const auto& args=work->at("Arguments");const auto op=work->at("Operation");nlohmann::json data;
                try{
                    if(op=="Catalogue"){
                        data["Loaders"]=nlohmann::json::array();for(const auto& c:LoaderCapabilities)data["Loaders"].push_back({{"Name",c.Name},{"Title",c.DisplayName},{"Patch",c.StarterPatch},{"Clone",c.Clone},{"Help",c.Purpose}});
                    }else if(op=="AssetSearch")data={{"AssetMatches",nlohmann::json::array({{{"Path","/Preview/Items/IronOre.IronOre"},{"Name","Iron ore (sample)"}}})}};
                    else if(op=="AssetCapture")data={{"Entry",args.at("Entry")},{"Fields",{{"PowerLevel",5},{"MaxStackSize",100}}},{"AvailableFields",{{"PowerLevel",{{"type","integer"}}},{"MaxStackSize",{{"type","integer"}}}}}};
                    else if(op=="StarterBuild"){
                        data["Document"]=LoaderTemplate::Build(args.at("Loader"),args.at("Entry"),args.at("Fields"),args.value("Mode",0),args.value("Mod",std::string{}),args.value("Name",std::string{}),args.value("Id",std::string{}));data["Baseline"]=data["Document"];
                    }else if(op=="PresetsList")data={{"Documents",Json::array({{{"Name","Player overview"},{"IncludePlayer",true}}})},{"Errors",Json::array()}};
                    else if(op=="TraceProfilesList")data={{"Documents",Json::array({Profile()})}};
                    else if(op=="TraceProfileLoad")data={{"Document",Profile()},{"Options",Profile()["Trace"]},{"Export",Profile()["Export"]},{"TraceTarget","/Preview/Player_1"}};
                    else if(op=="PlayerTraceStop")data={{"Events",{{"events",Json::array({Event()})}}}};
                    else if(op=="EventSuggestions")data={{"Event",Event()},{"Suggestions",Json::array({{{"Path",Json::array({"Amount"})},{"Equals",3}}})}};
                    else if(op=="SavedItems")data={{"SavedItems",Report(false)}};
                    else if(op=="SavedWorld")data={{"SavedWorld",Report(true)}};
                    else if(op=="Imports")data={{"Folder","runtime/live/jobs/imports/saves"},{"Files",Json::array({{{"Name","Offline preview character"},{"Path","preview-character.json"}}})}};
                    else if(op=="NpcPreview")data={{"NPC",{{"Id","preview-npc"},{"Enabled",false}}},{"Warnings",Json::array({"Sample appearance"})}};
                    else if(op=="CleanupInspect")data={{"Owners",Json::array({{{"Owner","RemovedExampleMod"},{"Count",2},{"Installed",false}}})},{"RegistryReady",false}};
                    else if(op=="CleanupPreview")data={{"Token","preview-only"},{"Removed",Json::array({{{"Owner","RemovedExampleMod"},{"Path","Inventory[0]"}}})}};
                    else if(op=="NiagaraPresets")data={{"Documents",Json::array({{{"Name","Sample fire effect"},{"Attachments",Json::array()}}})}};
                    else if(op=="SaveEditorList")data={{"Runtime","Game Pass/WinGDK"},{"AttachAvailable",false},{"Files",Json::array({{{"Name","Example character"},{"Path","preview-character.json"},{"Storage","Imported JSON payload"}}})},
                        {"Mods",Json::array({{{"Mod","ExampleMod"},{"Source","examplemount"},{"Root","Example RuneSchema/mods/ExampleMod"},{"Registered",true}}})}};
                    else if(op=="SaveEditorOpen"){Editor.Open(Character);data=Editor.Browse();}
                    else if(op=="SaveEditorAppearance"||op=="SaveEditorAppearanceSet"||op=="SaveEditorRename"){
                        if(op=="SaveEditorAppearanceSet")Editor.SetAppearance(args.at("Field"),args.at("Row"),args.at("Revision"),{"ExampleRow","AnotherRow"});
                        if(op=="SaveEditorRename")Editor.Rename(args.at("Name"),args.at("Revision"));
                        data=Editor.Appearance();data["Choices"]["Fields"]=Json::array();for(const auto& field:CharacterWorkbench::Fields)data["Choices"]["Fields"].push_back({{"Key",field.Key},{"Label",field.Label},{"Rows",{"ExampleRow","AnotherRow"}}});
                    }
                    else if(op=="SaveEditorSlotItems"){
                        const auto slot=args.at("Slot").get<std::string>();data={{"Slot",slot},{"HasMore",false},{"NextOffset",1},
                            {"Rows",Json::array({{{"Name","Compatible native fixture"},{"Path","/Preview/Fit.Item"},{"SlotStrategy",slot},{"NativeSlotVerified",true}}})}};
                    }
                    else if(op=="SaveEditorRender")throw std::runtime_error("Synthetic UI fixture: native game rendering is tested separately in the private game copy.");
                    else if(op=="SaveEditorBrowse")data=Editor.Browse(args.value("Pointer",std::string{}),args.value("Offset",size_t{}));
                    else if(op=="SaveEditorSet"){Editor.Set(args.at("Pointer"),args.at("Value"),args.at("Revision"));data=Editor.Browse("/GameProgress");}
                    else if(op=="SaveEditorInventory"||op=="SaveEditorStack"||op=="SaveEditorRemoveItems"){
                        if(op=="SaveEditorStack")Editor.SetCount(args.at("Pointer"),args.at("Count"),args.at("Revision"),100);
                        if(op=="SaveEditorRemoveItems")Editor.RemoveItems(args.at("Pointers"),args.at("Revision"),args.at("Confirm"));
                        data=Editor.Inventory(args.value("Offset",size_t{}));for(auto& row:data["Items"]){const bool mod=row.at("PersistenceID")=="example-modded-id";
                            row["DisplayName"]=mod?"Enchanted rune bars":"Iron helm";row["SourceName"]=mod?"ExampleMod":"Base game";row["MaxStackSize"]=mod?100:1;row["Category"]=mod?"Materials":"Armour";}
                    }
                    else if(op=="SaveEditorPreview")data=Editor.Preview(Character);
                    else if(op=="SaveEditorExport"){auto copy=Editor.Consume(Character,args.at("Token"),args.at("Confirm"));data={{"File","Preview-only edited copy (no file written)"},{"Document",copy}};}
                    else if(op=="SaveEditorClose"){Editor.Reset();data=Json::object();}
                    else throw std::runtime_error("Preview fixture has no native operation; game actions disabled.");
                    Host.Complete(work->at("RequestId"),data,"Sample data only; game actions disabled.");
                }catch(const std::exception& error){Host.Complete(work->at("RequestId"),{{"Error",error.what()}},error.what(),false);}
            }
            reply=Host.Snapshot(request.value("Revision",uint64_t{}));return true;
        };
    }
    ~AuthoringPreviewFixture(){AuthoringClient::Clear();}
};
}
