#include "Core/CharacterSaveEditor.h"
#include "Core/HelpyAuthoringProtocol.h"
#include <cassert>
using namespace PS::CharacterSaveEditor;
template<class F>void Reject(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}assert(caught);}
int main(){
    Json original={{"GUID","identity"},{"GameProgress",{{"Version",86},{"Health",100},{"SkillXP",{{"Mining",125}}},
        {"Inventory",{{"0",{{"ItemData","modded-item"},{"GUID","item-guid"},{"Amount",3},{"UnknownModField",{{"Keep",true}}}}},{"MaxSlotIndex",0}}},
        {"Progress",{{"RecipesUnlocked",{"modded-recipe"}}}},{"QuestProgress",{{"QuestInts",{{"QuestVariableName","RuneSchema.Owner:Mod"},{"QuestVariableValue",1}}}}}}},
        {"Customization",{{"DisplayName","Before"},{"Unknown",42}}},{"UnknownFutureSection",{{"Keep",true}}}};
    Session session;Reject([&]{session.Open({{"World","SPUD"}});});session.Open(original);
    auto root=session.Browse();assert(root["Rows"].size()==4);
    auto game=session.Browse("/GameProgress");const auto revision=game["Revision"].get<uint64_t>();
    session.Set("/GameProgress/Health",75,revision);
    Reject([&]{session.Set("/GameProgress/Health",60,revision);});
    auto reviewed=session.Preview(original);assert(reviewed["Count"]==1);
    Reject([&]{session.Consume(original,reviewed["Token"],false);});
    reviewed=session.Preview(original);auto copy=session.Consume(original,reviewed["Token"],true);
    assert(copy["GameProgress"]["Health"]==75&&original["GameProgress"]["Health"]==100);
    assert(copy["UnknownFutureSection"]==original["UnknownFutureSection"]&&copy["GameProgress"]["Inventory"]==original["GameProgress"]["Inventory"]);
    Reject([&]{session.Consume(original,reviewed["Token"],true);});
    for(const auto* path:{"/GUID","/GameProgress/Version","/GameProgress/Inventory/0/ItemData","/GameProgress/Inventory/0/GUID","/GameProgress/Inventory/MaxSlotIndex","/GameProgress/QuestProgress/QuestInts/QuestVariableValue"}){
        const auto current=session.Browse()["Revision"].get<uint64_t>();Reject([&]{session.Set(path,1,current);});
    }
    auto values=session.Browse("/GameProgress/Progress/RecipesUnlocked");assert(values["Rows"][0]["Editable"]==false);
    Reject([&]{session.Set("/GameProgress/Progress/RecipesUnlocked/0","different-id",session.Browse()["Revision"]);});
    Reject([&]{session.Set("/GameProgress/Health","not a number",session.Browse()["Revision"]);});
    Reject([&]{session.Set("/GameProgress/Health",1.25,session.Browse()["Revision"]);});
    Reject([&]{session.Set("/GameProgress/Inventory",Json::object(),session.Browse()["Revision"]);});
    Reject([&]{session.Set("/GameProgress/DoesNotExist",1,session.Browse()["Revision"]);});
    reviewed=session.Preview(original);session.Set("/Customization/DisplayName","After",session.Browse()["Revision"]);
    Reject([&]{session.Consume(original,reviewed["Token"],true);});
    reviewed=session.Preview(original);auto changed=original;changed["GameProgress"]["Health"]=99;
    Reject([&]{session.Consume(changed,reviewed["Token"],true);});
    Json many=Json::array();for(int i=0;i<205;++i)many.push_back(i);auto large=original;large["GameProgress"]["Numbers"]=many;session.Open(large);
    assert(session.Browse("/GameProgress/Numbers",100)["Rows"].size()==100&&session.Browse("/GameProgress/Numbers",200)["Rows"].size()==5);
    session.Reset();Reject([&]{session.Browse();});
    const auto request=[](const char* op,Json args){return Json{{"RequestId",uint64_t(1)},{"Generation",uint64_t(1)},{"Operation",op},{"Arguments",args}};};
    Reject([&]{PS::HelpyAuthoring::Validate(request("SaveEditorOpen",{{"Path","save.json"},{"Offline",false}}));});
    Reject([&]{PS::HelpyAuthoring::Validate(request("SaveEditorExport",{{"Token","x"},{"Confirm",false}}));});
    assert(PS::HelpyAuthoring::Validate(request("SaveEditorOpen",{{"Path","save.json"},{"Offline",true}}))["Operation"]=="SaveEditorOpen");
    auto inventory=original;auto& progress=inventory["GameProgress"];
    progress["Inventory"]={{"MaxSlotIndex",10},{"2",{{"ItemData","coin"},{"GUID","keep-instance"},{"Count",20},{"UnknownModField",{{"Keep",true}}}}},{"7",{{"ItemData","helm"},{"GUID","keep-helm"}}}};
    progress["PersonalInventory"]={{"MaxSlotIndex",4},{"1",{{"ItemData","ore"},{"Count",4},{"ModData","preserve elsewhere"}}}};
    progress["Loadout"]={{"MaxSlotIndex",8},{"0",{{"PlayerInventoryItemIndex",2}}},{"5",{{"PlayerInventoryItemIndex",7}}}};
    session.Open(inventory);auto items=session.Inventory();assert(items["Total"]==3&&items["Items"][0]["Slot"]=="2");
    assert(items["Items"][0]["StackEditable"]==true&&items["Items"][0]["Equipped"]==true&&items["Items"][1]["StackEditable"]==false);
    assert(session.ItemIdentity("/GameProgress/Inventory/2")=="coin");
    const auto initial=items["Revision"].get<uint64_t>();
    Reject([&]{session.SetCount("/GameProgress/Inventory/2",25,initial);}); // Unknown native max cannot approve an increase.
    Reject([&]{session.SetCount("/GameProgress/Inventory/2",31,initial,30);});
    session.SetCount("/GameProgress/Inventory/2",25,initial,30);
    Reject([&]{session.SetCount("/GameProgress/Inventory/2",10,initial);}); // Stale revision.
    items=session.Inventory();const auto current=items["Revision"].get<uint64_t>();
    Reject([&]{session.SetCount("/GameProgress/Inventory/7",2,current,20);}); // Never invent a Count.
    Reject([&]{session.SetCount("/GameProgress/Inventory/2",0,current);});
    Reject([&]{session.SetCount("/GameProgress/Loadout/5",1,current);});
    Reject([&]{session.Set("/GameProgress/Inventory/2/Count",-1,current);});
    session.SetCount("/GameProgress/Inventory/2",12,current); // Safe decrease with unresolved metadata.
    reviewed=session.Preview(inventory);assert(reviewed["Summary"][0]["Label"]=="Change stack count");
    copy=session.Consume(inventory,reviewed["Token"],true);
    assert(copy["GameProgress"]["Inventory"]["2"]["GUID"]=="keep-instance"&&copy["GameProgress"]["Inventory"]["2"]["UnknownModField"]==progress["Inventory"]["2"]["UnknownModField"]);
    const auto rev=session.Inventory()["Revision"].get<uint64_t>();const Json selected={"/GameProgress/Inventory/2","/GameProgress/PersonalInventory/1"};
    Reject([&]{session.RemoveItems(selected,rev,false);});
    Reject([&]{session.RemoveItems({"/GameProgress/Inventory/2","/GameProgress/Inventory/2"},rev,true);});
    assert(session.Inventory()["Total"]==3); // A partially validated batch never commits.
    Reject([&]{session.RemoveItems({"/GameProgress/Inventory/2","/GameProgress/Inventory/MaxSlotIndex"},rev,true);});
    Reject([&]{session.Inventory(8193);});
    session.RemoveItems(selected,rev,true);reviewed=session.Preview(inventory);copy=session.Consume(inventory,reviewed["Token"],true);
    assert(copy["GameProgress"]["Inventory"].size()==2&&copy["GameProgress"]["Inventory"].contains("7"));
    assert(copy["GameProgress"]["Inventory"]["MaxSlotIndex"]==10&&copy["GameProgress"]["PersonalInventory"]["MaxSlotIndex"]==4);
    assert(!copy["GameProgress"]["Loadout"].contains("0")&&copy["GameProgress"]["Loadout"]["5"]["PlayerInventoryItemIndex"]==7);
    assert(copy["GUID"]==inventory["GUID"]&&copy["GameProgress"]["QuestProgress"]==progress["QuestProgress"]);
    auto broken=inventory;broken["GameProgress"]["Loadout"]["0"]["PlayerInventoryItemIndex"]="2";session.Open(broken);
    Reject([&]{session.RemoveItems({"/GameProgress/Inventory/2"},session.Inventory()["Revision"],true);});
    assert(session.Inventory()["Total"]==3);
    Reject([&]{PS::HelpyAuthoring::Validate(request("SaveEditorStack",{{"Pointer","/GameProgress/Inventory/2"},{"Count",0},{"Revision",uint64_t(1)}}));});
    Reject([&]{PS::HelpyAuthoring::Validate(request("SaveEditorRemoveItems",{{"Pointers",selected},{"Confirm",false},{"Revision",uint64_t(1)}}));});
    assert(PS::HelpyAuthoring::Validate(request("SaveEditorRemoveItems",{{"Pointers",selected},{"Confirm",true},{"Revision",uint64_t(1)}}))["Operation"]=="SaveEditorRemoveItems");
}
