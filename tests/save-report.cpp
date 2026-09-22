#include "Generator/SaveReport.h"
#include "Generator/NpcSaveExport.h"
#include <cassert>
using nlohmann::json;
int main(){
    {
        json source=json::parse(R"({"meta_data":{"char_name":"Test"},"GameProgress":{"Inventory":{"3":{"ItemData":"staff"}},"Loadout":{"0":{"ItemData":"helmet"},"5":{"PlayerInventoryItemIndex":3}}}})");
        for(const auto* key:DragonWilds::HumanNpc::AppearanceKeys)source["Customization"]["CustomizationData"][key]={{"rowName","Row"}};
        const auto original=source;
        auto preview=PS::NpcSaveExport::Preview(source);
        assert(preview["Rows"].size()==2 && preview["NPC"]["Enabled"]==false);
        preview["Rows"][0]["Slot"]="Head";preview["Rows"][0]["AssetPath"]="/Game/Test/Helmet.Helmet";
        preview["Rows"][1]["Slot"]="HeldTwoHanded";preview["Rows"][1]["AssetPath"]="/Game/Test/Staff.Staff";
        PS::NpcSaveExport::ApplyResolved(preview);
        assert(preview["NPC"]["Equipment"]["MainHand"]=="/Game/Test/Staff.Staff");
        auto offhand=PS::NpcSaveExport::Preview(source);
        offhand["Rows"][0]["Slot"]="HeldOnlyLeft";offhand["Rows"][0]["AssetPath"]="/Game/Test/Shield.Shield";
        PS::NpcSaveExport::ApplyResolved(offhand);
        assert(offhand["NPC"]["Equipment"]["OffHand"]=="/Game/Test/Shield.Shield");
        assert(preview["NPC"]["Appearance"].size()==8 && source==original);
        assert(!preview["NPC"].contains("GameProgress"));
        auto unresolved=PS::NpcSaveExport::Preview(source);PS::NpcSaveExport::ApplyResolved(unresolved);
        assert(unresolved["Warnings"].size()==2 && unresolved["NPC"]["Equipment"].empty());
        auto ambiguous=PS::NpcSaveExport::Preview(source);
        for(auto& row:ambiguous["Rows"])row["Slot"]="HeldOnlyRight";
        ambiguous["Rows"][0]["AssetPath"]="/Game/Test/A.A";ambiguous["Rows"][1]["AssetPath"]="/Game/Test/B.B";
        PS::NpcSaveExport::ApplyResolved(ambiguous);
        assert(!ambiguous["NPC"]["Equipment"].contains("MainHand") && !ambiguous["Warnings"].empty());
        auto missing=source;missing["GameProgress"]["Inventory"].erase("3");
        assert(PS::NpcSaveExport::Preview(missing)["Warnings"].size()==1);
    }
    using namespace PS::SaveReport;
    assert(Guid("{12345678-12345678-12345678-ABCDEF12}")=="123456781234567812345678ABCDEF12");
    assert(Guid("wrong").empty());assert(Guid(std::string(32,'0')).empty());
    const auto save=json::parse(R"({"meta_data":{"private":"not exported"},"GameProgress":{"Inventory":{"0":{"ItemData":"coin","Durability":50}},"PersonalInventory":{"1":{"ItemData":"ring"}},"QuestProgress":{"secret":true}}})");
    const auto rows=Inventory(save);
    assert(rows.size()==2 && rows[0]["PersistenceID"]=="coin" && rows[0]["InternalName"].is_null());
    assert(rows.dump().find("secret")==std::string::npos && rows.dump().find("Durability")==std::string::npos);
    assert(Origin("/Game/RuneSchema/Currency/coin")=="RuneSchema namespace");
    assert(Origin("/Game/Mods/Test/coin").starts_with("Mod namespace"));
    assert(Origin("/Game/Gameplay/coin").starts_with("Other loaded"));
    bool invalid=false;try{Inventory(json::object());}catch(const std::exception&){invalid=true;}assert(invalid);
    assert(Inventory({{"GameProgress",json::object()}}).empty());
}
