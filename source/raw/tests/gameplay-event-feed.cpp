#include "Core/GameplayEventFeed.h"
#include <cassert>
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
    std::cout<<"Gameplay events are normalized, bounded and read with a cursor\n";
}
