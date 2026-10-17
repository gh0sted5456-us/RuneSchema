#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("Ground contract input missing");return {std::istreambuf_iterator<char>(f),{}};}
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    Require(argc==2,"Expected native source root");const std::filesystem::path root(argv[1]);
    const auto npc=Read(root/"src/Loader/DragonWildsNpcLoader.cpp");
    const auto spawn=Read(root/"src/Loader/DragonWildsSpawnLoader.cpp");
    const auto event=Read(root/"include/Loader/EventRuntime.h");
    const auto tools=Read(root/"src/Loader/SpawnTools.inl");
    const auto sdk=Read(root/"src/SDK/Classes/KismetSystemLibrary.cpp");
    const auto nearby=Read(root/"include/SDK/Helper/GroundedPlacement.h");
    Require(sdk.find("CapsuleTraceSingleByProfile")!=std::string::npos
        &&sdk.find("Capsule trace profile-name contract changed")!=std::string::npos,
        "Spawn clearance is still only a visibility ray instead of a native blocking-profile volume");
    Require(nearby.find("Nearby(policy,")!=std::string::npos&&nearby.find("SameNearbyFloor(")!=std::string::npos
        &&nearby.find("SpawnFootprint::Overlaps(")!=std::string::npos
        &&event.find("std::vector<PS::SpawnFootprint::Reservation> reservations")!=std::string::npos,
        "Nearby migration loses its floor, body, or same-wave reservation checks");
    const auto journal=Read(root/"src/Loader/JournalPersistence.inl");
    const auto inspection=Read(root/"src/Generator/InspectionTools.cpp");
    const auto cell=Read(root/"src/SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.cpp");
    Require(spawn.find("ReconcileShownCells(deltaSeconds);")!=std::string::npos
        &&spawn.find("UWorldPartitionRuntimeLevelStreamingCell::VisitShown(m_readyWorld")!=std::string::npos
        &&cell.find("scanned++<2048")!=std::string::npos&&cell.find("microseconds(500)")!=std::string::npos,
        "Already-shown spawn cells have no bounded catch-up after loader readiness");
    Require(npc.find("ReconcileShownVendorCells(deltaSeconds);")!=std::string::npos
        &&npc.find("UWorldPartitionRuntimeLevelStreamingCell::VisitShown(world")!=std::string::npos
        &&cell.find("if(!cell->GetIsHLOD()&&cell->IsShown())visit(cell);")!=std::string::npos,
        "NPC definitions cannot recover the same confirmed visible-cell boundary as resource placements");
    Require(cell.find("TEXT(\"LevelStreaming\")")!=std::string::npos
        &&cell.find("TEXT(\"LoadedLevel\")")!=std::string::npos&&cell.find("TEXT(\"bIsVisible\")")!=std::string::npos
        &&cell.find("TEXT(\"bShouldBeVisible\")")==std::string::npos,
        "Cell catch-up guesses visibility from requested state instead of reflected live state");
    Require(spawn.find("authoritative && hasPending && GetAIDirector(world)")==std::string::npos
        &&spawn.find("spawn.Type==ESpawnEntryType::AISpawnPoint&&!GetAIDirector(world)")!=std::string::npos,
        "Resource actors still depend on unrelated AI-director readiness");
    const auto begin=npc.find("void DragonWildsNpcLoader::TrySpawnConfiguredVendors(");
    const auto ground=npc.find("ResolveNpcGrounding(world,definition);",begin);
    const auto gate=npc.find("if (!definition.SpawnGate.Begin()) continue;",begin);
    const auto rows=npc.find("CreateInlineMerchantRow(definition);",gate);
    Require(begin!=std::string::npos&&ground<gate&&gate<rows,"NPC ground readiness consumed its construction gate or mutated a shop row");
    const auto pending=npc.find("catch (const PS::Grounding::SurfacePending&)",begin);
    const auto hard=npc.find("catch (const std::exception& error)",pending);
    Require(pending!=std::string::npos&&hard!=std::string::npos&&npc.substr(pending,hard-pending).find("SpawnGate.Begin")==std::string::npos,"Ground misses permanently close an NPC spawn gate");
    Require(npc.substr(pending,hard-pending).find("WarnOnce")==std::string::npos&&npc.substr(pending,hard-pending).find("LogLevel::Verbose")!=std::string::npos,"Ordinary NPC collision readiness still warns in normal logs");
    Require(npc.find("GroundRetry.Ready()") == std::string::npos&&spawn.find("GroundRetry.Ready()") == std::string::npos,"Persistent placements still stop at the encounter retry deadline");
    Require(npc.find("GroundRetry.PlacementReady()")!=std::string::npos&&spawn.find("GroundRetry.PlacementReady()")!=std::string::npos,"Persistent placements cannot recover after late collision readiness");
    Require(npc.find("m_events.ReportFailure=[this]")!=std::string::npos&&event.find("Failure(definition.Key+\": failed: \"")!=std::string::npos,"Event failures are hidden with ordinary event diagnostics");
    const std::string groundCatch="PS::Grounding::SurfacePending&";
    for(size_t at=spawn.find(groundCatch);at!=std::string::npos;at=spawn.find(groundCatch,at+groundCatch.size())){
        const auto next=spawn.find("catch",at+groundCatch.size());const auto handler=spawn.substr(at,next-at);
        Require(handler.find("LogLevel::Warning")==std::string::npos&&handler.find("LogLevel::Verbose")!=std::string::npos,"Ordinary spawn collision readiness still warns");
    }
    Require(npc.find("groundRetryReady || m_scanBudget.Begin(world != nullptr)")!=std::string::npos,"NPC ground wait consumes its asset-readiness budget");
    Require(npc.find("m_scanBudget.Advance(m_scanElapsed);")!=std::string::npos
        &&npc.find("&&(!m_scanBudget.Exhausted()||definition.GroundRetry.Pending)")==std::string::npos,
        "Late native class readiness is permanently suppressed by an exhausted initial scan burst");
    Require(npc.find("definition.GroundSearch.ObserveCell(bounds->Min.Z(),bounds->Max.Z())")!=std::string::npos
        &&npc.find("definition.GroundRetry.CellShown()")!=std::string::npos,"Streamed NPC collision cannot re-arm a bounded miss");
    Require(spawn.find("spawn.GroundSearch.ObserveCell(box.Min.Z(),box.Max.Z())")!=std::string::npos,"Spawns ignore streamed-cell vertical bounds");
    for(const auto* source:{&npc,&spawn,&event,&tools})Require(source->find("UKismetSystemLibrary::FindGroundSurface(")!=std::string::npos,"A placement loader bypasses the shared ground query");
    Require(tools.find("UKismetSystemLibrary::LineTraceGround(")==std::string::npos,"Authoring still uses a raw visibility ray for placement");
    Require(inspection.find("UKismetSystemLibrary::LineTraceGround(")!=std::string::npos,"Crosshair inspection was changed into vertical placement");
    const auto wave=event.find("void SpawnWave(");const auto positions=event.find("positions.push_back(location)",wave);
    const auto construct=event.find("auto* actor=Spawn(",wave);
    Require(positions!=std::string::npos&&positions<construct,"Event wave can construct actors before every ground position is ready");
    Require(event.find("if(wavePending){")<event.find("if(run.WaveComplete())"),"Pending event waves cannot retry before completion logic");
    Require(event.find("groundRetry.PlacementReady()")!=std::string::npos&&event.find("run.Tick(delta)")!=std::string::npos&&event.find("wavePending=false;initialWavePending=false;groundRetry={};")!=std::string::npos,"Event ground waits must recover late collision while remaining bounded by encounter timeout and travel reset");
    Require(sdk.find("PS::Grounding::QuerySurface")!=std::string::npos&&sdk.find("if(contractFailure)return false;")!=std::string::npos,"Complex fallback retries a failed native contract");
    Require(sdk.find("floor.Contains(impact.Z())")!=std::string::npos,"Shared ground helper accepts invalid hit coordinates");
    Require(sdk.find("FVector(end.X(),end.Y(),floor.Bottom)")!=std::string::npos,"Intended floor bound is not used by the actual collision ray");
    Require(sdk.find("LandscapeHeightfieldCollisionComponent")!=std::string::npos&&sdk.find("attempt<16")!=std::string::npos,"Bare $ does not use a bounded terrain baseline");
    Require(sdk.find("!penetrating")!=std::string::npos&&sdk.find("normal.Z()>=0.5")!=std::string::npos,"Ground query accepts walls or initial overlap as a floor");
    Require(npc.find("impact,error,definition.GroundReferenceZ")!=std::string::npos&&spawn.find("impact, traceError,spawn.GroundReferenceZ")!=std::string::npos,"Configured loaders lost their floor reference");
    Require(event.find("member.RelativeGround?std::nullopt:std::optional<double>{member.Position[2]}")!=std::string::npos&&event.find("impact,error,reference")!=std::string::npos&&tools.find("ground,error,origin.Z()")!=std::string::npos,"Bare-dollar events must discover destination terrain; explicit floors and nearby player-relative tools must retain their floor references");
    Require(journal.find("JournalSave::RemoveCurrent")==std::string::npos
        &&journal.find("JournalSave::Record(payload,RegisteredOwners())")!=std::string::npos,"Journal save hook strips active learned progress");
}
