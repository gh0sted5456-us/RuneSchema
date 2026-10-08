#include "Generator/AssetTemplate.h"
#include "Core/JsonPatchDirective.h"
#include <cassert>
using nlohmann::json;
template<class F> void Rejects(F operation){bool caught=false;try{operation();}catch(const std::exception&){caught=true;}assert(caught);}
int main() {
    using namespace PS::AssetTemplate;
    assert(Matches("Goblin Pack","/Game/Items/ITEM_Consumable_GoblinPack"));
    assert(Matches("PACK goblin","Goblin_Pack"));
    assert(!Matches("Goblin Gold","Goblin Pack"));assert(!Matches("!!","Goblin"));
    const std::string path="/Game/Items/Goblin.Goblin";
    const json fields={{"Name","Goblin Pack"},{"Weight",1},{"PersistenceID","source"},{"InternalName","source"}};
    const auto edit=Build(path,fields,0,"","","");assert(!edit[path].contains("PersistenceID"));
    const auto patch=Build(path,fields,1,"","","");
    const auto directive=DragonWilds::JsonPatchDirective::Parse(patch["Patch"],{},"asset");
    assert(directive && directive->Reference==path && directive->Changes["Weight"]==1);
    const auto clone=Build(path,fields,2,"MyMod","MyItem","AAAAAAAAAAAAAAAAAAAAAQ");
    const auto& body=clone.at("/Game/RuneSchema/MyMod/Items/MyItem.MyItem");
    assert(body["$Clone"]==path && body["InternalName"]=="MyItem" && body["PersistenceID"]!="source");
    const json unlockFields = {{"RecipesToUnlock", json::array({"/Game/Recipes/Test.Test"})}, {"BuildingPieceToUnlock", nullptr}};
    const auto unlockClone = Build(path, unlockFields, 2, "MyMod", "Scroll", "AAAAAAAAAAAAAAAAAAAAAQ");
    const auto& unlockBody = unlockClone.at("/Game/RuneSchema/MyMod/Items/Scroll.Scroll");
    assert(unlockBody.at("RecipesToUnlock") == unlockFields.at("RecipesToUnlock"));
    assert(unlockBody.at("BuildingPieceToUnlock").is_null());
    const auto clearClone = Build(path, {{"RecipesToUnlock",json::array()}}, 2, "MyMod", "Scroll", "AAAAAAAAAAAAAAAAAAAAAQ");
    assert(clearClone.begin().value().at("RecipesToUnlock").empty());
    Rejects([&]{Build(path,fields,2,"../Mod","item","AAAAAAAAAAAAAAAAAAAAAQ");});
    Rejects([&]{Build(path,fields,2,"Mod","item","");});
    Rejects([&]{Build(path,fields,2,"Mod","item","AAAAAAAAAAAAAAAAAAAAAB");});
    Rejects([&]{Build(path,{{"$Clone","bad"}},0,"","","");});
    Rejects([&]{Build(path,fields,3,"","","");});
}
