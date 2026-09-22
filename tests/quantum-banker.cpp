#include "Loader/NpcCatalog.h"
#include "Loader/DialogueDefinition.h"
#include "Loader/EventIdentity.h"
#include "Loader/QuestDefinition.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <set>
using Json=nlohmann::json;
void Require(bool value){if(!value)throw std::runtime_error("Quantum Banker package invariant failed");}
template<class F>bool Rejects(F fn){try{fn();return false;}catch(const std::exception&){return true;}}
Json Read(const std::filesystem::path& path){std::ifstream stream(path);Require(stream.good());return Json::parse(stream,nullptr,true,true);}
int main(int argc,char** argv){
    Require(argc==2);const std::filesystem::path root=argv[1];
    auto npc=Read(root/"RuneSchema2VendorTest/npc/50-QuantumBanker.jsonc");
    DragonWilds::NpcCatalog catalog;catalog.AddNpc("RuneSchema2VendorTest",npc);
    Require(npc.at("Type")=="Prop" && !npc.value("HideMesh",false) && npc.at("EnableCollision")==true);
    Require(npc.at("Mesh")=="/Game/Art/Skeleton/Weapons/Melee/Sword_Steel_1H_01/SM_Sword_Steel_1H_01.SM_Sword_Steel_1H_01");
    Require(npc.at("Location").at(2)=="$+100" && !npc.contains("VisualEffect"));
    Require(npc.at("VendorID")=="quantum_exchange");
    // Keep validation coverage for hidden Niagara props independently of the example's visual.
    npc["HideMesh"]=true;
    npc["VisualEffect"]={{"Type","Niagara"},{"System","/Agility/Art/VFX/NS_Orb_Ready_01.NS_Orb_Ready_01"},{"Parameters",{{"User.Destination_VFX",true}}}};
    auto invalid=npc;invalid["Type"]="Resource";
    Require(Rejects([&]{DragonWilds::NpcCatalog other;other.AddNpc("Test",invalid);}));
    for(const auto& value:{Json("true"),Json::array({1,2,3}),Json{{"R",1},{"G",0},{"B",0}}}) {
        invalid=npc;invalid["VisualEffect"]["Parameters"]["User.Test"]=value;
        Require(Rejects([&]{DragonWilds::NpcCatalog other;other.AddNpc("Test",invalid);}));
    }
    invalid=npc;invalid["VisualEffect"]["LocationOffset"]={{"X",0},{"Y",0}};
    Require(Rejects([&]{DragonWilds::NpcCatalog other;other.AddNpc("Test",invalid);}));
    invalid=npc;invalid["VisualEffect"]["System"]="not an asset";
    Require(Rejects([&]{DragonWilds::NpcCatalog other;other.AddNpc("Test",invalid);}));
    invalid=npc;invalid["VisualEffect"]["Unknown"]=true;
    Require(Rejects([&]{DragonWilds::NpcCatalog other;other.AddNpc("Test",invalid);}));
    const auto story=DragonWilds::Dialogue::Parse("RuneSchema2VendorTest",Read(root/"RuneSchema2VendorTest/dialogue/50-QuantumBanker.jsonc"));
    Require(story.Key=="RuneSchema2VendorTest:quantum_banker_story");
    Require(story.Stores.contains("RuneSchema2VendorTest:quantum_exchange"));
    const std::string mod="RuneSchema2VendorTest";
    const auto mortimerNpc=Read(root/"RuneSchema2VendorTest/npc/60-Mortimer.jsonc");
    catalog.AddNpc(mod,mortimerNpc);
    Require(mortimerNpc.at("VisualEffect").at("Type")=="Ghost Glow");
    Require(mortimerNpc.at("VisualEffect").at("Overlay")==true && mortimerNpc.at("VisualEffect").at("BodyMaterial")==false);
    const auto necromancer=DragonWilds::Dialogue::Parse(mod,Read(root/"RuneSchema2VendorTest/dialogue/60-Mortimer.jsonc"));
    Require(necromancer.Data.at("Nodes").at("returned").at("Choices").at(2).at("WhenTimeOfDay")=="Day");
    Require(necromancer.Data.at("Nodes").at("remembered").at("Choices").at(1).at("WhenTimeOfDay")=="Night");
    const auto firepitDocument=Read(root/"RuneSchema2VendorTest/spawns/62-NightFirepit.jsonc");
    Require(firepitDocument.is_array() && firepitDocument.size()==1);
    const auto& firepit=firepitDocument.at(0);
    Require(firepit.at("Type")=="BuildingProp" && firepit.at("TimeOfDay")=="Night");
    Require(firepit.at("AllowDeconstruction")==true
        && firepit.at("Building")=="/Game/Gameplay/BaseBuilding/Data/BuildingPieces/Furniture/Comfort/BUILDPIECE_Campfire.BUILDPIECE_Campfire");
    std::map<std::string,DragonWilds::Events::SpawnTemplate> spawns;
    for(const auto& entry:Read(root/"RuneSchema2VendorTest/spawns/60-MortimerZombies.json")) {
        auto parsed=DragonWilds::Events::ParseSpawn(mod,entry);
        DragonWilds::Events::ValidateIdentity(DragonWilds::Events::DecodeIdentity(DragonWilds::Events::EncodeIdentity(parsed)),parsed);
        if(parsed.Key==mod+":rs_mortimer_giant" || parsed.Key==mod+":rs_quantum_ghost_giant")Require(parsed.Scale==2.0);
        Require(spawns.emplace(parsed.Key,parsed).second);
    }
    for(const auto& entry:Read(root/"RuneSchema2VendorTest/spawns/90-GrumbleGoblins.json")) {
        auto parsed=DragonWilds::Events::ParseSpawn(mod,entry);
        Require(spawns.emplace(parsed.Key,parsed).second);
    }
    std::set<std::string> events;
    for(const auto& entry:Read(root/"RuneSchema2VendorTest/events/60-MortimerEncounters.json")) {
        const auto event=DragonWilds::Events::Parse(mod,entry);Require(events.insert(event.Key).second);
        for(const auto& wave:event.Waves)for(const auto& member:wave) {
            Require(member.Ground && spawns.contains(member.Spawn));
            Require(member.GroundOffset==(entry.at("Id")=="mortimer_scattered_debtors"?250:500));
        }
    }
    {
        const auto event=DragonWilds::Events::Parse(mod,Read(root/"RuneSchema2VendorTest/events/90-GrumbleGoblinWaves.json"));
        Require(events.insert(event.Key).second);
        for(const auto& wave:event.Waves)for(const auto& member:wave)Require(member.Ground && spawns.contains(member.Spawn));
    }
    for(const auto& event:necromancer.Events)Require(events.contains(event));
    for(const auto& event:story.Events)Require(events.contains(event));
    Require(necromancer.Quests==std::set<std::string>{mod+":mortimer_debtors"});
    Require(story.Quests==std::set<std::string>{mod+":mortimer_debtors"});
    const auto debtors=DragonWilds::Quests::Parse(mod,Read(root/"RuneSchema2VendorTest/quests/60-MortimerDebtors.json"));
    Require(debtors.Stages.size()==2 && debtors.Stages[0].second.size()==3 && debtors.Stages[1].second.size()==1);
    Require(debtors.Story && !debtors.Repeat.Enabled && !debtors.AutomaticReward);
    for(const auto& objective:debtors.Stages[0].second) {
        Require(objective.Kill && objective.Kill->IncludeDerived
            && objective.Kill->EventKey==mod+":mortimer_scattered_debtors" && !objective.Kill->SpawnKey.empty());
        Require(objective.Marker && objective.Marker->RadiusMeters==15);
    }
    const auto& giant=debtors.Stages[1].second[0];
    Require(giant.Kill && giant.Kill->IncludeDerived && giant.Kill->EventKey==mod+":mortimer_giant_debtor"
        && giant.Kill->SpawnKey==mod+":rs_mortimer_giant"
        && giant.Marker && giant.Marker->RadiusMeters==25);
    const auto patrol=DragonWilds::Quests::Parse(mod,Read(root/"RuneSchema2VendorTest/quests/60-GraniteGoblinPatrol.jsonc"));
    Require(patrol.Stages.size()==1 && patrol.Stages[0].second.size()==1);
    const auto& patrolObjective=patrol.Stages[0].second[0];
    Require(patrolObjective.Kill && patrolObjective.Kill->EventKey==mod+":grumble_goblin_waves" && patrolObjective.Kill->SpawnKey.empty());
    Require(patrolObjective.Marker && patrolObjective.Marker->RadiusMeters==20);
    const auto loot=Read(root/"RuneSchema2VendorTest/raw/60-MortimerLoot.json");
    for(const auto& [key,spawn]:spawns)if(!spawn.LootRow.empty())Require(loot.at("DT_EnemyLootDropTable").contains(spawn.LootRow));
    const auto& gold=loot.at("DT_LootDropTable").at("RS_Quantum_GiantGold").at("Resources")[0];
    Require(gold.at("DropChance")==100 && gold.at("bAutoAddToInventory")==false);
    Require(loot.at("DT_LootDropTable").at("RS_Quantum_GhostEmpty").at("Resources").empty());
    const auto store=Read(root/"RuneSchema2VendorTest/vendors/50-QuantumExchange.json");
    Require(store.at("Items").size()==4);
    for(const auto& offer:store.at("Items")) {
        DragonWilds::VendorOffers::Properties(offer);
        Require(DragonWilds::VendorOffers::Category(offer)=="Currency");
    }
    const auto grumble=DragonWilds::Dialogue::Parse("RuneSchema2VendorTest",Read(root/"RuneSchema2VendorTest/dialogue/40-Grumble.jsonc"));
    const auto& choices=grumble.Data.at("Nodes").at("hello").at("Choices");
    Require(choices.at(1).at("WhenQuest").at("States")==Json::array({"Active"}));
    const auto coins=Read(root/"Currency/assets/00-Coins.jsonc");
    const auto original=Read(root/"Currency/raw/30-CoinDrops.jsonc");
    const auto extra=Read(root/"Currency/raw/ZZ-AllLootCoins.jsonc");
    Require(extra.at("DT_LootDropTable").size()==137 && extra.at("DT_LootChest_Sets").size()==68);
    size_t added=0;
    for(const auto& [table,rows]:extra.items())for(const auto& [name,patch]:rows.items()) {
        Require(patch.at("$PatchOnly")==true);
        const auto field=table=="DT_LootDropTable"?"Resources":"SpawnableItems";
        std::set<std::string> seen;
        if(original.contains(table) && original.at(table).contains(name))
            for(const auto& drop:original.at(table).at(name).at("$Append").at(field))seen.insert(drop.at("SpawnedItemData").get<std::string>());
        for(const auto& drop:patch.at("$Append").at(field)) {
            const auto path=drop.at("SpawnedItemData").get<std::string>();
            Require(coins.contains(path) && seen.insert(path).second);
            Require(drop.at("MinimumDropAmount").get<int>()>=1 && drop.at("MaximumDropAmount").get<int>()>=drop.at("MinimumDropAmount").get<int>());
            Require(drop.at("DropChance").get<double>()>0 && drop.at("DropChance").get<double>()<=100);++added;
        }
        Require(seen.size()==3);
    }
    Require(added==606);
}
