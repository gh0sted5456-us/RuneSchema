#include "Utility/ModFolderLayout.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

int main()
{
    namespace fs=std::filesystem;
    using namespace PS::ModFolderLayout;
    assert(EqualsInsensitive("recipes","Recipes"));
    assert(EqualsInsensitive("ASSETS","assets"));
    assert(!EqualsInsensitive("recipes","recipe"));

    const auto token=std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root=fs::temp_directory_path()/("runeschema-loader-case-"+token);
    fs::create_directories(root/"Recipes");
    const auto resolved=ResolveLoaderDirectory(root,"recipes");
    assert(resolved&&resolved->filename()=="Recipes");
    assert(!ResolveLoaderDirectory(root,"assets"));
    const auto junk=root/"NexusManagerDebris";
    fs::create_directories(junk/"__vortex_staging");
    {std::ofstream(junk/"meta.ini")<<"manager metadata";}
    {std::ofstream(junk/"README.txt")<<"not RuneSchema content";}
    assert(!LooksLikeRuneSchemaMod(junk));

    const auto valid=root/"ActualRuneSchemaMod";
    fs::create_directories(valid/"Assets");
    {std::ofstream(valid/"notes.txt")<<"ignored";}
    assert(LooksLikeRuneSchemaMod(valid));

    const auto luaOnly=root/"LuaOnlyRuneSchemaMod";
    fs::create_directories(luaOnly/"ue4ss"/"scripts");
    {std::ofstream(luaOnly/"ue4ss"/"scripts"/"main.lua")<<"print('loaded')";}
    assert(LooksLikeRuneSchemaMod(luaOnly));

    const auto legacy=root/"LegacyPakMod";
    fs::create_directories(legacy/"old");
    {std::ofstream(legacy/"old"/"Legacy.pak")<<"";}
    assert(LooksLikeRuneSchemaMod(legacy));

    const auto cooked=root/"CookedLogicMod";
    fs::create_directories(cooked/"PaKs"/"ZetaLogic");
    fs::create_directories(cooked/"PaKs"/"AlphaLogic");
    {std::ofstream(cooked/"PaKs"/"ZetaLogic"/"ZetaLogic.pak")<<"";}
    {std::ofstream(cooked/"PaKs"/"ZetaLogic"/"ZetaLogic.utoc")<<"";}
    {std::ofstream(cooked/"PaKs"/"ZetaLogic"/"ZetaLogic.ucas")<<"";}
    {std::ofstream(cooked/"PaKs"/"AlphaLogic"/"AlphaLogic.pak")<<"";}
    const auto cookedReads=PakReadDirectories(cooked);
    assert(cookedReads.size()==2);
    assert(cookedReads[0].filename()=="AlphaLogic");
    assert(cookedReads[1].filename()=="ZetaLogic");

    const auto optIn=root/"OptInLogicMod";
    fs::create_directories(optIn/"LogicMods"/"FirstLogic");
    {std::ofstream(optIn/"LogicMods"/"FirstLogic"/"FirstLogic.pak")<<"";}
    assert(LooksLikeRuneSchemaMod(optIn));
    const auto logicReads=PakReadDirectories(optIn);
    assert(logicReads.size()==1&&logicReads[0].filename()=="FirstLogic");

    const auto flat=root/"FlatCookedMod";
    fs::create_directories(flat/"paks");
    {std::ofstream(flat/"paks"/"Flat.pak")<<"";}
    const auto flatReads=PakReadDirectories(flat);
    assert(flatReads.size()==1&&flatReads[0].filename()=="paks");

    const auto rootCooked=root/"RootCookedMod";
    fs::create_directories(rootCooked);
    {std::ofstream(rootCooked/"Root.pak")<<"";}
    const auto rootReads=PakReadDirectories(rootCooked);
    assert(rootReads.size()==1&&rootReads[0]==rootCooked);
    fs::remove_all(root);
}
