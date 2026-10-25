#include "Core/JsonArrayPatch.h"
#include "Core/JsonPatchDirective.h"
#include "Loader/ItemIdentity.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
using nlohmann::json;
namespace fs = std::filesystem;
using namespace DragonWilds;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
json read(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Missing tutorial file: " + path.string());
    return json::parse(input, nullptr, true, true);
}
int main(int argc, char** argv) try {
    require(argc == 2, "Expected tutorial root");
    const fs::path root = argv[1];
    int files = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.path().extension() == ".jsonc") { read(entry.path()); ++files; }
    }
    require(files == 10, "Update coverage when adding tutorial fixtures");
    const auto chest = read(root / "BiggerChests/blueprints/10-ash-chest.jsonc");
    const auto exportChest = read(root / "exports/ash-chest.fmodel.jsonc").at(0);
    require(chest.contains("BP_BaseBuilding_Chest_Small_C"), "Chest class mismatch");
    const auto& component = chest.at("BP_BaseBuilding_Chest_Small_C").at(exportChest.at("Name").get<std::string>());
    require(component.at("MaxSlotCount") == 56 && component.at("bSupportsSortAndFillStacks") == true, "Chest values mismatch");
    for (const auto& [key, ignored] : component.items())
        require(exportChest.at("Properties").contains(key), "Chest patch uses a field absent from the research fixture");

    const auto extra = read(root / "MoreAbyssalDrops/raw/10-add-drop.jsonc");
    const auto append = JsonPatchDirective::Parse(extra, {}, "documented raw append");
    require(append.has_value(), "Raw row envelope missing");
    const auto& drops = append->Changes.at("$Append").at("Resources");
    require(drops.is_array() && drops.size() == 1, "Expected exactly one additive drop");
    const auto& drop = drops.at(0);
    auto nativeDrop = read(root / "exports/loot-table.fmodel.jsonc").at(0).at("Rows").at("AbyssalDemon").at("Resources").at(0);
    const auto reference = nativeDrop.at("SpawnedItemData").at("ObjectPath").get<std::string>();
    const auto path = drop.at("SpawnedItemData").get<std::string>();
    require(reference.substr(0, reference.rfind('.')) == path.substr(0, path.rfind('.')), "FModel reference normalization mismatch");
    nativeDrop["SpawnedItemData"] = path;
    require(nativeDrop == drop, "Append example differs from documented source fields");

    const auto tune = JsonPatchDirective::Parse(read(root / "TuneAbyssalDrops/raw/10-tune-existing-drop.jsonc"), {}, "documented raw tune");
    require(tune && tune->Reference == append->Reference, "Loot row targets differ");
    const auto edits = JsonArrayPatch::Parse(tune->Changes.at("Resources"));
    require(edits.size() == 1 && edits[0].Match.at("SpawnedItemData") == path, "Wrong drop selected");
    require(JsonArrayPatch::Select(edits[0], 1, [&](int) { return edits[0].Match.at("SpawnedItemData") == drop.at("SpawnedItemData"); }) == 0, "Loot selector failed");
    require(edits[0].Target.at("MinimumDropAmount") == 2 && edits[0].Target.at("MaximumDropAmount") == 3, "Loot tuning mismatch");

    const std::array<std::string_view, 2> identities{"PersistenceID", "InternalName"};
    const auto item = read(root / "PatchWood/assets/10-stack-size.jsonc");
    const auto patch = JsonPatchDirective::Parse(item.begin().value(), identities, "documented item");
    require(patch && patch->Changes == json{{"MaxStackSize", 100}}, "Item patch changes more than stack size");
    const auto cloneFile = read(root / "CopyWood/assets/10-ash-variant.jsonc");
    const auto& clone = cloneFile.begin().value();
    require(clone.at("$Clone") == patch->Reference, "Clone and item patch donors differ");
    require(IsCanonicalPersistenceId(clone.at("PersistenceID").get<std::string>()), "Noncanonical clone demonstration ID");
    const auto internal = clone.at("InternalName").get<std::string>();
    require(cloneFile.begin().key() == "/Game/RuneSchema/CopyWood/Items/" + internal + "." + internal, "Clone destination disagrees with runtime identity");

    const auto inventory = read(root / "BiggerInventory/blueprints/10-inventory-pages.jsonc");
    const auto& base = inventory.at("/Game/Gameplay/Character/Components/BP_Components_Inventory");
    const auto& owner = inventory.at("/Game/Gameplay/Character/Player/BP_PlayerController").at("BP_Components_Inventory");
    require(base == owner && base.at("MaxSlotCount") == 312, "Inventory component/controller rules differ");
    const auto pageEdits = JsonArrayPatch::Parse(base.at("InventoryPages"));
    require(pageEdits.size() == 4, "Expected four semantic page edits");
    int total = 0;
    for (const auto& edit : pageEdits) {
        require(!edit.Index && edit.Match.contains("SlotType"), "Inventory should select page meaning, not index");
        total += edit.Target.at("NumSlots").get<int>();
    }
    require(total == 304, "Do not invent an explanation for overall 312 capacity");
    std::cout << files << " JSONC tutorial files parsed; row, array, identity and source tie-ins passed. Live reflection/gameplay not tested.\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
