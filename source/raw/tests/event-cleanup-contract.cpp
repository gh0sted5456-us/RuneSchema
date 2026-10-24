#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Event runtime source required");
    std::ifstream file(argv[1]);
    if(!file)throw std::runtime_error("Event runtime source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto healthStart=source.find("static std::optional<double> HealthPercent(");
    const auto healthEnd=source.find("void PresentHealthStages()",healthStart);
    assert(healthStart!=source.npos && healthEnd!=source.npos);
    const auto health=source.substr(healthStart,healthEnd-healthStart);
    assert(health.find("PS::PlayerVitals::Number(health,TEXT(\"/Script/Dominion.HealthComponent:GetAuthoritativeHealth\"))")!=health.npos);
    assert(health.find("GetCurrentHealth")==health.npos && health.find("HealthComponent:GetHealth\"")==health.npos);
    assert(source.find("if(!health || *health<=0)")!=source.npos);
    assert(source.find("!contractController || !health || *health<=0")!=source.npos);
    assert(source.find("Contract invalidated before victory could be credited")!=source.npos);
    const auto start=source.find("static bool DestroyOwnedActor(");
    const auto end=source.find("bool Cleanup()",start);
    assert(start!=source.npos && end!=source.npos);
    const auto helper=source.substr(start,end-start);
    assert(helper.find("bActorIsBeingDestroyed")!=helper.npos);
    const auto call=helper.find("ActorHelper::DestroyActor");
    assert(call!=helper.npos);
    assert(helper.find("handle.Get()",call)!=helper.npos);
    assert(helper.find("return false;",call)!=helper.npos);
    assert(helper.find("/Script/Engine.Actor:SetLifeSpan")!=helper.npos);
    assert(helper.find("/Script/AIModule.AIController")!=helper.npos);
    const auto cleanup=source.substr(end,source.find("void SpawnWave()",end)-end);
    assert(cleanup.find("DestroyOwnedActor(item.Controller)")!=cleanup.npos);
    assert(cleanup.find("DestroyOwnedActor(item.Actor)")!=cleanup.npos);
    assert(cleanup.find("DisarmDrops(actor)")<cleanup.find("DestroyOwnedActor(item.Controller)"));
    const auto footprint=source.find("location=PrepareGrounded(member.Spawn,world,location,static_cast<AActor*>(player),reservations,member.GroundOffset+10.0)");
    assert(source.find("member.RelativeGround?std::nullopt:std::optional<double>{member.Position[2]}")!=source.npos);
    assert(source.find("GroundedPlacement::NativeCapsule(player)")!=source.npos);
    assert(source.find("PS::Grounding::QueryWithFloorHint(")!=source.npos);
    assert(source.find("member.RelativeGround&&definition.Contract?std::optional<double>{participantFloor}:std::nullopt")!=source.npos);
    assert(source.find("retry scheduled; \"+pending.what()")!=source.npos);
    assert(source.find("impact,error,anchor)")==source.npos);
    const auto accept=source.substr(source.find("std::string Start("),source.find("std::string Cancel(")-source.find("std::string Start("));
    assert(accept.find("PrefetchCookedAssets(prefetch)")!=accept.npos);
    assert(accept.find("TrySpawnWave(true)")==accept.npos);
    assert(accept.find("SpawnManifest(member.Spawn)")!=accept.npos);
    assert(source.find("validated.insert(member.Spawn).second")!=source.npos);
    assert(source.find("warmElapsed<2.0")!=source.npos);
    assert(footprint!=source.npos&&footprint<source.find("auto* actor=Spawn(member.Spawn"));
    const auto despawn=source.find("void OnDespawn(");
    assert(source.find("DisarmDrops(owned[i].Actor.Get())",despawn)<source.find("run.Missing(i)",despawn));
    assert(cleanup.find("if(failed)remaining.push_back(item)")!=cleanup.npos);
    assert(source.find("return run.Active() || pendingStop || !owned.empty()")!=source.npos);
    assert(source.find("if(!Cleanup())return;")<source.find("run.Advance();"));
    assert(source.find("RestoreEventWeather();const bool removed=Cleanup()")!=source.npos);
    assert(source.find("HideOwnedArea();RestoreEventWeather();Cleanup();")!=source.npos);
    assert(source.find("HideOwnedArea();RestoreEventWeather();Report(definition.Key+\": complete\")")!=source.npos);
}
