#pragma once
#include "Loader/DialogueDefinition.h"
#include "Loader/NpcVisualEffect.h"
#include "Loader/TimeOfDay.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <map>
namespace DragonWilds::Events {
using Json=nlohmann::json;
struct SpawnTemplate {std::string Key,Class,Name;double Scale=1;std::string BossName;Json VisualEffect=Json::object();std::string LootRow;int PowerLevel=-1;};
inline SpawnTemplate ParseSpawn(const std::string& mod,const Json& data) {
    Dialogue::Fields(data,{"Id","Type","EventOnly","AIClass","DisplayName","BossName","Scale","VisualEffect","LootRow","PowerLevel"});
    if(data.at("Type")!="AISpawnPoint" || data.at("EventOnly")!=true)throw std::runtime_error("Event spawn requires Type AISpawnPoint and EventOnly true");
    SpawnTemplate value{Dialogue::Reference(mod,data.at("Id")),Dialogue::Text(data.at("AIClass"),1024),data.contains("DisplayName")?Dialogue::Text(data.at("DisplayName"),256):std::string{}};
    if(value.Class.front()!='/')throw std::runtime_error("Event AIClass requires an asset path");
    if(data.contains("PowerLevel")) {
        const auto& level=data.at("PowerLevel");
        if(!level.is_number_integer() || level<1 || level>100)
            throw std::runtime_error("Event PowerLevel must be an integer from 1 to 100; omit to inherit native power");
        value.PowerLevel=level.get<int>();
    }
    if(data.contains("BossName"))value.BossName=Dialogue::Text(data.at("BossName"),128);
    if(data.contains("LootRow")) {
        value.LootRow=Dialogue::Text(data.at("LootRow"),256);
        if(value.LootRow.find_first_of("\r\n\t")!=std::string::npos)throw std::runtime_error("LootRow cannot contain control whitespace");
    }
    if(value.Name.size()>128)throw std::runtime_error("DisplayName must contain between 1 and 128 characters");
    if(data.contains("Scale")) {
        if(!data["Scale"].is_number())throw std::runtime_error("Event spawn Scale must be numeric");
        value.Scale=data["Scale"].get<double>();
        if(!std::isfinite(value.Scale) || value.Scale<=0.0)throw std::runtime_error("Event spawn Scale must be finite and greater than zero");
    }
    if(data.contains("VisualEffect")) {
        NpcVisualEffect::Validate(data.at("VisualEffect"));
        if(data.at("VisualEffect").at("Type")!="Ghost Glow")throw std::runtime_error("Event VisualEffect currently requires Ghost Glow");
        value.VisualEffect=data.at("VisualEffect");
    }
    return value;
}
struct Member {std::string Spawn;std::array<double,3> Position;bool Ground=true,RelativeGround=false;double GroundOffset=100;};
enum class Scope { Participant, Party, World };
enum class AreaVisibility { Auto, Always, Never };
struct Area {std::array<double,3> Position{};double RadiusMeters=0;AreaVisibility Visibility=AreaVisibility::Auto;};
struct Messages {std::string Started,WaveStarted,BossStarted,Completed,Failed,Cancelled;};
struct HealthStage {double Percent=0;std::string Message,Spawn;};
struct ActorAction {double AfterSeconds=0;std::string Spawn;Json Cue=Json::object();};
struct Definition {
    std::string Key;
    double Timeout=300;
    Scope Audience=Scope::Participant;
    TimeOfDay::Requirement Time=TimeOfDay::Requirement::Any;
    TimeOfDay::Requirement DespawnTime=TimeOfDay::Requirement::Any;
    std::optional<std::string> Weather;
    std::optional<Area> EventArea;
    Messages Text;
    std::vector<HealthStage> HealthStages;
    std::vector<ActorAction> Actions;
    std::vector<std::vector<Member>> Waves;
};
inline std::string ScopeName(Scope value) {
    switch(value){case Scope::Participant:return "Participant";case Scope::Party:return "Party";case Scope::World:return "World";}
    throw std::runtime_error("Invalid event scope");
}
inline std::string VisibilityName(AreaVisibility value) {
    switch(value){case AreaVisibility::Auto:return "Auto";case AreaVisibility::Always:return "Always";case AreaVisibility::Never:return "Never";}
    throw std::runtime_error("Invalid event area visibility");
}
inline Definition Parse(const std::string& mod,const Json& data) {
    Dialogue::Fields(data,{"Id","TimeoutSeconds","Scope","TimeOfDay","SpawnTimeOfDay","DespawnTimeOfDay","Weather","Area","Messages","HealthStages","Actions","Waves"});
    Definition value;value.Key=Dialogue::Reference(mod,data.at("Id"));
    if(data.contains("TimeoutSeconds")) {
        if(!data["TimeoutSeconds"].is_number())throw std::runtime_error("Event TimeoutSeconds must be numeric");
        value.Timeout=data["TimeoutSeconds"].get<double>();
    }
    if(!std::isfinite(value.Timeout) || value.Timeout<10 || value.Timeout>900)throw std::runtime_error("Event timeout must be 10..900 seconds");
    if(data.contains("Scope")) {
        const auto scope=Dialogue::Text(data.at("Scope"),32);
        if(scope=="Participant")value.Audience=Scope::Participant;
        else if(scope=="Party")value.Audience=Scope::Party;
        else if(scope=="World")value.Audience=Scope::World;
        else throw std::runtime_error("Event Scope must be Participant, Party, or World");
    }
    if(data.contains("TimeOfDay") && data.contains("SpawnTimeOfDay"))
        throw std::runtime_error("Use SpawnTimeOfDay or legacy TimeOfDay, not both");
    if(data.contains("TimeOfDay"))value.Time=TimeOfDay::Parse(Dialogue::Text(data.at("TimeOfDay"),16));
    if(data.contains("SpawnTimeOfDay"))value.Time=TimeOfDay::Parse(Dialogue::Text(data.at("SpawnTimeOfDay"),16));
    if(data.contains("DespawnTimeOfDay")) {
        value.DespawnTime=TimeOfDay::Parse(Dialogue::Text(data.at("DespawnTimeOfDay"),16));
        if(value.DespawnTime==TimeOfDay::Requirement::Any)
            throw std::runtime_error("Event DespawnTimeOfDay must be Day or Night");
    }
    if(data.contains("Weather")) {
        const auto weather=Dialogue::Text(data.at("Weather"),32);
        static constexpr std::array allowed{"Sunny","Cloudy","Fog","Rain_Light","Rain_Heavy","Rain_Storm","Rain_LightningStorm","Sandstorm","Velgar_BossFight"};
        if(std::find(allowed.begin(),allowed.end(),weather)==allowed.end())
            throw std::runtime_error("Event Weather is not a native EWeatherType value");
        value.Weather=weather;
    }
    if(data.contains("Area")) {
        const auto& area=data.at("Area");Dialogue::Fields(area,{"Location","RadiusMeters","Visibility"});
        Area parsed;
        const auto& position=area.at("Location");
        if(!position.is_array() || position.size()!=3)throw std::runtime_error("Event Area.Location requires [X,Y,Z]");
        for(size_t i=0;i<3;++i) {
            if(!position[i].is_number())throw std::runtime_error("Event Area coordinates must be numeric");
            parsed.Position[i]=position[i].get<double>();
            if(!std::isfinite(parsed.Position[i]) || std::abs(parsed.Position[i])>100000000)throw std::runtime_error("Event Area coordinate out of range");
        }
        if(!area.at("RadiusMeters").is_number())throw std::runtime_error("Event Area.RadiusMeters must be numeric");
        parsed.RadiusMeters=area.at("RadiusMeters").get<double>();
        if(!std::isfinite(parsed.RadiusMeters) || parsed.RadiusMeters<0.01 || parsed.RadiusMeters>10000)
            throw std::runtime_error("Event Area.RadiusMeters must be 0.01..10000");
        if(area.contains("Visibility")) {
            const auto visibility=Dialogue::Text(area.at("Visibility"),16);
            if(visibility=="Auto")parsed.Visibility=AreaVisibility::Auto;
            else if(visibility=="Always")parsed.Visibility=AreaVisibility::Always;
            else if(visibility=="Never")parsed.Visibility=AreaVisibility::Never;
            else throw std::runtime_error("Event Area.Visibility must be Auto, Always, or Never");
        }
        value.EventArea=parsed;
    }
    if(data.contains("Messages")) {
        const auto& messages=data.at("Messages");
        Dialogue::Fields(messages,{"Started","WaveStarted","BossStarted","Completed","Failed","Cancelled"});
        const auto read=[&](const char* key,std::string& target){if(messages.contains(key))target=Dialogue::Text(messages.at(key),512);};
        read("Started",value.Text.Started);read("WaveStarted",value.Text.WaveStarted);read("BossStarted",value.Text.BossStarted);
        read("Completed",value.Text.Completed);read("Failed",value.Text.Failed);read("Cancelled",value.Text.Cancelled);
    }
    if(data.contains("HealthStages")) {
        const auto& stages=data.at("HealthStages");
        if(!stages.is_array() || stages.empty() || stages.size()>16)throw std::runtime_error("Event HealthStages requires 1..16 entries");
        std::set<double> thresholds;
        for(const auto& stage:stages) {
            Dialogue::Fields(stage,{"Percent","Message","SpawnID"});
            if(!stage.at("Percent").is_number())throw std::runtime_error("HealthStages.Percent must be numeric");
            HealthStage parsed;parsed.Percent=stage.at("Percent").get<double>();parsed.Message=Dialogue::Text(stage.at("Message"),512);
            if(!std::isfinite(parsed.Percent) || parsed.Percent<=0 || parsed.Percent>=100 || !thresholds.insert(parsed.Percent).second)
                throw std::runtime_error("HealthStages.Percent must be unique and between 0 and 100");
            if(stage.contains("SpawnID"))parsed.Spawn=Dialogue::Reference(mod,stage.at("SpawnID"));
            value.HealthStages.push_back(std::move(parsed));
        }
        std::sort(value.HealthStages.begin(),value.HealthStages.end(),[](const auto& a,const auto& b){return a.Percent>b.Percent;});
    }
    if(data.contains("Actions")) {
        const auto& actions=data.at("Actions");
        if(!actions.is_array() || actions.empty() || actions.size()>32)
            throw std::runtime_error("Event Actions requires 1..32 entries");
        for(const auto& action:actions) {
            Dialogue::Fields(action,{"AfterSeconds","SpawnID","Pose","Emote"});
            if(!action.contains("AfterSeconds") || !action.at("AfterSeconds").is_number())
                throw std::runtime_error("Event action AfterSeconds must be numeric");
            if(action.contains("Pose")==action.contains("Emote"))
                throw std::runtime_error("Event action requires exactly one of Pose or Emote");
            ActorAction parsed;parsed.AfterSeconds=action.at("AfterSeconds").get<double>();
            if(!std::isfinite(parsed.AfterSeconds) || parsed.AfterSeconds<0 || parsed.AfterSeconds>900)
                throw std::runtime_error("Event action AfterSeconds must be 0..900");
            parsed.Spawn=Dialogue::Reference(mod,action.at("SpawnID"));
            const bool emote=action.contains("Emote");
            const auto selection=HumanPose::Parse(action.at(emote?"Emote":"Pose"));
            if(selection.Path.empty())throw std::runtime_error("Event action requires a concrete pose or emote");
            if(emote && selection.Mode!=HumanPose::Playback::Once)
                throw std::runtime_error("Event Emote action requires Playback Once");
            parsed.Cue[emote?"Emote":"Pose"]=action.at(emote?"Emote":"Pose");
            value.Actions.push_back(std::move(parsed));
        }
        std::stable_sort(value.Actions.begin(),value.Actions.end(),[](const auto& a,const auto& b){return a.AfterSeconds<b.AfterSeconds;});
    }
    const auto& waves=data.at("Waves");
    if(!waves.is_array() || waves.empty() || waves.size()>8)throw std::runtime_error("Event requires 1..8 waves");
    size_t total=0;
    for(const auto& wave:waves) {
        if(!wave.is_array() || wave.empty() || wave.size()>16)throw std::runtime_error("Event wave requires 1..16 spawn entries");
        std::vector<Member> members;
        for(const auto& member:wave) {
            Dialogue::Fields(member,{"SpawnID","Location","GroundToSurface","GroundOffset"});
            Member item{Dialogue::Reference(mod,member.at("SpawnID")),{}};
            const auto& position=member.at("Location");
            if(!position.is_array() || position.size()!=3)throw std::runtime_error("Event Location requires [X,Y,Z]");
            for(size_t i=0;i<3;++i) {
                if(i==2 && position[i].is_string()) {
                    const auto text=position[i].get<std::string>();
                    if(text.empty() || text.front()!='$')throw std::runtime_error("Event Location.Z string must use $, $+offset, or $-offset");
                    item.Ground=true;
                    item.RelativeGround=true;
                    item.Position[i]=0;
                    if(text.size()>1 && text[1]!='+' && text[1]!='-')
                        throw std::runtime_error("Event Location.Z string must use $, $+offset, or $-offset");
                    if(text.size()>1)try {item.GroundOffset=std::stod(text.substr(1));}
                    catch(...) {throw std::runtime_error("Event Location.Z $ offset must be numeric");}
                    if(!std::isfinite(item.GroundOffset) || std::abs(item.GroundOffset)>100000)
                        throw std::runtime_error("Event Location.Z $ offset must be finite and between -100000 and 100000");
                    continue;
                }
                if(!position[i].is_number())throw std::runtime_error("Event coordinates must be numeric (Z also accepts $, $+offset, or $-offset)");
                item.Position[i]=position[i].get<double>();
                if(!std::isfinite(item.Position[i]) || std::abs(item.Position[i])>100000000)throw std::runtime_error("Event coordinate out of range");
            }
            if(member.contains("GroundToSurface")) {
                if(!member.at("GroundToSurface").is_boolean())throw std::runtime_error("Event GroundToSurface must be boolean");
                item.Ground=member.at("GroundToSurface").get<bool>();
                if(item.RelativeGround && !item.Ground)throw std::runtime_error("Event Location.Z $ requires GroundToSurface true");
            }
            if(member.contains("GroundOffset")) {
                if(item.RelativeGround)throw std::runtime_error("Use Event Location.Z $+offset or GroundOffset, not both");
                if(!member.at("GroundOffset").is_number())throw std::runtime_error("Event GroundOffset must be numeric");
                item.GroundOffset=member.at("GroundOffset").get<double>();
                if(!std::isfinite(item.GroundOffset) || std::abs(item.GroundOffset)>100000)
                    throw std::runtime_error("Event GroundOffset must be finite and between -100000 and 100000");
            }
            members.push_back(std::move(item));
        }
        total+=members.size();if(total>64)throw std::runtime_error("Event exceeds 64 total enemies");
        value.Waves.push_back(std::move(members));
    }
    return value;
}
class Catalog {
    std::map<std::string,Definition> entries;
public:
    void Load(const std::string& mod,const Json& data) {
        auto next=entries;
        const auto add=[&](const Json& item){auto value=Parse(mod,item);if(next.size()>=128 || !next.emplace(value.Key,std::move(value)).second)throw std::runtime_error("Duplicate event ID or event limit exceeded");};
        if(data.is_array())for(const auto& item:data)add(item);else add(data);
        entries=std::move(next);
    }
    const Definition& Find(const std::string& key) const {
        const auto it=entries.find(key);if(it==entries.end())throw std::runtime_error("Missing event: "+key);return it->second;
    }
};
enum class State { Idle, Active, Complete, Cancelled, Failed };
class Run {
    State state=State::Idle;
    std::vector<size_t> sizes;
    std::set<size_t> dead;
    size_t wave=0;
    double elapsed=0,timeout=0;
public:
    State Status()const{return state;}
    size_t Wave()const{return wave;}
    bool Active()const{return state==State::Active;}
    void Start(const Definition& definition) {
        if(Active())throw std::runtime_error("An event is already active");
        sizes.clear();for(const auto& members:definition.Waves)sizes.push_back(members.size());
        if(sizes.empty())throw std::runtime_error("Event has no waves");
        dead.clear();wave=0;elapsed=0;timeout=definition.Timeout;state=State::Active;
    }
    bool Death(size_t member){return Active() && member<sizes[wave] && dead.insert(member).second;}
    size_t ConfirmedDeaths()const{return dead.size();}
    bool WaveComplete()const{return Active() && dead.size()==sizes[wave];}
    bool IsDead(size_t member)const{return dead.contains(member);}
    void Advance(){if(!WaveComplete())throw std::runtime_error("Event wave is not complete");dead.clear();if(++wave==sizes.size())state=State::Complete;}
    void Tick(double delta){if(!Active())return;if(!std::isfinite(delta) || delta<0){state=State::Failed;return;}elapsed+=delta;if(elapsed>=timeout)state=State::Cancelled;}
    void Missing(size_t member){if(Active() && !IsDead(member))state=State::Failed;}
    void Cancel(){if(Active())state=State::Cancelled;}
    void Fail(){state=State::Failed;}
};
}
