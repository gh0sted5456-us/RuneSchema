#include "Utility/StructuredDiagnosticB3D.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/StructuredDiagnosticB3B.h"
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> emitted;
bool Emit(PS::StructuredDiagnostic::Record record) noexcept {
    record.EventId="b3d5-event";
    record.SessionId="b3d5-session";
    record.Timestamp="2026-10-21T20:00:00Z";
    if (!PS::StructuredDiagnostic::Validate(record)) return false;
    emitted.push_back(std::move(record));
    return true;
}
}

struct Site {
    std::string_view code, file, source, outcome, phase;
};

static std::string contents(const std::filesystem::path& file) {
    std::ifstream stream(file,std::ios::binary);
    assert(stream.good());
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}

int main(int argc,char** argv) {
    assert(argc==2);
    const auto root=std::filesystem::path{argv[1]};
    constexpr std::array sites={
        Site{"RS-RGST-004","Loader/DragonWildsRegistryLoader.cpp","RegistryLoader","Skipped","Validation"},
        Site{"RS-RGST-005","Loader/DragonWildsRegistryLoader.cpp","RegistryLoader","Partial","Discovery"},
        Site{"RS-RGST-006","Loader/DragonWildsRegistryLoader.cpp","RegistryLoader","Partial","Discovery"},
        Site{"RS-RGST-007","Loader/DragonWildsRegistryLoader.cpp","RegistryLoader","Deferred","Reload"},
        Site{"RS-BLD-016","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Resolution"},
        Site{"RS-BLD-017","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Resolution"},
        Site{"RS-BLD-018","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Finalization"},
        Site{"RS-BLD-019","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Validation"},
        Site{"RS-BLD-020","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Application"},
        Site{"RS-BLD-021","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Partial","Finalization"},
        Site{"RS-BLD-022","Loader/DragonWildsBuildingModLoader.cpp","BuildingLoader","Blocked","Finalization"},
        Site{"RS-CORE-030","dllmain.cpp","LoaderCore","Partial","Application"},
        Site{"RS-CORE-031","dllmain.cpp","LoaderCore","Skipped","Application"},
        Site{"RS-CORE-032","dllmain.cpp","LoaderCore","Skipped","Validation"},
    };
    for(const auto& site:sites) {
        const auto text=contents(root/std::string(site.file));
        const auto needle="\""+std::string(site.code)+"\"";
        const auto at=text.find(needle);
        assert(at!=std::string::npos && text.find(needle,at+needle.size())==std::string::npos);
        const auto producer=site.source=="LoaderCore" ? "StructuredDiagnosticB3D::Report("
            : site.source=="BuildingLoader" ? "StructuredDiagnosticB3B::Report("
            : "StructuredDiagnosticB3C::Report(";
        const auto prefix=text.rfind(producer,at);
        assert(prefix!=std::string::npos && at-prefix<110);

        bool accepted=false;
        if(site.source=="LoaderCore")
            accepted=PS::StructuredDiagnosticB3D::Report(std::string(site.code),"Warning",std::string(site.outcome),
                "Observed integration issue","A known integration branch failed.","PluginService",std::string(site.phase),"TestMod");
        else if(site.source=="BuildingLoader")
            accepted=PS::StructuredDiagnosticB3B::Report(std::string(site.code),"Error",std::string(site.outcome),
                "BuildingLoader","Observed building issue","A world registry operation did not complete.","TestMod",{},"BuildingWorldRegistry",std::string(site.phase));
        else
            accepted=PS::StructuredDiagnosticB3C::Report(std::string(site.code),"Error",std::string(site.outcome),
                "RegistryLoader","Observed registry issue","A cooked registry operation did not complete.","TestMod","pak:/Game/Mods/TestMod/Registry","CookedRegistry",std::string(site.phase));
        assert(accepted);
        const auto& record=PS::StructuredDiagnostics::emitted.back();
        assert(record.Source==site.source && record.Code==site.code && record.Outcome==site.outcome);
        assert(record.Context.value("Phase",std::string{})==site.phase);
        assert(record.Context.value("ModId",std::string{})=="TestMod");
        const auto encoded=PS::StructuredDiagnostic::Encode(record);
        assert(encoded && PS::StructuredDiagnostic::Parse(*encoded));
    }
    assert(PS::StructuredDiagnostics::emitted.size()==sites.size());
    const auto firstKey=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::emitted[0]);
    assert(PS::StructuredDiagnosticB3C::Report("RS-RGST-004","Error","Skipped","RegistryLoader",
        "Cooked registry rejected","Owner is different","OtherMod","pak:/Game/Mods/OtherMod/Registry","CookedRegistry","Validation"));
    assert(firstKey!=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::emitted.back()));

    const auto entry=contents(root/"dllmain.cpp");
    assert(entry.find("config->Load();")<entry.find("\"RS-CORE-030\""));
    assert(entry.find("[RuneSchema][DID-NOT-START][CONSTRUCTION]")!=std::string::npos);
    const auto reg=contents(root/"Loader/DragonWildsRegistryLoader.cpp");
    assert(reg.find("Cooked registry asset '{}' rejected:")!=std::string::npos);
    assert(reg.find("JSON registries remain active:")!=std::string::npos);
    const auto building=contents(root/"Loader/DragonWildsBuildingModLoader.cpp");
    assert(building.find("Custom Building registry rollback failed; transient state was retained")!=std::string::npos);
    assert(building.find("Building registration was aborted")!=std::string::npos);
    assert(building.find("other RuneSchema systems continue")!=std::string::npos);
}
