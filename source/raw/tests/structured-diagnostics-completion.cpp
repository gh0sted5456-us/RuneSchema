#include "Utility/StructuredDiagnosticB3C.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace PS::StructuredDiagnostics {
static std::vector<StructuredDiagnostic::Record> observed;
bool Emit(StructuredDiagnostic::Record record) noexcept {
    record.EventId="coverage-event";
    record.SessionId="coverage-session";
    record.Timestamp="2026-10-21T21:00:00Z";
    if(!StructuredDiagnostic::Validate(record)) return false;
    observed.push_back(std::move(record));
    return true;
}
}
struct Site {std::string_view code,file;};
static std::string read(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary);assert(f.good());
    return {std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{}};
}
int main(int argc,char** argv) {
    assert(argc==2);
    const auto root=std::filesystem::path{argv[1]};
    constexpr std::array<Site,72> sites={{
        {"RS-BP-004","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-005","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-006","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-007","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-008","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-009","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-010","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-BP-011","src/Loader/DragonWildsBlueprintModLoader.cpp"},
        {"RS-CORE-033","include/Loader/DragonWildsQuestLoader.h"},
        {"RS-CORE-034","include/Loader/DragonWildsQuestLoader.h"},
        {"RS-CORE-035","include/Loader/DragonWildsQuestLoader.h"},
        {"RS-CORE-036","include/Loader/DragonWildsQuestLoader.h"},
        {"RS-CORE-037","include/Loader/DragonWildsDialogueLoader.h"},
        {"RS-CORE-038","include/Loader/DragonWildsEventLoader.h"},
        {"RS-CORE-039","include/Loader/DragonWildsEventLoader.h"},
        {"RS-CORE-040","src/Loader/ModLoadOrder.cpp"},
        {"RS-CORE-041","src/Loader/ModLoadOrder.cpp"},
        {"RS-CORE-042","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-043","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-044","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-045","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-046","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-047","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-048","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-049","src/Loader/Spawn/PlayerRules.cpp"},
        {"RS-CORE-050","src/Runtime/RegistryBridge.cpp"},
        {"RS-CORE-051","src/Runtime/RegistryBridge.cpp"},
        {"RS-CORE-052","src/Runtime/RegistryBridge.cpp"},
        {"RS-CORE-053","src/Runtime/RegistryBridge.cpp"},
        {"RS-CORE-054","src/Runtime/RegistryBridge.cpp"},
        {"RS-CORE-055","src/Runtime/RegistryBridge.cpp"},
        {"RS-CRS-007","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-008","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-009","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-010","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-011","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-012","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-CRS-013","src/Loader/DragonWildsCourseLoader.cpp"},
        {"RS-IMP-001","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-002","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-003","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-004","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-005","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-006","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-007","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-008","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-009","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-010","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-011","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-IMP-012","src/Loader/DragonWildsAssetModLoader.cpp"},
        {"RS-JRN-007","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-008","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-009","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-010","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-011","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-012","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-013","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-JRN-014","src/Loader/DragonWildsJournalModLoader.cpp"},
        {"RS-RGST-008","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-009","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-010","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-011","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-012","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-013","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-014","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-RGST-015","src/Misc/DragonWildsDataRegistrar.cpp"},
        {"RS-SPAWN-008","src/Loader/DragonWildsSpawnLoader.cpp"},
        {"RS-SPAWN-009","src/Loader/DragonWildsSpawnLoader.cpp"},
        {"RS-SPAWN-010","src/Loader/DragonWildsSpawnLoader.cpp"},
        {"RS-SPAWN-011","src/Loader/DragonWildsSpawnLoader.cpp"},
        {"RS-SPAWN-012","src/Loader/DragonWildsSpawnLoader.cpp"},
        {"RS-SPAWN-013","src/Loader/DragonWildsSpawnLoader.cpp"}
    }};
    for(const auto& s:sites) {
        const auto t=read(root/std::string(s.file));
        const auto at=t.find("\""+std::string(s.code)+"\"");
        assert(at!=std::string::npos);
        const auto begin=at>128?at-128:0;
        const auto before=t.substr(begin,at-begin);
        assert(before.find("Report(")!=std::string::npos || before.find("reportImport(")!=std::string::npos);
        assert(PS::StructuredDiagnostic::IsCode(s.code));
    }
    const auto asset=read(root/"src/Loader/DragonWildsAssetModLoader.cpp");
    assert(asset.find("pendingAsset.OperationId")!=std::string::npos);
    assert(asset.find("if (settings.Dry)")!=std::string::npos);
    assert(asset.find("if (totalErrors != 0)")!=std::string::npos);
    assert(asset.find("no changes applied.")!=std::string::npos);
    PS::StructuredDiagnosticB2::Provenance provenance{"TestMod","assets/modules.jsonc","/Game/Test.DA_Test","asset-42"};
    for(const auto* code:{"RS-IMP-001","RS-IMP-007","RS-IMP-012"}) {
        assert(PS::StructuredDiagnosticB2::Report(code,"Warning","Partial","ImportPlanner",
            "A real import observation","The import step was partially applied",provenance,"Application"));
    }
    assert(PS::StructuredDiagnostics::observed.size()==3);
    const auto& entries=PS::StructuredDiagnostics::observed;
    for(const auto& r:entries) {
        assert(r.OperationId=="asset-42");
        assert(r.Context.at("ModId")=="TestMod");
        assert(r.Context.at("File")=="assets/modules.jsonc");
        assert(r.Context.at("Target")=="/Game/Test.DA_Test");
        assert(r.Source=="ImportPlanner");
        const auto wire=PS::StructuredDiagnostic::Encode(r);
        assert(wire);
        const auto parsed=PS::StructuredDiagnostic::Parse(*wire);
        assert(parsed && parsed->OperationId=="asset-42");
    }
    assert(PS::StructuredDiagnostic::IssueKey(entries[0])!=PS::StructuredDiagnostic::IssueKey(entries[1]));
    const auto journal=read(root/"src/Loader/DragonWildsJournalModLoader.cpp");
    const auto loaderBase=read(root/"include/Loader/DragonWildsModLoaderBase.h");
    assert(loaderBase.find("const std::string& GetModFolderType();")!=std::string::npos);
    assert(journal.find("RC::to_string(GetModFolderType())")==std::string::npos);
    assert(journal.find("{}, {},GetModFolderType(),\"Validation\"")!=std::string::npos);
    const auto bpHeader=read(root/"include/Loader/Blueprint/DragonWildsBlueprintMod.h");
    const auto bpLoader=read(root/"src/Loader/DragonWildsBlueprintModLoader.cpp");
    assert(bpHeader.find("GetModName() const")!=std::string::npos);
    assert(bpLoader.find("RC::to_string(mod.ModName)")==std::string::npos);
    assert(bpLoader.find("RC::to_string(mod.GetModName())")!=std::string::npos);
    const auto main=read(root/"src/dllmain.cpp");
    assert(main.find("[DID-NOT-START][CONSTRUCTION]")!=std::string::npos);
    const auto registrar=read(root/"src/Misc/DragonWildsDataRegistrar.cpp");
    assert(registrar.find("the lane is atomic and no declared object")!=std::string::npos);
    return 0;
}
