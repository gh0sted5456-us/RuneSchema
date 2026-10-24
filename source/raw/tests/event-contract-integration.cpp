#include "Loader/EventQuestContract.h"
#include "Loader/EventIdentity.h"
#include "Generator/LoaderSchemas.h"
#include <fstream>
#include <filesystem>
#include <iostream>
using namespace DragonWilds::Events;
void Check(bool ok){if(!ok)throw std::runtime_error("Contract regression failed");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(int argc,char** argv){
    try {
        const auto root=std::filesystem::path(argc>1?argv[1]:".");
        Json event={{"Id","arena"},{"Scope","Participant"},{"Area",{{"Location",{10000,20000,300}},{"RadiusMeters",60}}},
            {"Contract",{{"QuestID","work"},{"SpawnOrigin","Participant"},{"ActivationRadiusMeters",15},{"ExitGraceSeconds",10}}},
            {"Waves",Json::array({Json::array({{{"SpawnID","guard"},{"Location",{2000,0,"$"}}}}),Json::array({{{"SpawnID","boss"},{"Location",{-2000,0,"$"}}}})})}};
        auto parsed=Parse("Test",event);Check(parsed.Contract->Quest=="Test:work");
        const auto anchored=AnchorContract(parsed,{10100,20100,300});
        Check(anchored.Waves[0][0].Position[0]==12100 && anchored.Waves[1][0].Position[0]==8100);
        Check(parsed.Waves[0][0].Position[0]==2000);
        Reject([&]{AnchorContract(parsed,{12000,20000,300});});
        auto bad=event;bad.erase("Area");Reject([&]{Parse("Test",bad);});
        bad=event;bad["Contract"]["ActivationRadiusMeters"]=61;Reject([&]{Parse("Test",bad);});
        bad=event;bad["Contract"]["ExitGraceSeconds"]=-1;Reject([&]{Parse("Test",bad);});
        bad=event;bad["Contract"]["ExitGraceSeconds"]="10";Reject([&]{Parse("Test",bad);});
        bad=event;bad["Scope"]="Party";Reject([&]{Parse("Test",bad);});
        bad=event;bad["DespawnTimeOfDay"]="Day";Reject([&]{Parse("Test",bad);});
        bad=event;bad["Waves"][0][0]["Location"]={0,0,"$"};Reject([&]{Parse("Test",bad);});
        bad=event;bad["Waves"][0][0]["Location"]={11000,0,"$"};Reject([&]{Parse("Test",bad);});
        bad=event;bad["Waves"][0][0]["Location"]={2000,0,100};Reject([&]{Parse("Test",bad);});
        auto tooSmall=parsed;tooSmall.EventArea->RadiusMeters=20;Reject([&]{AnchorContract(tooSmall,{10100,20000,300});});
        ContractBoundary boundary;Check(boundary.Tick(false,6,10));Check(boundary.Tick(true,1,10));
        Check(boundary.OutsideSeconds()==0);Check(boundary.Tick(false,9,10));Check(!boundary.Tick(false,1,10));
        boundary.Reset();Check(!boundary.Tick(false,0,0));Check(!boundary.Tick(true,-1,10));
        Json quest={{"Id","work"},{"PersistenceID","AAAAAAAAAAAAAAAAAAAAAA"},{"Title","Work"},{"Description","Survive."},
            {"Objective",{{"Id","victory"},{"Text","Win"},{"Type","Kill"},{"AIClasses",{"/Game/Test.Test_C"}},{"EventID","arena"},{"SpawnID","boss"},{"Count",1}}},
            {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}}};
        auto q=DragonWilds::Quests::Parse("Test",quest);ValidateContractQuest(parsed,q);
        auto invalid=q;invalid.Required.Count=2;Reject([&]{ValidateContractQuest(parsed,invalid);});
        invalid=q;invalid.Kill->SpawnKey="Test:guard";Reject([&]{ValidateContractQuest(parsed,invalid);});
        Catalog catalog;catalog.Load("Test",event);Check(catalog.IsContractQuest("Test:work"));
        bad=event;bad["Id"]="second";Reject([&]{catalog.Load("Test",bad);});
        const auto lookup=[](const std::string& key){return Json{{"spawn",key}};};
        const auto manifest=BuildNetworkManifest(parsed,lookup);auto changed=parsed;changed.Contract->ExitGrace+=1;
        Check(manifest!=BuildNetworkManifest(changed,lookup));
        auto legacy=event;legacy.erase("Contract");Check(!BuildNetworkManifest(Parse("Test",legacy),lookup).contains("contract"));
        Run run;run.Start(parsed);Check(run.Death(0));run.Advance();Check(run.Active());run.Cancel();Check(run.Status()==State::Cancelled);
        run.Start(parsed);run.Death(0);run.Advance();run.Death(0);run.Advance();Check(run.Status()==State::Complete);
        const auto schemas=PS::JsonSchemaGenerator::LoaderSchemas();
        for(const auto& [name,schema]:schemas.items()){std::ofstream out(root/"docs"/"schemas"/(name+".schema.json"));if(!out)throw std::runtime_error("Cannot write generated schema");out<<schema.dump(2)<<'\n';}
        if(argc>2) {
            const auto content=std::filesystem::path(argv[2]);
            const auto load=[&](const char* file){std::ifstream input(content/file);if(!input)throw std::runtime_error(std::string("Missing content: ")+file);Json value;input>>value;return value;};
            const std::string mod="JobBoardStarterQuests";
            const auto documents=load("quests/10-StarterQuests.json"),encounters=load("events/30-ContractEncounters.json"),templates=load("spawns/30-EncounterTemplates.json");
            Check(documents.size()==20 && encounters.size()==12);
            std::map<std::string,DragonWilds::Quests::Definition> questMap;
            std::map<std::string,SpawnTemplate> spawnMap;
            std::set<std::string> identities;
            DragonWilds::Quests::Catalog questCatalog;
            for(const auto& document:documents){auto value=DragonWilds::Quests::Parse(mod,document);Check(identities.insert(value.PersistenceId).second);questCatalog.Add(mod,document);questMap.emplace(value.Key,value);}
            for(const auto& document:templates){auto value=ParseSpawn(mod,document);Check(spawnMap.emplace(value.Key,value).second);}
            Catalog eventCatalog;
            for(const auto& document:encounters){
                const auto value=Parse(mod,document);ValidateContractQuest(value,questMap.at(value.Contract->Quest));eventCatalog.Load(mod,document);
                const auto& q=questMap.at(value.Contract->Quest);
                Check(std::find(q.Kill->Classes.begin(),q.Kill->Classes.end(),spawnMap.at(q.Kill->SpawnKey).Class)!=q.Kill->Classes.end());
                AnchorContract(value,value.EventArea->Position);
                for(const auto& wave:value.Waves)for(const auto& member:wave)Check(spawnMap.contains(member.Spawn));
            }
            for(const auto& document:load("dialogue/30-EncounterLedgers.json")){auto graph=DragonWilds::Dialogue::Parse(mod,document);for(const auto& key:graph.Events)eventCatalog.Find(key);}
            const auto scrolls=load("assets/20-QuestContracts.json"),recipeRows=load("recipes/20-QuestContracts.json");
            Check(scrolls.size()==20 && recipeRows.size()==20);
            for(const auto& [asset,scroll]:scrolls.items()){
                const auto id=scroll.at("PersistenceID").get<std::string>();Check(DragonWilds::IsCanonicalPersistenceId(id)&&identities.insert(id).second);
                Check(scroll.at("$QuestScroll").at("PersistenceID")==id);Check(questMap.contains(scroll.at("$QuestScroll").at("QuestID").get<std::string>()));
            }
            for(const auto& [key,row]:recipeRows.items()){
                const auto id=row.at("PersistenceID").get<std::string>();Check(DragonWilds::IsCanonicalPersistenceId(id)&&identities.insert(id).second);
                Check(scrolls.contains(row.at("ItemsCreated").at(0).at("ItemData").get<std::string>()));
            }
            std::cout<<"PASS: 20 quests, 12 contracts, target classes, all spawn links, dialogue, 20 scroll bindings and 60 unique stable identities.\n";
        }
        std::cout<<"PASS: contract parser, anchoring, boundary, target provenance, duplicate bindings, manifest and wave outcomes.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
