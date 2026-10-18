#include "Loader/QuestDefinitionIdentity.h"
#include <map>
#include <string>
#include <cassert>
using namespace DragonWilds::Quests::DefinitionIdentity;
int main(){
    const Json original={{"Id","q"},{"Title","Old"},{"Description","Old"},{"Objective",{{"Id","o"},{"Text","Old"},{"Count",8}}},
        {"Reward",{{"Count",1},{"Item","/Game/Reward.Reward"}}}};
    std::map<std::string,int> vars;
    const auto read=[&](const char* key){return vars[key];};
    const auto pair=[&](uint64_t hash,const char* lo,const char* hi){vars[lo]=std::bit_cast<int32_t>(uint32_t(hash));vars[hi]=std::bit_cast<int32_t>(uint32_t(hash>>32));};
    pair(Hash(original),"RuneSchema.DefLo","RuneSchema.DefHi");
    assert(Compare(original,read)==Match::Exact);
    for(unsigned mask=1;mask<8;++mask){auto update=original;if(mask&1)update["Region"]="Brynmoor";if(mask&2)update["CompletedDescription"]="Done";if(mask&4)update["Icon"]="/Game/UI/T_Icon.T_Icon";
        assert(Compare(update,read)==Match::Presentation);
        update["Reward"]["Count"]=2;assert(Compare(update,read)==Match::Changed);
    }
    vars["RuneSchema.LogicVersion"]=1;pair(Hash(Progress(original)),"RuneSchema.LogicLo","RuneSchema.LogicHi");
    auto cosmetic=original;cosmetic["Title"]="New";cosmetic["Description"]="New";cosmetic["Region"]="FutureRegion";cosmetic["Objective"]["Text"]="New";
    assert(Compare(cosmetic,read)==Match::Presentation);
    cosmetic["Icon"]="/Game/UI/T_Icon.T_Icon";assert(Compare(cosmetic,read)==Match::Presentation);
    cosmetic["Icon"]="/Game/UI/T_Other.T_Other";assert(Compare(cosmetic,read)==Match::Presentation);
    auto mechanics=cosmetic;mechanics["Objective"]["Count"]=9;assert(Compare(mechanics,read)==Match::Changed);
    mechanics=cosmetic;mechanics["Objective"]["Id"]="new-id";assert(Compare(mechanics,read)==Match::Changed);
    mechanics=cosmetic;mechanics["Category"]="Task";assert(Compare(mechanics,read)==Match::Changed);
    auto staged=original;staged.erase("Objective");staged["Stages"]=Json::array({{{"Id","stage"},{"Objectives",Json::array({original.at("Objective")})}}});
    cosmetic=staged;cosmetic["Stages"][0]["Objectives"][0]["Hint"]="New hint";assert(Progress(cosmetic)==Progress(staged));
    cosmetic["Stages"][0]["Objectives"][0]["Count"]=9;assert(Progress(cosmetic)!=Progress(staged));
}
