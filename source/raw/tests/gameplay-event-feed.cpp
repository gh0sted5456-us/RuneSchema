#include "Core/GameplayEventFeed.h"
#include <cassert>
#include <limits>
#include <iostream>
// Gameplay events are normalized, bounded and read with a cursor.
using namespace PS;using Json=nlohmann::json;namespace F=GameplayEventFeed;
int main(){
    // Normalization of native calls from captured parameters.
    auto craft=F::Normalize(F::CraftFunction,{{"Recipe","/Game/Mods/Dye/DA_Recipe_RedDye.DA_Recipe_RedDye"},{"Count",3}});
    assert(craft&&craft->Kind=="craft"&&craft->Subject=="/Game/Mods/Dye/DA_Recipe_RedDye.DA_Recipe_RedDye"&&craft->Amount==3);
    auto unlocked=F::Normalize(F::RecipesUnlockedFunction,{{"Recipes",{{"Count",2}}}});assert(unlocked&&unlocked->Kind=="recipe-unlocked"&&unlocked->Amount==2);
    assert(!F::Normalize(F::BuildingsUnlockedFunction,{{"Buildings",{{"Count",0}}}}));
    auto skill=F::Normalize(F::SkillXPFunction,{{"SkillData","/Game/Skills/DA_Mining.DA_Mining"},{"PreviousXP",10.0},{"CurrentXP",25.5}});
    assert(skill&&skill->Kind=="skill-xp"&&skill->Amount==15.5&&skill->Value==25.5&&skill->Subject=="/Game/Skills/DA_Mining.DA_Mining");
    assert(!F::Normalize(F::SkillXPFunction,{{"PreviousXP",10.0},{"CurrentXP",5.0}}));         // XP loss is not progress.
    auto quest=F::Normalize(F::QuestUpdatedFunction,{{"OldQuest",{{"Quest","/Game/Q/Old.Old"}}},{"UpdatedQuest",{{"$x",1},{"Quest","/Game/Q/DA_Quest_Wolves.DA_Quest_Wolves"},{"bComplete",true}}}});
    assert(quest&&quest->Subject=="/Game/Q/DA_Quest_Wolves.DA_Quest_Wolves"&&quest->Parameters["UpdatedQuest"]["bComplete"]==true);
    assert(F::Normalize(F::RespawnFunction,Json::object())->Kind=="death");
    assert(!F::Normalize("/Game/Mods/X/BP_Boss.BP_Boss_C:OnDefeated",Json::object()));          // Not watched.
    std::set<std::string,std::less<>> watched{"/Game/Mods/X/BP_Boss.BP_Boss_C:OnDefeated"};
    auto custom=F::Normalize("/Game/Mods/X/BP_Boss.BP_Boss_C:OnDefeated",Json::object(),watched);assert(custom&&custom->Kind=="custom"&&custom->Subject=="/Game/Mods/X/BP_Boss.BP_Boss_C:OnDefeated");
    auto bad=F::Normalize(F::BuildFunction,{{"Building","not a path"}});assert(bad&&bad->Subject.empty());
    auto big=F::Normalize(F::UseItemFunction,{{"Blob",std::string(5000,'x')}});assert(big&&big->Parameters.contains("$truncated"));
    assert(F::Kill("/Game/Mods/Goblins/BP_GoblinChief.BP_GoblinChief_C").Subject=="/Game/Mods/Goblins/BP_GoblinChief.BP_GoblinChief_C");
    // Cursor reads.
    F::Feed feed(42);
    for(int i=0;i<5;++i)feed.Push(F::Kill("/Game/A.A_C"));
    {F::Event bad;bad.Kind="not-a-kind";feed.Push(bad);bad.Kind="kill";bad.Amount=-1;feed.Push(bad);}                                     // Rejected.
    auto first=feed.Since(0,3);assert(first["Session"]=="42"&&first["Events"].size()==3&&first["Cursor"]==3&&first["Dropped"]==false);
    auto rest=feed.Since(first["Cursor"]);assert(rest["Events"].size()==2&&rest["Cursor"]==5);
    assert(feed.Since(5)["Events"].empty()&&feed.Since(5)["Cursor"]==5&&feed.Since(99)["Cursor"]==5);
    for(size_t i=0;i<F::Capacity+10;++i)feed.Push(F::Kill("/Game/A.A_C"));
    assert(feed.Since(5)["Dropped"]==true&&feed.Since(0)["Dropped"]==false);                      // A fresh reader is never "dropped".
    // Watch list via the service.
    auto reply=F::Service({{"Cursor",feed.Since(UINT64_MAX/2)["Cursor"]},{"Watch",{"/Game/M/BP.BP_C:OnWin","bad",F::CraftFunction,"/Game/M/BP.BP_C"}}},feed);
    assert(reply["Watched"]==Json::array({"/Game/M/BP.BP_C:OnWin"})&&reply["Watching"]==1&&reply["Events"].empty());
    const auto revision=feed.WatchRevision();F::Service({{"Watch",{"/Game/M/BP.BP_C:OnWin"}}},feed);assert(feed.WatchRevision()==revision);
    bool threw=false;try{F::Service({{"Cursor","x"}},feed);}catch(...){threw=true;}assert(threw);
    // Mod-reported events (RuneSchema_ReportModEventV1 -> ReportV1).
    {F::Feed reports(7);const auto last=[&]{const auto events=reports.Since(0)["Events"];return events.empty()?Json():events.back();};
        assert(F::ReportV1(reports,"ue4ss/JBM","tattoo-designs",1,31,nullptr)==F::ReportOk);
        auto event=last();assert(event["Kind"]=="report"&&event["Owner"]=="ue4ss/jbm"&&event["Subject"]=="tattoo-designs"&&event["Amount"]==1&&event["Value"]==31&&!event.contains("Parameters"));
        assert(F::ReportV1(reports,"Mods/Bigger Inventory","award",2,0,R"({"Id":"Elf","Nested":{"Ok":true}})")==F::ReportOk);
        event=last();assert(event["Owner"]=="mods/bigger inventory"&&event["Amount"]==2&&event["Parameters"]["Id"]=="Elf"&&event["Parameters"]["Nested"]["Ok"]==true);
        assert(F::ReportV1(reports,"mods/x","n",1,0,"")==F::ReportOk&&!last().contains("Parameters")); // Empty text means no parameters.
        const auto count=[&]{return reports.Since(0)["Events"].size();};const auto before=count();
        const auto rejected=[&](const char* owner,const char* name,double amount,double value,const char* parameters){return F::ReportV1(reports,owner,name,amount,value,parameters)==F::ReportInvalidArgument;};
        // Owners: only "mods/<folder>" or "ue4ss/<folder>", one folder, visible text, at most 160 characters.
        for(const auto* owner:{"","jbm","mods/","ue4ss/","plugins/x","mods/a/b","mods/a\\b","mods/..","mods/.","mods/ x","mods/x ","mods/c:x","mods/\x01"})assert(rejected(owner,"n",1,0,nullptr));
        assert(rejected(("mods/"+std::string(156,'a')).c_str(),"n",1,0,nullptr)&&F::ReportV1(reports,("mods/"+std::string(155,'a')).c_str(),"n",1,0,nullptr)==F::ReportOk);
        // Names: the achievement id alphabet, 1-64 characters.
        for(const auto* name:{"","has space","a/b","a:b","a*"})assert(rejected("mods/x",name,1,0,nullptr));
        assert(rejected("mods/x",std::string(65,'n').c_str(),1,0,nullptr)&&!rejected("mods/x",std::string(64,'n').c_str(),1,0,nullptr));
        // Amount above 0 and finite (at most 1e9); value finite. Non-finite values are dropped.
        const double inf=std::numeric_limits<double>::infinity(),nan=std::numeric_limits<double>::quiet_NaN();
        for(const double amount:{0.0,-1.0,nan,inf,2e9})assert(rejected("mods/x","n",amount,0,nullptr));
        for(const double value:{nan,inf,-inf})assert(rejected("mods/x","n",1,value,nullptr));
        assert(!rejected("mods/x","n",1,-5,nullptr)); // A negative value is a valid reading.
        // Parameters: a JSON object; larger than 1 KiB parsed becomes {"$truncated":true}; more than 4096 bytes of text is refused.
        for(const auto* parameters:{"[1,2]","\"text\"","{bad json","7","null"})assert(rejected("mods/x","n",1,0,parameters));
        assert(!rejected("mods/x","n",1,0,("{\"Blob\":\""+std::string(2000,'x')+"\"}").c_str())&&(last()["Parameters"]==Json{{"$truncated",true}}));
        assert(rejected("mods/x","n",1,0,("{\"Blob\":\""+std::string(F::MaximumReportInput,'x')+"\"}").c_str()));
        // Null pointers never crash.
        assert(rejected(nullptr,"n",1,0,nullptr)&&rejected("mods/x",nullptr,1,0,nullptr));
        assert(count()==before+4); // Exactly the four accepted above were queued.
        // The service passes Owner through to Helpy.
        assert(F::Service({{"Cursor",0}},reports)["Events"][0]["Owner"]=="ue4ss/jbm");
        // Other kinds still carry no owner.
        F::Feed plain(8);plain.Push(F::Kill("/Game/A.A_C"));assert(!plain.Since(0)["Events"][0].contains("Owner"));}
    std::cout<<"Gameplay events are normalized, bounded and read with a cursor; mod reports are validated\n";
}
