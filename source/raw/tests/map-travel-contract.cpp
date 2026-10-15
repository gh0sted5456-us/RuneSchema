#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing map travel source");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==5,"Expected travel, spawn tools, lifecycle and map UI");const auto travel=Read(argv[1]),tools=Read(argv[2]),life=Read(argv[3]),ui=Read(argv[4]);
    Check(travel.contains("WorldPartitionStreamingSourceComponent:IsStreamingCompleted")&&travel.contains("\"Activated\""),"Destination must be activated, not only metadata-loaded");
    Check(travel.find("MapLanding::ResolveNear")<travel.find("teleport.Invoke()"),"Ground/capsule footprint must be verified before native departure");
    Check(travel.contains("TeleportationSubsystem:TeleportWithParams")&&travel.contains("\"bNoCheck\",false")&&travel.contains("\"bIsATest\",false"),"Use normal native travel checks, never test/no-check teleport");
    Check(!travel.contains("K2_TeleportTo")&&!travel.contains("sleep_for")&&!travel.contains("BlockTillLevelStreaming"),"No blind position jump or blocking streaming flush");
    Check(travel.contains("ToolMapTravelTeleporting(pawn)")&&travel.contains("MapTravel::Arrived"),"Acceptance alone must not report arrival");
    Check(travel.find("!pawn->IsA(playerClass)")<travel.find("if(ToolMapTravelTeleporting(pawn))"),"Reject spectator/other pawns before invoking native character functions");
    Check(travel.contains("/Script/CoreUObject.Vector")&&travel.contains("/Script/CoreUObject.Rotator")&&travel.contains("Native teleport safety flags changed type"),"Coordinate and safety-flag semantics must be checked, not only their byte sizes");
    Check(travel.contains("Generation!=PS::SpawnToolRequests::Generation")&&travel.contains("GetObjectRef(controller,TEXT(\"Pawn\"))==pawnObject"),"World/possession changes must cancel stale jobs");
    Check(travel.contains("RF_Transient")&&travel.contains("bInReplicates\"),false")&&life.contains("ToolMapTravelReset(false)"),"Anchor must not persist/replicate or call the engine during shutdown");
    Check(tools.find("ToolMapTravelPump(m_readyWorld)")<tools.find("SpawnToolRequests::Take()"),"Async travel must retain the mutation slot");
    Check(tools.find("Only the authoritative server may run spawn tools")<tools.find("ToolMapTravelBegin(world"),"Map travel must retain host authority");
    Check(ui.contains("destinationReady=Data.Operation==2||groundReady"),"Only teleport may prepare unloaded ground; spawn/build retain ground gates");
}
