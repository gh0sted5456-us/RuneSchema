#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing map travel source");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){try{
    Check(argc==5,"Expected travel, spawn tools, lifecycle and map UI");const auto travel=Read(argv[1]),tools=Read(argv[2]),life=Read(argv[3]),ui=Read(argv[4]);
    Check(travel.contains("WorldPartitionStreamingSourceComponent:IsStreamingCompleted")&&travel.contains("\"Activated\""),"Destination must be activated, not only metadata-loaded");
    Check(travel.find("job.GroundProbe.Resolve")<travel.find("teleport.Invoke()"),"Ground/capsule footprint must be verified before native departure");
    Check(travel.contains("TeleportationSubsystem:TeleportWithParams")&&travel.contains("\"bNoCheck\",false")&&travel.contains("\"bIsATest\",false"),"Use normal native travel checks, never test/no-check teleport");
    Check(!travel.contains("K2_TeleportTo")&&!travel.contains("sleep_for")&&!travel.contains("BlockTillLevelStreaming"),"No blind position jump or blocking streaming flush");
    Check(travel.contains("ToolMapTravelTeleporting(pawn)")&&travel.contains("MapTravel::Arrived"),"Acceptance alone must not report arrival");
    Check(travel.find("!pawn->IsA(playerClass)")<travel.find("if(ToolMapTravelTeleporting(pawn))"),"Reject spectator/other pawns before invoking native character functions");
    Check(travel.contains("/Script/CoreUObject.Vector")&&travel.contains("/Script/CoreUObject.Rotator")&&travel.contains("Native teleport safety flags changed type"),"Coordinate and safety-flag semantics must be checked, not only their byte sizes");
    Check(travel.contains("Generation!=PS::SpawnToolRequests::Generation")&&travel.contains("GetObjectRef(controller,TEXT(\"Pawn\"))==pawnObject"),"World/possession changes must cancel stale jobs");
    Check(travel.contains("RF_Transient")&&travel.contains("bInReplicates\"),false")&&life.contains("ToolMapTravelReset(false)"),"Anchor must not persist/replicate or call the engine during shutdown");
    Check(tools.find("ToolMapTravelPump(m_readyWorld)")<tools.find("SpawnToolRequests::Take()"),"Async travel must retain the mutation slot");
    Check(tools.contains("sampled<24")&&tools.contains("microseconds(1000)")&&tools.contains("PS::ResourceObservation::Near")&&tools.contains("ResourceRadiusMetres"),"Resource observations must stay budgeted and near connected players");
    Check(tools.find("Only the authoritative server may run spawn tools")<tools.find("ToolMapTravelBegin(world"),"Map travel must retain host authority");
    Check(tools.find("Only the authoritative server may run spawn tools")<tools.find("if(request->contains(\"TargetPlayer\"))"),"Party travel must not bypass the authoritative world gate");
    Check(tools.contains("Party travel world or moving character changed")&&tools.contains("Party destination disconnected or changed character")&&tools.contains("request->value(\"PartyPoint\",false)"),"Party travel must revalidate world and identities for player and map destinations");
    Check(tools.contains("ActorHelper::GetActorLocation(static_cast<AActor*>(targetPawn))")&&tools.contains("map[\"PartyTravelVersion\"]=2"),"Party travel must resolve the current native destination and advertise capability");
    Check(ui.contains("destinationReady=Data.Operation==2||groundReady"),"Only teleport may prepare unloaded ground; spawn/build retain ground gates");
    Check(ui.contains("Data.ProbeWanted=Data.Operation!=2")&&ui.contains("previewReady=Data.Operation==2||"),"Repeated teleport selection still waits for a placement-only preview");
    Check(ui.contains("const bool context=ImGui::IsMouseReleased(ImGuiMouseButton_Right)")
        &&ui.contains("if(context||(ImGui::IsMouseReleased(ImGuiMouseButton_Left)&&!Data.Dragged))")
        &&ui.contains("if(context||Data.BuildingPick)")
        &&ui.contains("Data.MapPopupAnchor=io.MousePos;Data.Popup=true;Data.BuildingPick=false;ImGui::OpenPopup")
        &&ui.contains("Teleport member to...")&&ui.contains("Confirm party travel"),
        "Map must separate ordinary left selection from right context, permit explicit building picks and confirm party mutations");
    Check(ui.contains("ProgressBar(-")&&ui.contains("duration_cast<std::chrono::seconds>")&&ui.contains("Cancel travel"),"Travel must show an indeterminate phase indicator, elapsed time and pre-departure cancel");
    Check(travel.contains("ToolMapTravelProgress(\"Preparing\"")&&travel.contains("ToolMapTravelProgress(\"Checking ground\"")&&travel.contains("ToolMapTravelProgress(\"Travelling\""),"Native travel stages must report progress without completing the request");
    Check(travel.contains("ActorComponent:GetOwner")&&travel.contains("owner.Result<UObject*>()!=anchor")&&!travel.contains("SceneComponent:GetAttachParent"),"Streaming source must validate its owning destination actor, not assume scene-component inheritance");
    Check(travel.contains("TravelCandidates")&&travel.contains("job.Candidate++")&&travel.contains("CapsuleBlocked"),"Travel must progressively search nearby footprints and verify capsule clearance");
    Check(travel.contains("milliseconds(500)"),"Travel progress must be throttled");
    Check(tools.contains("SpawnToolRequests::TakeMapGround()")&&tools.contains("if(!PS::SpawnToolRequests::IsWaiting()&&groundNow>=nextMapGround)"),"Height queries must stay below queued travel/mutation priority");
    Check(ui.contains("SpawnToolRequests::SubmitMapGround")&&ui.contains("milliseconds(120)")&&tools.contains("milliseconds(150)")&&ui.contains("ground unavailable"),"Hover Z must be debounced, independently requested and honest about unloaded ground");
    Check(ui.contains("request[\"TargetLocation\"][\"Z\"]")&&travel.contains("groundHint.value_or(origin.Z())")&&travel.contains("job.GroundProbe.Resolve"),"Measured Z must guide streaming but never bypass runtime ground verification");
    Check(travel.contains("job.TerrainIgnored.push_back")&&travel.contains("LandingProbe::Yield")&&travel.contains("milliseconds(2)")&&travel.contains("job.GroundProbe={}"),"Terrain discovery and footprint checks must yield and discard samples when readiness changes");
    Check(travel.contains("if(groundContractFailed)throw;")&&travel.contains("[HELPY][MAP-TRAVEL]")&&travel.contains("\"bShowLoadingScreen\",false"),"Broken native contracts must stop with evidence and prepared travel must not request another loading overlay");
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
