#include "Loader/EventDefinition.h"
#include "Loader/EventIdentity.h"
#include "Generator/SpawnAuthoring.h"
#include "Loader/SpawnPlacementDraft.h"
#include "Loader/SpawnAuthoringFields.h"
#include <cassert>
using namespace DragonWilds::Events;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
Json Example(){return {{"Id","trial"},{"TimeoutSeconds",60},{"Scope","Party"},{"TimeOfDay","Night"},{"Weather","Rain_Storm"},
    {"Area",{{"Location",{100,200,300}},{"RadiusMeters",75},{"Visibility","Auto"}}},
    {"Messages",{{"Started","The trial begins."},{"WaveStarted","Wave {wave} of {waves}."},{"BossStarted","The boss arrives."},{"Completed","Victory."},{"Failed","Failed."},{"Cancelled","Cancelled."}}},
    {"Waves",Json::array({Json::array({{{"SpawnID","wolf"},{"Location",{1,2,3}}},{{"SpawnID","Other:wolf"},{"Location",{4,5,6}}}}),Json::array({{{"SpawnID","boss"},{"Location",{7,8,9}}}})})}};}
int main(){
    const auto arrayGround=DragonWilds::SpawnFields::ParseLocation(Json::array({17850,186650,"$"}));
    assert(arrayGround.Ground && arrayGround.GroundOffset==0 && arrayGround.Vector["X"]==17850 && arrayGround.Vector["Z"]==0);
    const auto arrayOffset=DragonWilds::SpawnFields::ParseLocation(Json::array({1,2,"$+250"}));
    assert(arrayOffset.Ground && arrayOffset.GroundOffset==250);
    assert(Rejects([&]{DragonWilds::SpawnFields::ParseLocation(Json::array({1,2}));}));
    const Json grid={{"Id","test"},{"Type","AISpawnPoint"},{"Location",{{"X",100},{"Y",200},{"Z","$100"}}},{"Rotation",{{"Yaw",90}}},{"Grid",{{"Rows",2},{"Columns",3},{"SpacingMeters",5}}}};
    const auto points=DragonWilds::SpawnFields::Expand(grid);
    assert(points.size()==6 && points[5]["Id"]=="test_1_2" && !points[0].contains("Grid"));
    assert(std::abs(points[1]["Location"]["Y"].get<double>()-700)<0.001 && points[1]["Location"]["Z"]=="$100");
    auto arrayGrid=grid;arrayGrid["Location"]=Json::array({100,200,"$"});
    const auto arrayPoints=DragonWilds::SpawnFields::Expand(arrayGrid);
    assert(arrayPoints.size()==6 && arrayPoints[0]["Location"]["Z"]=="$");
    auto invalidGrid=grid;invalidGrid["Grid"]["Rows"]=0;assert(Rejects([&]{DragonWilds::SpawnFields::Expand(invalidGrid);}));
    invalidGrid=grid;invalidGrid["Grid"]["Columns"]=20;invalidGrid["Grid"]["Rows"]=20;assert(Rejects([&]{DragonWilds::SpawnFields::Expand(invalidGrid);}));
    invalidGrid=grid;invalidGrid["EventOnly"]=true;assert(Rejects([&]{DragonWilds::SpawnFields::Expand(invalidGrid);}));
    auto drops=Json::array({{{"Item","/Game/Test.Test"},{"Min",1},{"Max",3},{"ChancePercent",50}}});
    DragonWilds::SpawnFields::Drops(drops);
    auto invalidDrops=drops;invalidDrops[0]["ChancePercent"]=101;assert(Rejects([&]{DragonWilds::SpawnFields::Drops(invalidDrops);}));
    invalidDrops=drops;invalidDrops[0]["Max"]=0;assert(Rejects([&]{DragonWilds::SpawnFields::Drops(invalidDrops);}));
    invalidDrops=drops;invalidDrops[0]["Item"]="relative";assert(Rejects([&]{DragonWilds::SpawnFields::Drops(invalidDrops);}));
    auto data=Example();auto def=Parse("Test",data);
    assert(def.Key=="Test:trial" && def.Waves.size()==2 && def.Waves[0][0].Spawn=="Test:wolf");
    assert(def.Waves[0][0].Ground && !def.Waves[0][0].RelativeGround);
    assert(def.Waves[0][1].Spawn=="Other:wolf");
    assert(def.Audience==Scope::Party && def.Time==DragonWilds::TimeOfDay::Requirement::Night && def.Weather && *def.Weather=="Rain_Storm" && def.EventArea && def.EventArea->RadiusMeters==75 && def.EventArea->Visibility==AreaVisibility::Auto);
    assert(def.Text.WaveStarted=="Wave {wave} of {waves}.");
    auto grounded=data;grounded["Waves"][0][0]["Location"]={1,2,"$"};
    grounded["Waves"][0][1]["Location"]={4,5,"$+125.5"};
    grounded["Waves"][1][0]["Location"]={7,8,"$-20"};
    const auto groundedDef=Parse("Test",grounded);
    assert(groundedDef.Waves[0][0].Ground && groundedDef.Waves[0][0].GroundOffset==0);
    assert(groundedDef.Waves[0][0].RelativeGround);
    assert(groundedDef.Waves[0][1].GroundOffset==125.5 && groundedDef.Waves[1][0].GroundOffset==-20);
    auto airborne=data;airborne["Waves"][0][0]["GroundToSurface"]=false;
    const auto airborneDef=Parse("Test",airborne);assert(!airborneDef.Waves[0][0].Ground);
    auto offset=data;offset["Waves"][0][0]["GroundOffset"]=75;
    assert(Parse("Test",offset).Waves[0][0].GroundOffset==75);
    auto mixedOffset=grounded;mixedOffset["Waves"][0][0]["GroundOffset"]=1;
    assert(Rejects([&]{Parse("Test",mixedOffset);}));
    auto floatingRelative=grounded;floatingRelative["Waves"][0][0]["GroundToSurface"]=false;
    assert(Rejects([&]{Parse("Test",floatingRelative);}));
    auto malformedGround=grounded;malformedGround["Waves"][0][0]["Location"][2]="$100";
    assert(Rejects([&]{Parse("Test",malformedGround);}));
    for(const auto& v:{Json(),Json("60"),Json(false),Json(0),Json(9),Json(901),Json(1e99)}) {auto bad=data;bad["TimeoutSeconds"]=v;assert(Rejects([&]{Parse("Test",bad);}));}
    for(const auto& v:{Json("all"),Json(7),Json(false)}){auto invalid=data;invalid["Scope"]=v;assert(Rejects([&]{Parse("Test",invalid);}));}
    for(const auto& v:{Json("Dusk"),Json(7),Json(false)}){auto invalid=data;invalid["TimeOfDay"]=v;assert(Rejects([&]{Parse("Test",invalid);}));}
    for(const auto& v:{Json("Rain"),Json(7),Json(false)}){auto invalid=data;invalid["Weather"]=v;assert(Rejects([&]{Parse("Test",invalid);}));}
    for(const auto& v:{Json("Sometimes"),Json(7),Json(false)}){auto invalid=data;invalid["Area"]["Visibility"]=v;assert(Rejects([&]{Parse("Test",invalid);}));}
    {auto invalid=data;invalid["Area"]["RadiusMeters"]=0;assert(Rejects([&]{Parse("Test",invalid);}));}
    {auto invalid=data;invalid["Area"]["Location"]={1,2,"$"};assert(Rejects([&]{Parse("Test",invalid);}));}
    {auto invalid=data;invalid["Messages"]["Started"]=std::string(513,'x');assert(Rejects([&]{Parse("Test",invalid);}));}
    auto stagedHealth=data;stagedHealth["HealthStages"]=Json::array({{{"Percent",75},{"Message","Still standing."}},{{"Percent",25},{"Message","You will fall."},{"SpawnID","boss"}}});
    const auto health=Parse("Test",stagedHealth);assert(health.HealthStages.size()==2 && health.HealthStages[0].Percent==75 && health.HealthStages[1].Spawn=="Test:boss");
    auto duplicateHealth=stagedHealth;duplicateHealth["HealthStages"][1]["Percent"]=75;assert(Rejects([&]{Parse("Test",duplicateHealth);}));
    auto posed=data;posed["Actions"]=Json::array({{{"AfterSeconds",3},{"SpawnID","wolf"},{"Pose",{{"Preset","Crouched"}}}}});
    const auto posedDef=Parse("Test",posed);assert(posedDef.Actions.size()==1 && posedDef.Actions[0].Spawn=="Test:wolf" && posedDef.Actions[0].Cue.contains("Pose"));
    auto badAction=posed;badAction["Actions"][0]["Emote"]={{"Preset","Wave"}};assert(Rejects([&]{Parse("Test",badAction);}));
    for(const auto& v:{Json(),Json::array(),Json::array({Json::array()})}) {auto bad=data;bad["Waves"]=v;assert(Rejects([&]{Parse("Test",bad);}));}
    auto bad=data;bad["Waves"]=Json::array();for(int i=0;i<9;++i)bad["Waves"].push_back(data["Waves"][0]);assert(Rejects([&]{Parse("Test",bad);}));
    bad=data;bad["Waves"][0][0]["Location"]={1,true,3};assert(Rejects([&]{Parse("Test",bad);}));
    Json spawn={{"Id","wolf"},{"Type","AISpawnPoint"},{"EventOnly",true},{"AIClass","/Game/Wolf.Wolf_C"},{"DisplayName","Named wolf"}};
    assert(ParseSpawn("Test",spawn).Name=="Named wolf");
    spawn["BossName"]="Named boss";
    assert(ParseSpawn("Test",spawn).BossName=="Named boss");
    spawn["LootRow"]="RS_TestGold";
    assert(ParseSpawn("Test",spawn).LootRow=="RS_TestGold");
    for(const auto& invalid:{Json(""),Json(12),Json("bad\nrow"),Json(std::string(257,'x'))}) {
        auto badLoot=spawn;badLoot["LootRow"]=invalid;assert(Rejects([&]{ParseSpawn("Test",badLoot);}));
    }
    const auto named=ParseSpawn("Test",spawn);
    const auto wire=EncodeIdentity(named);
    const auto identity=DecodeIdentity(wire);
    ValidateIdentity(identity,named);
    auto encounterIdentity=identity;encounterIdentity["version"]=3;encounterIdentity["event"]="Test:trial";
    const auto decodedEncounter=DecodeIdentity(encounterIdentity.dump());ValidateIdentity(decodedEncounter,named);
    assert(decodedEncounter.at("event")=="Test:trial");
    auto tool=named;tool.Key="__RuneSchemaTools:temporary_1";
    auto toolPayload=Identity(tool);toolPayload["kind"]="tool-ai";
    assert(ToolIdentity(DecodeIdentity(toolPayload.dump())).Name==named.Name);
    auto resource=toolPayload;resource["kind"]="tool-resource";resource["boss"]="";resource["loot"]="";
    assert(ToolIdentity(DecodeIdentity(resource.dump())).Class==named.Class);
    resource["visual"]={{"Type","Ghost Glow"},{"Overlay",true},{"BodyMaterial",false}};
    assert(!ToolIdentity(DecodeIdentity(resource.dump())).VisualEffect.empty());
    auto authored=PS::SpawnAuthoring::Placement(spawn,"authored_1",{100,200,300},90);
    assert(!authored.contains("EventOnly") && authored["Type"]=="AISpawnPoint" && authored["Id"]=="authored_1");
    assert(authored["Location"]["X"]==100 && authored["Rotation"]["Yaw"]==90);
    auto staged=PS::SpawnAuthoring::StageGroundDraft(authored,312.5,12.5);
    assert(staged["Location"]["Z"]=="$+12.5" && staged["$z"]==312.5);
    auto resolved=PS::SpawnAuthoring::ResolveGroundDrafts(Json::array({staged}));
    assert(resolved[0]["Location"]["Z"]==312.5 && !resolved[0].contains("$z"));
    const auto resourceExport=PS::SpawnAuthoring::Placement({{"Type","Actor"},{"Class","/Game/Rock.Rock_C"},{"UseNativeRespawn",true}},"rock_1",{1,2,3},0);
    assert(resourceExport["UseNativeRespawn"]==true && resourceExport["Class"]=="/Game/Rock.Rock_C");
    assert(Rejects([&]{PS::SpawnAuthoring::Placement(spawn,"../bad",{0,0,0},0);}));
    auto badTool=toolPayload;badTool["scale"]=100;assert(Rejects([&]{ToolIdentity(badTool);}));
    badTool=toolPayload;badTool["spawn"]="Other:temporary_1";assert(Rejects([&]{ToolIdentity(badTool);}));
    badTool=toolPayload;badTool["extra"]=true;assert(Rejects([&]{ToolIdentity(badTool);}));
    const auto lookup=[&](const std::string& key){auto value=named;value.Key=key;return Identity(value);};
    const auto manifest=BuildNetworkManifest(def,lookup);
    assert(manifest==BuildNetworkManifest(def,lookup));
    auto changedEvent=def;changedEvent.Timeout+=1;assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.Audience=Scope::World;assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.Time=DragonWilds::TimeOfDay::Requirement::Day;assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.Weather="Fog";assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.EventArea->RadiusMeters+=1;assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.Text.Completed+="!";assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;changedEvent.Waves[0][0].Position[0]+=1;assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    changedEvent=def;std::swap(changedEvent.Waves[0],changedEvent.Waves[1]);assert(manifest!=BuildNetworkManifest(changedEvent,lookup));
    assert(Rejects([&]{BuildNetworkManifest(def,[](const std::string&)->Json{throw std::runtime_error("Missing spawn");});}));
    for(const auto* field:{"class","name","boss","spawn","scale","loot"}) {
        auto changed=identity;changed[field]="mismatch";
        assert(Rejects([&]{ValidateIdentity(changed,named);}));
    }
    assert(Rejects([&]{DecodeIdentity(DragonWilds::NpcIdentity::Encode("Test","npc","hash"));}));
    assert(Rejects([&]{DecodeIdentity(std::string(4097,'x'));}));
    auto badIdentity=identity;badIdentity["version"]=1;assert(Rejects([&]{DecodeIdentity(badIdentity.dump());}));
    auto ghost=spawn;ghost["VisualEffect"]={{"Type","Ghost Glow"}};
    const auto ghostTemplate=ParseSpawn("Test",ghost);
    ValidateIdentity(DecodeIdentity(EncodeIdentity(ghostTemplate)),ghostTemplate);
    assert(Rejects([&]{ValidateIdentity(identity,ghostTemplate);}));
    ghost["VisualEffect"]["Type"]="Typo";assert(Rejects([&]{ParseSpawn("Test",ghost);}));
    badIdentity=identity;badIdentity["spawn"]="unqualified";assert(Rejects([&]{DecodeIdentity(badIdentity.dump());}));
    badIdentity=encounterIdentity;badIdentity["event"]="unqualified";assert(Rejects([&]{DecodeIdentity(badIdentity.dump());}));
    auto bossOnly=spawn;bossOnly.erase("DisplayName");
    assert(ParseSpawn("Test",bossOnly).Name.empty() && ParseSpawn("Test",bossOnly).BossName=="Named boss");
    for(const auto* field:{"DisplayName","BossName"})for(const auto& invalid:{Json(""),Json(12),Json(std::string(129,'x'))}) {
        auto badName=spawn;badName[field]=invalid;assert(Rejects([&]{ParseSpawn("Test",badName);}));
    }
    for(const auto* field:{"Location","UseNativeRespawn","SpudGuid","CharacterProperties"}){auto invalid=spawn;invalid[field]=true;assert(Rejects([&]{ParseSpawn("Test",invalid);}));}
    spawn["EventOnly"]=false;assert(Rejects([&]{ParseSpawn("Test",spawn);}));
    Run run;run.Start(def);assert(run.Active());assert(Rejects([&]{run.Start(def);}));
    assert(run.Death(0));assert(!run.Death(0));assert(!run.Death(999));assert(run.ConfirmedDeaths()==1);assert(!run.WaveComplete());
    assert(run.Death(1));assert(run.ConfirmedDeaths()==2);assert(run.WaveComplete());run.Advance();assert(run.ConfirmedDeaths()==0);assert(run.Wave()==1 && run.Active());
    run.Death(0);run.Advance();assert(run.Status()==State::Complete);
    run.Start(def);run.Missing(0);assert(run.Status()==State::Failed);
    run.Start(def);run.Death(0);run.Missing(0);assert(run.Active());
    run.Tick(60);assert(run.Status()==State::Cancelled);
    run.Start(def);run.Cancel();assert(!run.Death(0));assert(run.ConfirmedDeaths()==0);assert(run.Status()==State::Cancelled);
    Catalog catalog;catalog.Load("Test",data);assert(Rejects([&]{catalog.Load("Test",data);}));
    assert(Rejects([&]{catalog.Find("Other:trial");}));
    const Json dialogue={{"Id","test"},{"Entry","start"},{"Nodes",{{"start",{{"Text","Ready?"},{"Choices",Json::array({{{"Id","go"},{"Text","Go"},{"Next","done"},{"Event",{{"Id","trial"},{"Action","Start"}}}}})}}},{"done",{{"Text","Started"},{"Choices",Json::array({{{"Id","bye"},{"Text","Bye"},{"End",true}}})}}}}}};
    assert(DragonWilds::Dialogue::Parse("Test",dialogue).Events.contains("Test:trial"));
    auto mixed=dialogue;mixed["Nodes"]["start"]["Choices"][0]["Quest"]={{"Id","supper"},{"Action","Accept"}};
    const auto combined=DragonWilds::Dialogue::Parse("Test",mixed);
    assert(combined.Events.contains("Test:trial") && combined.Quests.contains("Test:supper"));
}
