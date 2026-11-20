#include "Core/ModAchievements.h"
#include <cassert>
#include <cmath>
#include <iostream>
// Mod achievements parse strictly, progress profile-wide, and share a bounded summary.
using namespace PS;using Json=nlohmann::json;namespace A=ModAchievements;
static bool Rejects(const Json& document){try{A::Parse(document,"mods/goblins","Goblins");}catch(const std::exception&){return true;}return false;}
int main(){
    const Json file=Json::parse(R"({"format":1,"achievements":[
        {"id":"goblin-slayer","title":"Goblin Slayer","description":"Defeat 3 goblins.","points":10,
         "conditions":[{"event":"kill","subject":"/Game/Mods/Goblins/*","count":3}]},
        {"id":"chief","title":"Chief Toppled","points":25,"hidden":true,
         "conditions":[{"event":"custom","function":"/Game/Mods/Goblins/BP_Chief.BP_Chief_C:OnDefeated"},
                       {"event":"craft","subject":"/Game/Mods/Goblins/DA_Recipe_Totem.DA_Recipe_Totem"}]},
        {"id":"miner","title":"Deep Miner","points":5,"conditions":[{"event":"skill-xp","subject":"/Game/Skills/DA_Mining.DA_Mining","atLeast":1000}]},
        {"id":"quester","title":"Wolf Hunter","points":5,"match":"any",
         "conditions":[{"event":"quest-updated","subject":"/Game/Q/DA_Wolves.DA_Wolves","where":{"UpdatedQuest.bComplete":true}},
                       {"event":"death","count":100}]}]})");
    const auto definitions=A::Parse(file,"mods/goblins","Goblins");
    assert(definitions.size()==4&&definitions[1].Hidden&&definitions[3].Any&&definitions[0].Key()=="mods/goblins|goblin-slayer");
    assert((A::WatchList(definitions)==Json::array({"/Game/Mods/Goblins/BP_Chief.BP_Chief_C:OnDefeated"})));
    // Validation messages for modders.
    assert(Rejects({{"format",2},{"achievements",Json::array()}}));
    const auto one=[](const char* achievement){return Json::parse(std::string(R"({"format":1,"achievements":[)")+achievement+"]}");};
    assert(Rejects(one(R"({"id":"a b","title":"T","conditions":[{"event":"kill"}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","conditions":[{"event":"teleport"}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","points":101,"conditions":[{"event":"kill"}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","conditions":[{"event":"custom"}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","conditions":[{"event":"kill","atLeast":5}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","conditions":[{"event":"kill","subject":"relative/path"}]})")));
    assert(Rejects(one(R"({"id":"a","title":"T","conditions":[]})")));
    assert(!Rejects(one(R"({"id":"a","title":"T","conditions":[{"event":"kill"}]})")));
    {Json many=one(R"({"id":"x","title":"T","points":100,"conditions":[{"event":"kill"}]})");for(int i=0;i<10;++i){auto row=many["achievements"][0];row["id"]="a"+std::to_string(i);many["achievements"].push_back(row);}assert(Rejects(many));}
    // Progress.
    A::Progress progress;
    const auto kill=[](const char* subject){return Json{{"Kind","kill"},{"Subject",subject},{"Amount",1},{"Time",100}};};
    assert(progress.Apply(definitions,kill("/Game/Vanilla/BP_Wolf.BP_Wolf_C")).empty()&&progress.Entries.empty());
    assert(progress.Apply(definitions,kill("/Game/Mods/Goblins/BP_Goblin.BP_Goblin_C")).empty());
    assert(progress.Apply(definitions,kill("/Game/Mods/Goblins/BP_Goblin.BP_Goblin_C")).empty());
    assert(std::abs(progress.Fraction(definitions[0])-2.0/3)<1e-9);
    auto unlocked=progress.Apply(definitions,kill("/Game/Mods/Goblins/BP_GoblinArcher.BP_GoblinArcher_C"));
    assert(unlocked.size()==1&&unlocked[0].Title=="Goblin Slayer"&&unlocked[0].Points==10&&progress.Points()==10);
    assert(progress.Apply(definitions,kill("/Game/Mods/Goblins/BP_Goblin.BP_Goblin_C")).empty()); // Unlocks once.
    // AND across conditions.
    assert(progress.Apply(definitions,{{"Kind","custom"},{"Subject","/Game/Mods/Goblins/BP_Chief.BP_Chief_C:OnDefeated"},{"Amount",1}}).empty());
    assert(std::abs(progress.Fraction(definitions[1])-.5)<1e-9);
    unlocked=progress.Apply(definitions,{{"Kind","craft"},{"Subject","/Game/Mods/Goblins/DA_Recipe_Totem.DA_Recipe_Totem"},{"Amount",2}});
    assert(unlocked.size()==1&&unlocked[0].Key=="mods/goblins|chief"&&progress.Points()==35);
    // skill-xp atLeast uses the running total, not the gain.
    assert(progress.Apply(definitions,{{"Kind","skill-xp"},{"Subject","/Game/Skills/DA_Mining.DA_Mining"},{"Amount",40},{"Value",900}}).empty());
    assert(progress.Apply(definitions,{{"Kind","skill-xp"},{"Subject","/Game/Skills/DA_Mining.DA_Mining"},{"Amount",120},{"Value",1020}}).size()==1);
    // where + any.
    assert(progress.Apply(definitions,Json::parse(R"({"Kind":"quest-updated","Subject":"/Game/Q/DA_Wolves.DA_Wolves","Parameters":{"UpdatedQuest":{"bComplete":false}}})")).empty());
    assert(progress.Apply(definitions,Json::parse(R"({"Kind":"quest-updated","Subject":"/Game/Q/DA_Wolves.DA_Wolves","Parameters":{"UpdatedQuest":{"bComplete":true}}})")).size()==1);
    assert(progress.Points()==45&&progress.UnlockedCount()==4);
    assert(progress.Apply(definitions,{{"Kind","kill"},{"Amount",-5}}).empty()&&progress.Apply(definitions,{{"Kind","kill"},{"Amount",NAN}}).empty());
    // Persistence round trip and session resume.
    progress.Resume("777");progress.Cursor=42;
    auto restored=A::Progress::Parse(Json::parse(progress.Document().dump()));
    assert(restored.Points()==45&&restored.Session=="777"&&restored.Cursor==42&&restored.Unlocked("mods/goblins|chief"));
    assert(!restored.Resume("777")&&restored.Cursor==42&&restored.Resume("778")&&restored.Cursor==0);
    bool threw=false;try{A::Progress::Parse(Json::parse(R"({"Format":1,"Entries":{"bad key":{"Counters":[]}}})"));}catch(...){threw=true;}assert(threw);
    // Earned points survive the mod being removed: no definitions needed.
    assert(A::Progress::Parse(progress.Document()).Points()==45);
    // Shared summary.
    const auto summary=A::Summary(progress);
    assert(summary["Points"]==45&&summary["Unlocked"]==4&&summary["Mods"].size()==1&&summary["Mods"][0]["Name"]=="Goblins"&&summary["Recent"].size()==3);
    assert(summary.dump().size()<=A::SummaryBytes&&A::ValidateSummary(summary)==summary);
    auto forged=summary;forged["Admin"]=true;assert(A::ValidateSummary(forged).is_null());
    forged=summary;forged["Points"]=-1;assert(A::ValidateSummary(forged).is_null());
    forged=summary;forged["Recent"][0]["Title"]=std::string(1,'\x07');assert(A::ValidateSummary(forged).is_null());
    assert(A::ValidateSummary(nullptr).is_null()&&A::ValidateSummary("text").is_null());
    // Many mods stay under the size cap.
    A::Progress wide;for(int i=0;i<40;++i){A::Progress::Entry entry;entry.Unlocked=uint64_t(i+1);entry.Points=100;entry.Title=std::string(64,'T');entry.Owner="mods/m"+std::to_string(i);entry.ModName=std::string(96,'M');wide.Entries["mods/m"+std::to_string(i)+"|a"]=entry;}
    const auto big=A::Summary(wide);assert(big.dump().size()<=A::SummaryBytes&&!A::ValidateSummary(big).is_null()&&big["Points"]==4000);
    // UE4SS mods own achievements as "ue4ss/<folder>", alongside RuneSchema's "mods/<folder>".
    {const Json one=Json::parse(R"({"format":1,"achievements":[{"id":"Elf","title":"Pointed Ears","points":2,
        "conditions":[{"event":"custom","function":"/Game/JBM/Runtime/BPC_JBMEditorController.BPC_JBMEditorController_C:HelpyAchievement","where":{"Id":"Elf"}}]}]})");
        const auto ue4ss=A::Parse(one,"ue4ss/jbm","JBM");assert(ue4ss.size()==1&&ue4ss[0].Owner=="ue4ss/jbm"&&ue4ss[0].Key()=="ue4ss/jbm|Elf"&&ue4ss[0].ModName=="JBM");
        assert(A::Parse(one,"ue4ss/jbm","").front().ModName=="jbm"&&A::Parse(one,"mods/goblins","").front().ModName=="goblins");
        for(const auto* bad:{"ue4ss/","mods/","other/jbm","jbm","ue4ssjbm"}){bool threw=false;try{(void)A::Parse(one,bad,"X");}catch(const std::exception&){threw=true;}assert(threw);}
        A::Progress kept;A::Progress::Entry entry;entry.Unlocked=7;entry.Points=2;entry.Title="Pointed Ears";entry.Owner="ue4ss/jbm";entry.ModName="JBM";kept.Entries["ue4ss/jbm|Elf"]=entry;
        const auto back=A::Progress::Parse(Json::parse(kept.Document().dump()));assert(back.Points()==2&&back.Entries.at("ue4ss/jbm|Elf").Owner=="ue4ss/jbm");}
    // "report": events a mod pushes itself through RuneSchema_ReportModEventV1, matched only for that owner.
    {const Json reports=Json::parse(R"({"format":1,"achievements":[
        {"id":"tattoos","title":"Ink Collector","points":7,"conditions":[{"event":"report","name":"tattoo-designs","atLeast":53}]},
        {"id":"saves","title":"Saver","points":3,"conditions":[{"event":"report","name":"saves","count":3}]},
        {"id":"Elf","title":"Pointed Ears","points":2,"conditions":[{"event":"report","name":"award","where":{"Id":"Elf"}}]},
        {"id":"either","title":"Either Way","points":2,"match":"any","conditions":[
            {"event":"report","name":"award","where":{"Id":"Human"}},
            {"event":"custom","function":"/Game/JBM/Achievements/BPC_JBMAchievements.BPC_JBMAchievements_C:OnJBMAchievementEarned","where":{"Id":"Human"}}]}]})");
        const auto defs=A::Parse(reports,"ue4ss/jbm","JBM");
        assert(defs.size()==4&&defs[0].Conditions[0].Event=="report"&&defs[0].Conditions[0].Subject=="tattoo-designs"&&defs[0].Conditions[0].AtLeast==53);
        assert(A::WatchList(defs)==Json::array({"/Game/JBM/Achievements/BPC_JBMAchievements.BPC_JBMAchievements_C:OnJBMAchievementEarned"})); // Reports need no watched function.
        const auto report=[](const char* owner,const char* name,double amount,double value,Json parameters=Json::object()){
            Json event{{"Kind","report"},{"Owner",owner},{"Subject",name},{"Amount",amount},{"Value",value},{"Time",1800000000}};
            if(!parameters.empty())event["Parameters"]=parameters;return event;};
        A::Progress p;
        // atLeast follows the reported value, so Fraction is a real progress bar.
        assert(p.Apply(defs,report("ue4ss/jbm","tattoo-designs",1,31)).empty()&&std::abs(p.Fraction(defs[0])-31.0/53.0)<1e-12);
        assert(p.Apply(defs,report("ue4ss/jbm","tattoo-designs",1,12)).empty()&&std::abs(p.Fraction(defs[0])-31.0/53.0)<1e-12); // A lower value never takes progress back.
        // Owner check: another mod (or a RuneSchema mod with the same folder name) never matches.
        for(const auto* other:{"ue4ss/goblins","mods/jbm","","UE4SS/JBM"})assert(p.Apply(defs,report(other,"tattoo-designs",1,999)).empty());
        {auto ownerless=report("ue4ss/jbm","tattoo-designs",1,999);ownerless.erase("Owner");assert(p.Apply(defs,ownerless).empty());}
        {auto typed=report("ue4ss/jbm","tattoo-designs",1,999);typed["Owner"]=7;assert(p.Apply(defs,typed).empty());}
        assert(std::abs(p.Fraction(defs[0])-31.0/53.0)<1e-12);
        const auto done=p.Apply(defs,report("ue4ss/jbm","tattoo-designs",1,53));assert(done.size()==1&&done[0].Key=="ue4ss/jbm|tattoos"&&p.Fraction(defs[0])==1);
        // count adds up Amount; a different name never counts.
        assert(p.Apply(defs,report("ue4ss/jbm","saves",2,0)).empty()&&std::abs(p.Fraction(defs[1])-2.0/3.0)<1e-12);
        assert(p.Apply(defs,report("ue4ss/jbm","save",5,0)).empty()&&p.Apply(defs,report("ue4ss/jbm","saves",1,0)).size()==1);
        // where matches the reported parameters.
        assert(p.Apply(defs,report("ue4ss/jbm","award",1,0,{{"Id","Dwarf"}})).empty()&&p.Apply(defs,report("ue4ss/jbm","award",1,0)).empty());
        assert(p.Apply(defs,report("ue4ss/jbm","award",1,0,{{"Id","Elf"}})).size()==1);
        // Non-finite or non-positive inputs are ignored, as for every event.
        assert(p.Apply(defs,report("ue4ss/jbm","award",0,0,{{"Id","Human"}})).empty()&&p.Apply(defs,report("ue4ss/jbm","award",1,std::nan(""),{{"Id","Human"}})).empty());
        // A custom event still works beside a report condition, and carries no owner.
        assert(p.Apply(defs,{{"Kind","custom"},{"Subject","/Game/JBM/Achievements/BPC_JBMAchievements.BPC_JBMAchievements_C:OnJBMAchievementEarned"},{"Parameters",{{"Id","Human"}}}}).size()==1);
        assert(p.Points()==14&&A::Progress::Parse(Json::parse(p.Document().dump())).Points()==14);
        // Parse rules for report.
        const auto bad=[](Json condition){Json doc{{"format",1},{"achievements",{{{"id","x"},{"title","X"},{"conditions",{condition}}}}}};
            try{(void)A::Parse(doc,"ue4ss/jbm","JBM");}catch(const std::exception&){return true;}return false;};
        assert(bad({{"event","report"}})&&bad({{"event","report"},{"name",""}})&&bad({{"event","report"},{"name","has space"}})&&bad({{"event","report"},{"name",std::string(65,'a')}}));
        assert(bad({{"event","report"},{"name","a"},{"subject","/Game/X"}})&&bad({{"event","report"},{"name","a"},{"function","/Game/X.X_C:F"}})&&bad({{"event","report"},{"name",5}}));
        assert(!bad({{"event","report"},{"name","a.b_c-1"},{"atLeast",1e12}})&&bad({{"event","report"},{"name","a"},{"atLeast",-1}}));
        assert(bad({{"event","kill"},{"atLeast",5}})&&bad({{"event","custom"},{"function","/Game/X.X_C:F"},{"atLeast",5}})&&!bad({{"event","skill-xp"},{"atLeast",5}}));}
    std::cout<<"Mod achievements parse, progress, persist and summarize (RuneSchema and UE4SS owners, report events)\n";
}
