#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include "Bootstrap.h"

namespace {
const int ModuleAnchor = 0;

class FixtureMod : public RC::CppUserModBase
{
    void on_unreal_init() override
    {
        RC::Output::send(STR("[PakLoaderFixture] UE4SS delivered on_unreal_init.\n"));
    }

    void on_cpp_mods_loaded() override
    {
        const auto result = PS::PakLoader::StartFromMod(&ModuleAnchor);
        const auto repeated = PS::PakLoader::StartFromMod(&ModuleAnchor);
        RC::Output::send(STR("[PakLoaderFixture] existing native DLL started; status {} / {}\n"),
            static_cast<int>(result), static_cast<int>(repeated));
    }
};
}

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod() { return new FixtureMod(); }
extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
