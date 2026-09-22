#include "Loader/DialogueDefinition.h"
#include "Loader/EventDefinition.h"
#include "Loader/NpcCatalog.h"
#include "Loader/QuestDefinition.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using Json=nlohmann::json;
namespace fs=std::filesystem;

static Json Read(const fs::path& path) {
    std::ifstream file(path);
    assert(file);
    return Json::parse(file,nullptr,true,true);
}

template<class Callback>
static void Each(const fs::path& directory,Callback callback) {
    for(const auto& entry:fs::directory_iterator(directory)) {
        if(!entry.is_regular_file())continue;
        const auto extension=entry.path().extension().string();
        if(extension==".json" || extension==".jsonc")callback(Read(entry.path()));
    }
}

int main() {
    const fs::path root="examples/RSv16/ExampleMods/RuneSchema2VendorTest";
    assert(fs::is_directory(root));
    const std::string mod="RuneSchema2VendorTest";

    DragonWilds::NpcCatalog npcs;
    std::set<std::string> stores;
    Each(root/"vendors",[&](const Json& data){
        npcs.AddStore(mod,data);
        stores.insert(DragonWilds::NpcCatalog::Key(mod,data.at("Id").get<std::string>()));
        assert(!data.contains("Location") && !data.contains("Actor") && !data.contains("Mesh"));
    });
    Each(root/"npc",[&](const Json& data){npcs.AddNpc(mod,data);});

    std::set<std::string> quests;
    Each(root/"quests",[&](const Json& data){
        const auto parsed=DragonWilds::Quests::ParseDefinition(mod,data,false);
        quests.insert(parsed.Key);
        const auto check=[](const auto& objective){
            assert(!objective.ProgressText.empty());
            assert(!objective.CompleteText.empty());
            assert(objective.AnnounceProgress);
        };
        if(parsed.Stages.empty())check(parsed);
        else for(const auto& [stage,objectives]:parsed.Stages) {
            assert(!stage.empty());
            for(const auto& objective:objectives)check(objective);
        }
    });

    std::set<std::string> spawns;
    Each(root/"spawns",[&](const Json& data){
        assert(data.is_array());
        for(const auto& entry:data)
            spawns.insert(DragonWilds::Events::ParseSpawn(mod,entry).Key);
    });
    std::set<std::string> events;
    Each(root/"events",[&](const Json& data){
        const auto load=[&](const Json& entry){
            const auto parsed=DragonWilds::Events::Parse(mod,entry);
            events.insert(parsed.Key);
            for(const auto& wave:parsed.Waves)for(const auto& member:wave)
                assert(spawns.contains(member.Spawn));
        };
        if(data.is_array())for(const auto& entry:data)load(entry);else load(data);
    });

    Each(root/"dialogue",[&](const Json& data){
        const auto parsed=DragonWilds::Dialogue::Parse(mod,data);
        DragonWilds::Dialogue::ValidateStores(parsed,stores);
        for(const auto& quest:parsed.Quests)assert(quests.contains(quest));
        for(const auto& event:parsed.Events)assert(events.contains(event));
        assert(data.dump().find("WhenQuest")==std::string::npos);
    });

    const auto resolved=npcs.Resolve();
    assert(resolved.size()==9);
    bool cow=false,book=false;
    for(const auto& npc:resolved) {
        if(npc.Data.contains("QuestID"))assert(quests.contains(npc.Data.at("QuestID").get<std::string>()));
        if(npc.Data.at("Id")=="cabbage_trader_cow") {
            cow=true;assert(npc.Data.at("Stage")=="Merchant");
            assert(npc.Data.at("VendorID")==mod+":cabbage_trader");
        }
        if(npc.Data.at("Id")=="mortimer_village_book") {
            book=true;assert(npc.Data.at("LoreEntry")==mod+":rs_mortimer_village_ledger");
        }
    }
    assert(cow && book);
}
