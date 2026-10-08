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
    fs::remove_all(root);
}
