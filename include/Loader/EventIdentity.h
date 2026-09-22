#pragma once
#include "Loader/EventDefinition.h"
#include "Loader/NpcIdentityPayload.h"
namespace DragonWilds::Events {
inline Json Identity(const SpawnTemplate& spawn) {
    Json result={{"version",2},{"kind","event-ai"},{"spawn",spawn.Key},{"class",spawn.Class},
        {"name",spawn.Name},{"boss",spawn.BossName},{"scale",spawn.Scale},{"visual",spawn.VisualEffect},{"loot",spawn.LootRow}};
    if(spawn.PowerLevel!=-1)result["power"]=spawn.PowerLevel;
    return result;
}
inline std::string EncodeIdentity(const SpawnTemplate& spawn) {
    const auto text=Identity(spawn).dump();
    if(text.size()>NpcIdentity::MaxPayload)throw std::runtime_error("Event identity exceeds network payload limit");
    return text;
}
inline Json DecodeIdentity(const std::string& text) {
    if(text.empty() || text.size()>NpcIdentity::MaxPayload)throw std::runtime_error("Invalid event identity length");
    auto data=Json::parse(text);
    if(!data.is_object() || !data.contains("version") || !data["version"].is_number_integer()
        || ((data["version"]!=2 || data.size()!=9+size_t(data.contains("power"))) && (data["version"]!=3 || data.size()!=10+size_t(data.contains("power")) || !data.contains("event") || !data["event"].is_string()))
        || (data.value("kind",std::string{})!="event-ai" && data.value("kind",std::string{})!="tool-ai" && data.value("kind",std::string{})!="tool-resource")
        || !data.contains("spawn") || !data["spawn"].is_string())throw std::runtime_error("Invalid event identity protocol");
    if(data.contains("power") && (!data["power"].is_number_integer() || data["power"]<1 || data["power"]>100))
        throw std::runtime_error("Invalid event power identity");
    const auto key=data["spawn"].get<std::string>();
    if(key.size()>512 || key.find(':')==key.npos || Dialogue::Reference("_",key)!=key)
        throw std::runtime_error("Invalid event spawn identity");
    if(data["version"]==3) {
        const auto event=data.at("event").get<std::string>();
        if(data.at("kind")!="event-ai" || event.size()>512 || event.find(':')==event.npos || Dialogue::Reference("_",event)!=event)
            throw std::runtime_error("Invalid event encounter identity");
    }
    return data;
}
inline SpawnTemplate ToolIdentity(const Json& payload) {
    const auto key=payload.at("spawn").get<std::string>();
    if((payload.at("kind")!="tool-ai" && payload.at("kind")!="tool-resource") || !key.starts_with("__RuneSchemaTools:"))throw std::runtime_error("Invalid tool identity");
    Json input={{"Id",key.substr(key.find(':')+1)},{"Type","AISpawnPoint"},{"EventOnly",true},
        {"AIClass",payload.at("class")},{"Scale",payload.at("scale")}};
    if(payload.contains("power"))input["PowerLevel"]=payload.at("power");
    for(const auto& pair: {std::pair{"name","DisplayName"},std::pair{"boss","BossName"},std::pair{"loot","LootRow"}})
        if(payload.at(pair.first)!= "")input[pair.second]=payload.at(pair.first);
    if(!payload.at("visual").empty())input["VisualEffect"]=payload.at("visual");
    auto result=ParseSpawn("__RuneSchemaTools",input);
    auto expected=Identity(result);expected["kind"]=payload.at("kind");
    if(payload.at("kind")=="tool-resource" && (!result.BossName.empty() || !result.LootRow.empty()))throw std::runtime_error("Unsupported resource identity fields");
    if(expected!=payload)throw std::runtime_error("Malformed tool identity");
    return result;
}
inline void ValidateIdentity(const Json& payload,const SpawnTemplate& local) {
    auto normalized=payload;
    if(normalized.value("version",0)==3){normalized.erase("event");normalized["version"]=2;}
    if(normalized!=Identity(local))throw std::runtime_error("Server/client event spawn definition mismatch");
}
template<class Lookup> Json BuildNetworkManifest(const Definition& event,Lookup lookup) {
    Json waves=Json::array(),spawns=Json::object();
    for(const auto& wave:event.Waves) {
        auto members=Json::array();
        for(const auto& member:wave) {
            members.push_back({{"spawn",member.Spawn},{"position",member.Position},
                {"ground",member.Ground},{"relativeGround",member.RelativeGround},{"groundOffset",member.GroundOffset}});
            if(!spawns.contains(member.Spawn))spawns[member.Spawn]=lookup(member.Spawn);
        }
        waves.push_back(std::move(members));
    }
    Json area=nullptr;
    if(event.EventArea)area={{"position",event.EventArea->Position},{"radius",event.EventArea->RadiusMeters},
        {"visibility",VisibilityName(event.EventArea->Visibility)}};
    const Json messages={{"started",event.Text.Started},{"waveStarted",event.Text.WaveStarted},
        {"bossStarted",event.Text.BossStarted},{"completed",event.Text.Completed},
        {"failed",event.Text.Failed},{"cancelled",event.Text.Cancelled}};
    Json health=Json::array();for(const auto& stage:event.HealthStages)health.push_back({{"percent",stage.Percent},{"message",stage.Message},{"spawn",stage.Spawn}});
    Json actions=Json::array();for(const auto& action:event.Actions)actions.push_back({{"afterSeconds",action.AfterSeconds},{"spawn",action.Spawn},{"cue",action.Cue}});
    return {{"key",event.Key},{"timeout",event.Timeout},{"scope",ScopeName(event.Audience)},{"timeOfDay",TimeOfDay::Name(event.Time)},
        {"despawnTimeOfDay",TimeOfDay::Name(event.DespawnTime)},
        {"weather",event.Weather?Json(*event.Weather):Json(nullptr)},
        {"area",area},{"messages",messages},{"healthStages",health},{"actions",actions},{"waves",waves},{"spawns",spawns}};
}
}
