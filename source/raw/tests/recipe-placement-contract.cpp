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
    const auto raw = value.str();
    std::string text;
    text.reserve(raw.size());
    for (const auto character : raw) if (character != '\r') text += character;
    return text;
}

static void Need(const std::string& text, const std::string& token, const char* message)
{
    if (text.find(token) == std::string::npos) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void Avoid(const std::string& text, const std::string& token, const char* message)
{
    if (text.find(token) != std::string::npos) {
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
    Need(loader, "InspectRuntimeCloneOutputs(recipe,m_itemRoutes,m_ambiguousItemRoutes)",
        "timed processing does not validate runtime-clone outputs");
    Need(loader, "RuntimeRecipePath(def.ModName,def.Key)",
        "authored processing recipes do not receive a stable mod-scoped runtime path");
    Need(loader, "ResolveReference(",
        "assets can no longer resolve local recipe-unlocker targets");
    Need(loader, "runtime-clone output has not completed ItemSubsystem registration",
        "unregistered runtime-clone processing output is not isolated");
    Need(loader, "runtime-clone output has no stable PersistenceID",
        "runtime-clone processing identity is not validated");
    Need(loader, "does not round-trip through the live PersistenceID registry",
        "processing output is not verified against the live ItemSubsystem identity route");
    Need(loader, "processing recipe still has a transient identity",
        "transient processing recipes are not rejected");
    Need(loader, "m_invalidRecipes.insert(identity)",
        "partially written recipes can still reach a station table");
    Need(loader, "if(m_invalidRecipes.contains(identity))continue;",
        "invalid recipe placement is not blocked during initial and replay placement");
    Need(loader, "sizeof(UObject*)", "processing array element size is not checked");
    Need(properties, "String references carry no separate ObjectName",
        "moved RuneSchema string paths no longer receive stable-name relocation fallback");
    Need(properties, "object->IsA(expectedClass)",
        "relocated object references are not constrained to the property type");
    Need(properties, "Ambiguous relocated object reference",
        "ambiguous relocated object names are not rejected");
    Avoid(loader, "StaticFindObject<UObject*>(nullptr,nullptr,TEXT(\"/Engine/Transient\")",
        "recipe loader still constructs RecipeData in the transient engine package");
    Need(loader, "/Game/RuneSchema/Generated/Recipes/", "generated recipe objects do not use a stable RuneSchema route");
    Need(loader, "RuntimeRecipePath(def.ModName,def.Key)", "authored recipe objects do not use the mod-scoped RuneSchema route");
    Need(loader, "RefreshItemRoutes()", "recipe ItemData PersistenceID index disappeared");
    Need(loader, "m_itemRoutes.find(reference)", "recipe ItemData PersistenceID routing disappeared");
    Need(loader, "Recipe ItemData must be a full object path",
        "recipe ItemData does not enforce the path-first authoring contract");
    Need(loader, "Recipe ItemData path did not resolve to ItemData",
        "recipe ItemData paths are not resolved and type checked");
    Need(loader, "VerifyRecipeItemAmounts(recipe,propertyName,routed)",
        "recipe ingredient and output quantities are not verified after reflection writes");
    Need(loader, "Count did not survive the reflected write",
        "recipe count truncation is not rejected before station placement");
    Need(loader, "retained fewer entries than were authored",
        "recipe output loss is not rejected before station placement");
    Need(loader, "propertyName!=\"ItemsConsumed\" && propertyName!=\"ItemsCreated\"",
        "recipe routing is not scoped to native ingredient/output collections");
    Need(loader, "OnFinalizeLoad(", "recipe linking is no longer deferred until all clone assets load");
    std::cout << "Recipe placement contract covers crafting categories, processing arrays, exact tables and late replay.\n";
}
