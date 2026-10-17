#include "Generator/F2CatalogPlan.h"
#include <cassert>
using namespace PS::F2Catalog;
int main(){
    const auto ore=MountedCandidate("/Game/Gameplay/World/Mining/BP_OreNode_Test.BP_OreNode_Test","BP_OreNode_Test","Blueprint");
    assert(ore&&ore->kind==Kind::Resource&&ore->path.ends_with(".BP_OreNode_Test_C"));
    const auto tree=MountedCandidate("/Game/Gameplay/World/Trees/BP_Tree.BP_Tree_C","BP_Tree_C","BlueprintGeneratedClass");
    assert(tree&&tree->kind==Kind::Resource&&!tree->path.ends_with("_C_C"));
    const auto enemy=MountedCandidate("/Game/Gameplay/AI/BP_Goblin.BP_Goblin","BP_Goblin","Blueprint");
    assert(enemy&&enemy->kind==Kind::Enemy);
    const auto npc=MountedCandidate("/Game/Gameplay/NPCs/BP_Trader.BP_Trader","BP_Trader","Blueprint");
    assert(npc&&npc->kind==Kind::Npc);
    const auto item=MountedCandidate("/Mod/Items/Equipment.Equipment","Equipment","ArmorItemData");
    assert(item&&item->kind==Kind::Item);
    const auto wearable=MountedCandidate("/RuneSchema/MaxCape/Items/maxcape.maxcape","maxcape","WearableEquipmentData");
    assert(wearable&&wearable->kind==Kind::Item);
    const auto held=MountedCandidate("/Custom/Items/Blade.Blade","Blade","/Script/Dominion.HeldEquipmentData");
    assert(held&&held->kind==Kind::Item);
    assert(!MountedCandidate("/Game/Art/UI/T_Tree.T_Tree","T_Tree","Texture2D"));
    assert(!MountedCandidate("not a path","ITEM_Test","ItemData"));
    // Unknown directories are not invented resource classes. Explicit authored
    // definitions and manual full classification remain separate discovery paths.
    assert(!MountedCandidate("/Mod/Unknown.BP_Unknown","BP_Unknown","Blueprint"));
}
