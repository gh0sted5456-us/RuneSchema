#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "Runtime/LogicPakContract.h"

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
    assert(argc == 3);
    const auto fixture = fs::temp_directory_path() / "RuneSchema-LogicPakBridge-Contract";
    std::error_code error;
    fs::remove_all(fixture, error);
    const auto mod = fixture / "mods" / "Dye";
    const auto package = mod / "paks" / "ColorsOfMoneyVisualPilotV4";
    const auto legacy = fixture / "Content" / "Paks" / "LogicMods";
    fs::create_directories(package);
    fs::create_directories(legacy);
    Touch(package / "ColorsOfMoneyVisualPilotV4.pak");
    Touch(package / "ColorsOfMoneyVisualPilotV4.utoc");
    Touch(package / "ColorsOfMoneyVisualPilotV4.ucas");

    auto discovered = PS::LogicPaks::Discover(mod, "Dye", legacy);
    assert(discovered.size() == 1);
    assert(discovered[0].Name == "ColorsOfMoneyVisualPilotV4");
    assert(discovered[0].ActorPath ==
        "/Game/Mods/ColorsOfMoneyVisualPilotV4/ModActor.ModActor_C");
    assert(!discovered[0].LegacyOwned);

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

    const auto documentation = Read(argv[2]);
    assert(documentation.find("ModActor.ModActor_C") != std::string::npos);
    assert(documentation.find("Content/Paks/LogicMods") != std::string::npos);
    fs::remove_all(fixture, error);
}
