#include "Core/ItemCatalogFilter.h"
#include "Core/FocusedItemQuery.h"
#include "Core/LoadoutEligibility.h"
#include "Core/ItemTransfer.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace PS::ItemCatalogFilter;
template<class F> void Reject(F fn){bool failed=false;try{fn();}catch(...){failed=true;}assert(failed);}
int main(int argc,char** argv){
    for(const auto slot:PS::LoadoutEligibility::Slots){
        const nlohmann::json row={{"Path","/Game/Native.Item"},{"SlotStrategy",slot},{"NativeSlotVerified",true}};
        assert(PS::LoadoutEligibility::Matches(row,slot));
        auto missing=row;missing.erase("NativeSlotVerified");assert(!PS::LoadoutEligibility::Matches(missing,slot));
        assert(PS::LoadoutEligibility::Matches(PS::ItemTransfer::Compact(row),slot));
    }
    for(const auto strategy:{"HeldOnlyLeft","HeldOnlyRight","HeldTwoHanded","HeldAny"}){
        const nlohmann::json row={{"SlotStrategy",strategy},{"NativeSlotVerified",true}};
        assert(PS::LoadoutEligibility::Matches(row,"HeldLeft")==(std::string(strategy)=="HeldOnlyLeft"||std::string(strategy)=="HeldAny"));
        assert(PS::LoadoutEligibility::Matches(row,"HeldRight")!=(std::string(strategy)=="HeldOnlyLeft"));
        assert(!PS::LoadoutEligibility::Matches(row,"Ammo"));
    }
    assert(!PS::LoadoutEligibility::Matches({{"Name","Fire Rune"},{"NativeSlotVerified",true}},"MagicAmmo1"));
    assert(!PS::LoadoutEligibility::Matches({{"SlotStrategy","MagicAmmo1"},{"NativeSlotVerified",true}},"Ammo"));
    assert(!PS::LoadoutEligibility::Matches({{"SlotStrategy","CrossbowBolts"},{"NativeSlotVerified",true}},"FishingBait"));
    assert(!PS::LoadoutEligibility::Valid("Invented"));
    const nlohmann::json nativeRune={{"Path","/Game/Runes.Fire"},{"NativeSlotVerified",true},{"NativeMagicAmmo",true}};
    assert(PS::LoadoutEligibility::Matches(nativeRune,"MagicAmmo1"));
    assert(!PS::LoadoutEligibility::Matches(nativeRune,"Ammo"));
    assert(!PS::LoadoutEligibility::Matches(nativeRune,"HeldRight"));
    assert(PS::LoadoutEligibility::Matches(PS::ItemTransfer::Compact(nativeRune),"MagicAmmo1"));
    const nlohmann::json helm={{"Path","/Dye/Armor/ITEM_Helm.ITEM_Helm"},{"Name","Fallen Hoplite's Helm (Dyed Chrome)"},
        {"Category","Armor"},{"SourceKind","modded"},{"SourceName","Dye"},{"InternalName","chrome_helm"},{"Tags","Metal Heavy"},{"Class","/Script/Dominion.ItemData"}};
    assert(Query("")(helm));assert(Query("Chrome helm")(helm));assert(Query("metal Heavy")(helm));assert(Query("dominion")(helm));
    assert(Query("__helpysource:modded __helpycat:armor __helpyfamily:fallen_hoplite_dyed_chrome chrome")(helm));
    const auto ownedQuery=PS::ItemSource::Query(PS::ItemSource::Kind::Modded,"Armor","__helpyfamily:fallen_hoplite_dyed_chrome chrome","mods/dye");
    assert(Query(ownedQuery)(helm));
    auto otherOwner=helm;otherOwner["SourceName"]="Other";assert(!Query(ownedQuery)(otherOwner));
    auto ambiguous=helm;ambiguous["SourceName"]="";assert(!Query(ownedQuery)(ambiguous));
    std::string spaced=PS::ItemSource::ProviderToken("mods/my mod")+"test";
    assert(PS::ItemSource::TakeProvider(spaced)=="mods/my mod"&&spaced=="test");
    std::string longest=PS::ItemSource::ProviderToken("plugins/"+std::string(192,'a'));assert(!PS::ItemSource::TakeProvider(longest).empty()&&longest.empty());
    Reject([]{PS::ItemSource::ProviderToken("mods/../bad");});
    Reject([]{std::string query="__helpyowner:zz";PS::ItemSource::TakeProvider(query);});
    Reject([]{std::string query="__helpyowner:0";PS::ItemSource::TakeProvider(query);});
    auto nativeHelm=helm;nativeHelm["NativeSlotVerified"]=true;nativeHelm["SlotStrategy"]="Head";
    assert(Query("__helpyslot:Head __helpysource:modded __helpycat:armor __helpyfamily:fallen_hoplite_dyed_chrome chrome")(nativeHelm));
    assert(!Query("__helpyslot:Head")(helm));
    assert(!Query("__helpyslot:Body")(nativeHelm));
    Reject([]{Query invalid("__helpyslot:Invented");});
    const nlohmann::json necklace={{"SlotStrategy","Necklace"},{"NativeSlotVerified",true}};
    assert(Query("__helpyslot:Trinket")(necklace)&&Query("__helpyslot:Trinket2")(necklace));
    assert(!Query("__helpysource:vanilla")(helm));assert(!Query("__helpycat:weapons")(helm));assert(!Query("__helpyfamily:fallen_hoplite_dyed_gold")(helm));assert(!Query("chrome wood")(helm));
    auto sword=helm;sword["Name"]="Bronze sword";sword["SourceKind"]="vanilla";sword["Category"]="Weapons";sword["Tags"]="Item.Equipment.Weapon.1H.Sword ItemFilter.Type.Equipment.Weapon.Melee";
    assert(Query("__helpysource:vanilla __helpycat:weapons bronze")(sword));
    assert(!Query("")(nullptr));Reject([&]{Query invalid("__helpysource:invented");});Reject([&]{Query invalid(std::string(513,'x'));});
    auto health=helm;health["Name"]="Health Tincture";health["PersistenceID"]="tincture-health-42";
    assert(Query("Health")(health)&&Query("tincture health")(health)&&Query("health-42")(health));
    nlohmann::json stone={{"Path","/Game/Gameplay/Items/Resources/Mineral/ITEM_Resources_Stone.ITEM_Resources_Stone"},
        {"Name","Stone"},{"InternalName","res_stone"},{"Category","Building"},{"SourceKind","vanilla"},
        {"Tags","Item.BuildingResource ItemFilter.Material.BasicMaterial ItemFilter.Material.Mineral"}};
    assert(Query("Stone")(stone)&&Query("__helpycat:resources stone")(stone));
    assert(!Query("__helpycat:building stone")(stone));
    assert(PS::ItemCategory::Repair(stone)&&stone["Category"]=="Resources");
    assert(!PS::ItemCategory::Repair(stone));
    auto rune=stone;rune["Name"]="Air Rune";rune["Tags"]="ItemFilter.Type.Equipment.Ammo.Rune";
    assert(PS::ItemCategory::Resolve(rune)=="Runes"&&Query("__helpycat:runes air")(rune));
    auto tool=sword;tool["Name"]="Bronze Pickaxe";tool["Tags"]="Item.Equipment.Tool.Pickaxe";
    assert(PS::ItemCategory::Resolve(tool)=="Tools");
    auto runeSword=sword;runeSword["Name"]="Rune Sword";
    assert(PS::ItemCategory::Resolve(runeSword)=="Weapons");
    assert(PS::ItemCategory::Classify("/Game/Mods/RuneSchema/Armor/Snapshot.Snapshot","Snapshot","","","").empty());
    assert(PS::ItemCategory::Classify("/Mods/Armor/DA_Flower.DA_Flower","Flower","","","").empty());
    assert(PS::ItemCategory::Classify("/Mods/DA_Test.DA_Test","Test","","Item.QuestItemExtra","").empty());
    assert(PS::ItemCategory::Classify("/Mods/DA_Test.DA_Test","Rune Potion","","ItemFilter.Type.Consumable.Potion","")=="Consumables");
    assert(PS::ItemCategory::Classify("/Mods/ITEM_Rune_Sword.ITEM_Rune_Sword","Rune Sword","","","")=="Weapons");
    assert(PS::ItemCategory::Classify("/Mods/ITEM_Rune_Air.ITEM_Rune_Air","Air Rune","","","")=="Runes");
    assert(PS::ItemCategory::Classify("/Mods/ITEM_Arrow.ITEM_Arrow","Arrow","","","")=="Ammo");
    PS::FocusedItemQuery::Misses misses;
    const nlohmann::json pending={{"Rows",nlohmann::json::array()},{"HasMore",true}};
    const nlohmann::json empty={{"Rows",nlohmann::json::array()},{"HasMore",false}};
    const nlohmann::json found={{"Rows",nlohmann::json::array({health})},{"HasMore",true}};
    assert(!misses.NeedsLive("scope1","Health",pending));
    assert(!misses.NeedsLive("scope1","Health",found));
    assert(!misses.NeedsLive("scope1","Health",empty)); // Empty tail is not a cache miss.
    assert(misses.NeedsLive("scope1","uncached",empty));
    assert(!misses.NeedsLive("scope1","",empty)); // Browsing never triggers a focused lookup.
    assert(misses.NeedsLive("scope2","Health",empty)); // World/source changes discard old hit state.
    for(int i=1;i<argc;++i){
        std::ifstream input(argv[i]);const auto doc=nlohmann::json::parse(input);
        size_t corrected=0,stones=0;const auto& rows=doc.at("Rows");nlohmann::json changes=nlohmann::json::object();
        for(const auto& row:rows){
            const auto category=PS::ItemCategory::Resolve(row),old=PS::ItemCategory::Text(row,"Category");
            if(category!=old){
                ++corrected;auto& change=changes[old+" -> "+category];if(change.is_null())change={{"Count",0},{"Examples",nlohmann::json::array()}};
                change["Count"]=change["Count"].get<size_t>()+1;
                if(change["Examples"].size()<2)change["Examples"].push_back(PS::ItemCategory::Text(row,"Name"));
            }
            if(PS::ItemCategory::Text(row,"Name")=="Stone"){assert(Query("stone")(row)&&Query("__helpycat:resources stone")(row));++stones;}
        }
        assert(stones==1);
        std::cout<<rows.size()<<" cached rows checked; Stone found in All and Resources; "<<corrected<<" category corrections.\n";
        std::cout<<changes.dump(2)<<"\n";
    }
}
