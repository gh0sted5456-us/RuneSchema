#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "Runtime/LogicPakContract.h"
#include "Runtime/BPModLoaderPatch.h"

namespace fs = std::filesystem;

std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void Touch(const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    output << "fixture";
}

int main(int argc, char** argv) {
    if (argc == 2) {
        const auto actual = PS::BPModLoaderIntegration::PatchSource(Read(argv[1]));
        assert(actual.has_value());
        assert(actual->find("logicmods-register.lua") != std::string::npos);
        assert(actual->find("rsModsRoot .. \"/mods.txt\"") != std::string::npos);
        assert(actual->find("io.open(\"Mods/mods.txt\"") == std::string::npos);
        assert(PS::BPModLoaderIntegration::PatchSource(*actual) == actual);
        return 0;
    }
    assert(argc == 5);
    const auto fixture = fs::temp_directory_path() / "RuneSchema-LogicPakBridge-Contract";
    std::error_code error;
    fs::remove_all(fixture, error);
    const auto mod = fixture / "mods" / "Dye";
    const auto package = mod / "logicmods" / "ColorsOfMoneyVisualPilotV4";
    const auto ordinary = mod / "paks" / "OrdinaryCookedContent";
    const auto legacy = fixture / "Content" / "Paks" / "LogicMods";
    fs::create_directories(package);
    fs::create_directories(legacy);
    Touch(package / "ColorsOfMoneyVisualPilotV4_0.7.7.5m_P.pak");
    Touch(package / "ColorsOfMoneyVisualPilotV4_0.7.7.5m_P.utoc");
    Touch(package / "ColorsOfMoneyVisualPilotV4_0.7.7.5m_P.ucas");
    fs::create_directories(ordinary);
    Touch(ordinary / "OrdinaryCookedContent.pak");
    Touch(ordinary / "OrdinaryCookedContent.utoc");
    Touch(ordinary / "OrdinaryCookedContent.ucas");

    auto discovered = PS::LogicPaks::Discover(mod, "Dye", legacy);
    assert(discovered.size() == 1);
    assert(discovered[0].Name == "ColorsOfMoneyVisualPilotV4");
    assert(discovered[0].ActorPath ==
        "/Game/Mods/ColorsOfMoneyVisualPilotV4/ModActor.ModActor_C");
    assert(!discovered[0].LegacyOwned);
    assert(discovered[0].Name != "OrdinaryCookedContent");

    const std::string upstreamLua =
        "local function LoadModConfigs()\n"
        "    LoadModOrder()\n"
        "    SetupModOrder()\n"
        "end\n"
        "LoadModConfigs()\n";
    auto patched = PS::BPModLoaderIntegration::PatchSource(upstreamLua);
    assert(patched.has_value());
    assert(patched->find("logicmods-register.lua") != std::string::npos);
    assert(patched->find("rsModsRoot .. \"/mods.txt\"") != std::string::npos);
    assert(PS::BPModLoaderIntegration::PatchSource(*patched) == patched);
    assert(!PS::BPModLoaderIntegration::PatchSource("unrelated Lua").has_value());

    const auto ue4ssMods = fixture / "ue4ss" / "Mods";
    fs::create_directories(ue4ssMods);
    const auto modsTxt = ue4ssMods / "mods.txt";
    {
        std::ofstream output(modsTxt);
        output << "BPModLoaderMod : 1\nRuneSchema : 1\nRuneSchema : 0\n";
    }
    assert(!PS::BPModLoaderIntegration::EnabledInModsTxt(modsTxt, "RuneSchema"));
    {
        std::ofstream output(modsTxt);
        output << "BPModLoaderMod : 1\nRuneSchema : 1\n";
    }
    assert(PS::BPModLoaderIntegration::EnabledInModsTxt(modsTxt, "RuneSchema"));

    Touch(legacy / "ColorsOfMoneyVisualPilotV4.pak");
    discovered = PS::LogicPaks::Discover(mod, "Dye", legacy);
    assert(discovered.size() == 1 && discovered[0].LegacyOwned);

    PS::LogicPaks::WorldActivationGate gate;
    constexpr std::uintptr_t frontend = 0x1000;
    constexpr std::uintptr_t gameplay = 0x2000;
    assert(gate.Claim(frontend, "ColorsOfMoneyVisualPilotV4"));
    assert(!gate.Claim(frontend, "ColorsOfMoneyVisualPilotV4"));
    assert(gate.Claim(gameplay, "ColorsOfMoneyVisualPilotV4"));
    assert(!gate.Claim(gameplay, "ColorsOfMoneyVisualPilotV4"));

    const auto runtime = Read(argv[1]);
    assert(runtime.find("RegisterBeginPlayPostCallback") != std::string::npos);
    assert(runtime.find("Storefront::IsDedicatedServer()") != std::string::npos);
    assert(runtime.find("PreBeginPlay") != std::string::npos);
    assert(runtime.find("PostBeginPlay") != std::string::npos);
    assert(runtime.find("[LOGIC-PAK][STARTED]") != std::string::npos);
    assert(runtime.find("duplicate startup suppressed") != std::string::npos);

    const auto integration = Read(argv[3]);
    assert(integration.find("if (!patchScript)") != std::string::npos);
    assert(integration.find("AtomicWrite(list, emptyManifest)") != std::string::npos);
    assert(integration.find("HasMultipleHardlinks(bpScript)") != std::string::npos);
    assert(integration.find("PrepareLua(") != std::string::npos);
    assert(integration.find("logicmods.lua.generated.txt") != std::string::npos);
    const auto luaLoader = Read(argv[4]);
    assert(luaLoader.find("RegisterLoadMapPostHook") != std::string::npos);
    assert(luaLoader.find("RegisterBeginPlayPostHook") != std::string::npos);
    assert(luaLoader.find("started[name] = true") != std::string::npos);

    const auto documentation = Read(argv[2]);
    assert(documentation.find("ModActor.ModActor_C") != std::string::npos);
    assert(documentation.find("Content/Paks/LogicMods") != std::string::npos);
    fs::remove_all(fixture, error);
}
