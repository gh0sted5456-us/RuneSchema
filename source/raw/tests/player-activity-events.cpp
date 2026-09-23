#include "Loader/PlayerActivityEvents.h"
#include "Loader/VisualEffectLifetime.h"
#include "Generator/PlayerTraceOptions.h"
#include <cassert>
using nlohmann::json;
int main() {
    const auto timed=DragonWilds::ParseVisualEffectLifetime(
        {{"Type","Ghost"},{"Trigger","Respawn"},{"DurationSeconds",12}},"Load",{"Load","Respawn"});
    assert(timed.Trigger=="Respawn" && timed.DurationSeconds==12 && timed.IsFinite());
    const auto permanent=DragonWilds::ParseVisualEffectLifetime(
        {{"Type","Ghost"},{"DurationSeconds","INFINITE"}},"Load",{"Load","Respawn"});
    assert(permanent.Trigger=="Load" && !permanent.IsFinite());
    for(const auto& value : {json{{"DurationSeconds",0}},json{{"DurationSeconds",-1}},
        json{{"DurationSeconds",301}},json{{"DurationSeconds",true}},
        json{{"DurationSeconds","12"}},json{{"Trigger","Equip"}}}) {
        bool rejected=false;
        try{(void)DragonWilds::ParseVisualEffectLifetime(value,"Load",{"Load","Respawn"});}
        catch(const std::exception&){rejected=true;}
        assert(rejected);
    }
    const json xpMapping={{"/Game/Skills/Test.Test", "Mining"}};
    DragonWilds::ValidateSkillXP(xpMapping, {{"Mining",json::object()}});
    auto xp=json{{"SkillData","/Game/Skills/Test.Test"},{"PreviousXP",34},{"CurrentXP",47}};
    assert(DragonWilds::MatchSkillXP(xpMapping,xp)=="Mining");
    xp["CurrentXP"]=34; assert(DragonWilds::MatchSkillXP(xpMapping,xp).empty());
    xp["CurrentXP"]=0; assert(DragonWilds::MatchSkillXP(xpMapping,xp).empty());
    xp["CurrentXP"]=true; assert(DragonWilds::MatchSkillXP(xpMapping,xp).empty());
    xp["CurrentXP"]=47; xp["PreviousXP"]=-1; assert(DragonWilds::MatchSkillXP(xpMapping,xp).empty());
    xp["PreviousXP"]=34; xp["SkillData"]="/Game/Unknown.Unknown";
    assert(DragonWilds::MatchSkillXP(xpMapping,xp).empty());
    bool badMapping=false;
    try {DragonWilds::ValidateSkillXP({{"/Game/Skills/Test.Test","Unknown"}},{{"Mining",json::object()}});}
    catch(const std::exception&) {badMapping=true;}
    assert(badMapping);
    const auto events=json::parse(R"([{"Function":"/Script/Dominion.PlayerMagicComponent:Multicast_SendPayloadForSpellCasting","State":"Fishing","Parameters":[{"Path":["SpellNetId","NetId"],"Equals":123}]}])");
    const json states={{"Fishing",json::object()}};
    DragonWilds::ValidateActivityEvents(events,states);
    auto sixtyFourEvents=json::array();
    for(int i=0;i<64;++i) sixtyFourEvents.push_back(events[0]);
    DragonWilds::ValidateActivityEvents(sixtyFourEvents,states);
    sixtyFourEvents.push_back(events[0]);
    bool tooManyEvents=false;
    try {DragonWilds::ValidateActivityEvents(sixtyFourEvents,states);}catch(...) {tooManyEvents=true;}
    assert(tooManyEvents);
    const auto function=events[0]["Function"].get<std::string>();
    assert(DragonWilds::MatchActivityEvent(events,function,{{"SpellNetId",{{"NetId",123}}}})=="Fishing");
    assert(DragonWilds::MatchActivityEvent(events,function,{{"SpellNetId",{{"NetId",124}}}}).empty());
    assert(DragonWilds::MatchActivityEvent(events,function,{{"SpellNetId",{{"NetId",true}}}}).empty());
    assert(DragonWilds::MatchActivityEvent(events,function,{{"SpellNetId",{{"omitted","unsupported"}}}}).empty());
    assert(DragonWilds::MatchActivityEvent(events,"other",{{"SpellNetId",{{"NetId",123}}}}).empty());
    const auto skill=json::parse(R"([{"Function":"/Script/Dominion.SkillComponent:AwardSkillXP","State":"Artisan","Action":"Activate","Parameters":[{"Path":["SkillData"],"Equals":"/Game/Gameplay/Character/Player/Skills/SKILL_Artisan.SKILL_Artisan"}]}])");
    const json skillStates={{"Artisan",json::object()}};
    DragonWilds::ValidateActivityEvents(skill,skillStates);
    const auto skillMatch=DragonWilds::MatchActivityEventRule(skill,
        "/Script/Dominion.SkillComponent:AwardSkillXP",
        {{"SkillData","/Game/Gameplay/Character/Player/Skills/SKILL_Artisan.SKILL_Artisan"}});
    assert(skillMatch.State=="Artisan" && skillMatch.Action=="Activate");
    const auto unconditional=json::parse(R"([{"Function":"/Script/Dominion.SkillComponent:FinishAction","State":"Artisan","Action":"Deactivate"}])");
    DragonWilds::ValidateActivityEvents(unconditional,skillStates);
    assert(DragonWilds::MatchActivityEventRule(unconditional,
        "/Script/Dominion.SkillComponent:FinishAction",json::object()).Action=="Deactivate");
    const auto blueprint=json::parse(R"([{"Function":"/Game/Gameplay/Character/Components/BP_Components_Health.BP_Components_Health_C:OnHealEvent","State":"HealthGain","Action":"Pulse"}])");
    const json blueprintStates={{"HealthGain",json::object()}};
    DragonWilds::ValidateActivityEvents(blueprint,blueprintStates);
    assert(DragonWilds::ActivityFunctionName(blueprint[0]["Function"].get<std::string>())=="OnHealEvent");
    assert(DragonWilds::MatchActivityEventRule(blueprint,
        "/Game/Gameplay/Character/Components/BP_Components_Health.BP_Components_Health_C:OnHealEvent",
        json::object()).State=="HealthGain");
    const auto emote=json::parse(R"([{"Function":"/Script/Dominion.PlayerEmotesComponent:PlayEmote","State":"Celebrate","Action":"Pulse","Parameters":[{"Path":["EmoteIndex"],"Equals":1}]}])");
    const json emoteStates={{"Celebrate",json::object()}};
    DragonWilds::ValidateActivityEvents(emote,emoteStates);
    assert(DragonWilds::MatchActivityEventRule(emote,
        "/Script/Dominion.PlayerEmotesComponent:PlayEmote",{{"EmoteIndex",1}}).State=="Celebrate");
    assert(DragonWilds::MatchActivityEventRule(emote,
        "/Script/Dominion.PlayerEmotesComponent:PlayEmote",{{"EmoteIndex",2}}).State.empty());
    const auto pickup=json::parse(R"([{"Function":"/Game/Gameplay/Character/Player/BP_PlayerController.BP_PlayerController_C:OnInventoryChanged_BrokenItemFTUE","State":"LootPickup","Parameters":[{"Path":["AddedItems"],"NonEmpty":true}]}])");
    const json pickupStates={{"LootPickup",json::object()}};
    DragonWilds::ValidateActivityEvents(pickup,pickupStates);
    assert(DragonWilds::MatchActivityEvent(pickup,
        pickup[0]["Function"].get<std::string>(),{{"AddedItems",{{"Count",1}}}})=="LootPickup");
    assert(DragonWilds::MatchActivityEvent(pickup,
        pickup[0]["Function"].get<std::string>(),{{"AddedItems",{{"Count",0}}}}).empty());
    for(int i=0;i<8;++i) {
        auto bad=events;
        if(i==0)bad[0]["State"]="Dead";
        if(i==1)bad[0]["Function"]="/Game/Unsafe:Call";
        if(i==2)bad[0]["Function"]="/Script/Engine.Actor:Tick";
        if(i==3)bad[0]["Parameters"][0]["Equals"]="123";
        if(i==4)bad[0]["Parameters"][0]["Path"]=json::array();
        if(i==5)bad[0]["Parameters"]=json::array({
            events[0]["Parameters"][0],events[0]["Parameters"][0],events[0]["Parameters"][0],
            events[0]["Parameters"][0],events[0]["Parameters"][0],events[0]["Parameters"][0],
            events[0]["Parameters"][0],events[0]["Parameters"][0],events[0]["Parameters"][0]});
        if(i==6)bad[0]["Extra"]=true;
        if(i==7)bad[0]["Action"]="Execute";
        bool rejected=false;
        try {DragonWilds::ValidateActivityEvents(bad,states);}catch(...){rejected=true;}
        assert(rejected);
    }
    PS::PlayerTrace::ValidateOptions({{"CaptureParameters",true},{"Filter","SpellCasting"}});
    bool rejected=false;
    try {PS::PlayerTrace::ValidateOptions({{"CaptureParameters",true}});}catch(...){rejected=true;}
    assert(rejected);
    assert(DragonWilds::ActivityIdentifier("CustomArtisan"));
    assert(!DragonWilds::ActivityIdentifier("../unsafe"));
    assert(!DragonWilds::ActivityScalarEquals(json(uint64_t(-1)),json(-1)));
    assert(DragonWilds::ActivityScalarEquals(json(uint64_t(123)),json(123)));
    assert(!DragonWilds::ActivityScalarEquals(json(123.0),json(123)));
    assert(DragonWilds::ActivityScalarEquals(json("/Game/Skill.Skill"),json("/Game/Skill.Skill")));
    assert(DragonWilds::ActivityNonEmpty(json{{"Count",2}}));
    assert(!DragonWilds::ActivityNonEmpty(json{{"Count",0}}));
}
