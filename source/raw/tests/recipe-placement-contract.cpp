#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "FAIL: cannot open " << path << '\n';
        std::exit(1);
    }
    std::ostringstream value;
    value << file.rdbuf();
    return value.str();
}

static void Need(const std::string& text, const std::string& token, const char* message)
{
    if (text.find(token) == std::string::npos) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    const auto loader = Read(argv[1]);
    const auto guide = Read(argv[2]);
    const auto properties = Read(argv[3]);

    Need(loader,
        "const bool hasOneLayout = placement.Category.empty() != placement.Array.empty();",
        "runtime does not enforce one placement layout");
    Need(loader, "PlaceInCategory(recipe, rowStruct.Get(), row, placement.Category)",
        "crafting/merchant category placement disappeared");
    Need(loader, "PlaceInArray(recipe, rowStruct.Get(), row, placement.Array, placement.Replaces)",
        "processing array placement disappeared");
    Need(loader, "LoadAsset_Blocking(soft)",
        "exact custom DataTable targets are no longer loaded on demand");
    Need(loader, "PlaceForTable(datatable)",
        "late-serialized DataTables are no longer replayed");
    Need(loader, "recipe->IsA(acceptedClass)",
        "processing array writes are not type checked");
    Need(loader, "InspectRuntimeCloneOutputs(recipe)",
        "timed processing does not validate runtime-clone outputs");
    Need(loader, "RuntimeRecipePath(def.ModName,def.Key)",
        "authored processing recipes do not receive a stable mod-scoped runtime path");
    Need(loader, "runtime-clone output has not completed ItemSubsystem registration",
        "unregistered runtime-clone processing output is not isolated");
    Need(loader, "runtime-clone output has no stable PersistenceID",
        "runtime-clone processing identity is not validated");
    Need(loader, "processing recipe still has a transient identity",
        "transient processing recipes are not rejected");
    Need(loader, "m_invalidRecipes.insert(def.Key)",
        "partially written recipes can still reach a station table");
    Need(loader, "if(m_invalidRecipes.contains(def.Key))continue;",
        "invalid recipe placement is not blocked during initial and replay placement");
    Need(loader, "sizeof(UObject*)", "processing array element size is not checked");
    Need(properties, "String references carry no separate ObjectName",
        "moved RuneSchema string paths no longer receive stable-name relocation fallback");
    Need(properties, "object->IsA(expectedClass)",
        "relocated object references are not constrained to the property type");
    Need(properties, "Ambiguous relocated object reference",
        "ambiguous relocated object names are not rejected");
    Avoid(loader, "/Engine/Transient", "recipe loader still creates transient-engine RecipeData");
    Need(loader, "/Game/RuneSchema/Generated/Recipes/", "generated recipe objects do not use a stable RuneSchema route");
    Need(loader, "RuntimeRecipePath(def.ModName,def.Key)", "authored recipe objects do not use the mod-scoped RuneSchema route");
    Need(loader, "RefreshItemRoutes()", "recipe ItemData PersistenceID index disappeared");
    Need(loader, "m_itemRoutes.find(reference)", "recipe ItemData PersistenceID routing disappeared");
    Need(loader, "propertyName!=\"ItemsConsumed\" && propertyName!=\"ItemsCreated\"",
        "recipe routing is not scoped to native ingredient/output collections");
    Need(loader, "OnFinalizeLoad(", "recipe linking is no longer deferred until all clone assets load");
    Need(guide, "22-character `PersistenceID`", "clone-backed recipe PersistenceID authoring is undocumented");
    for (const auto* token : {
        "DT_CraftingStationsDataTable", "DT_ProcessingStationDataTable",
        "CraftingTable", "BrewingCauldron", "AdvancedSmelter",
        "Category\":\"Dyes", "Array\":\"Recipes"
    }) Need(guide, token, "verified recipe target or example is undocumented");

    std::cout << "Recipe placement contract covers crafting categories, processing arrays, exact tables and late replay.\n";
}
