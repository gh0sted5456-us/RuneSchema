#include "Loader/EventDefinition.h"
#include <fstream>
#include <cassert>
int main(int argc,char** argv) {
    assert(argc==2); const std::filesystem::path root(argv[1]);
    const auto read=[&](const char* path){std::ifstream file(root/path);assert(file.good());return nlohmann::json::parse(file);};
    const std::string mod="RuneSchema2VendorTest";
    std::map<std::string,DragonWilds::Events::SpawnTemplate> spawns;
    for(const auto& raw:read("spawns/90-GrumbleGoblins.json")) {auto s=DragonWilds::Events::ParseSpawn(mod,raw);assert(!s.Name.empty());spawns.emplace(s.Key,s);}
    const auto event=DragonWilds::Events::Parse(mod,read("events/90-GrumbleGoblinWaves.json"));
    assert(event.Waves.size()==3);
    size_t total=0;
    for(const auto& wave:event.Waves)for(const auto& member:wave) {
        assert(spawns.contains(member.Spawn));++total;
        assert(std::hypot(member.Position[0]-43629,member.Position[1]-175053)<20000);
        assert(std::hypot(member.Position[0]-41435,member.Position[1]-172509)<20000);
    }
    assert(total==5);
    auto dialogue=DragonWilds::Dialogue::Parse(mod,read("dialogue/40-Grumble.jsonc"));
    assert(dialogue.Events.contains(event.Key));
    DragonWilds::Events::Run run;run.Start(event);
    for(size_t wave=0;wave<event.Waves.size();++wave) {
        assert(run.Active() && run.Wave()==wave);
        for(size_t i=0;i<event.Waves[wave].size();++i) {assert(run.Death(i));assert(!run.Death(i));}
        assert(run.WaveComplete());run.Advance();
    }
    assert(run.Status()==DragonWilds::Events::State::Complete);
    std::map<std::string,DragonWilds::Events::SpawnTemplate> bosses;
    for(const auto& raw:read("spawns/91-GrumbleZogres.json")) {
        auto s=DragonWilds::Events::ParseSpawn(mod,raw);
        assert(s.Class=="/FutureMajorVersion/Gameplay/AI/ZombieFaction/Zogre/BP_AI_Zogre_Character.BP_AI_Zogre_Character_C");
        assert(!s.BossName.empty() && s.BossName==s.Name);bosses.emplace(s.Key,s);
    }
    const auto bossEvent=DragonWilds::Events::Parse(mod,read("events/91-GrumbleZogreTrial.json"));
    assert(bossEvent.Waves.size()==3 && dialogue.Events.contains(bossEvent.Key));
    for(const auto& wave:bossEvent.Waves) {assert(wave.size()==1);assert(bosses.contains(wave[0].Spawn));}
}
