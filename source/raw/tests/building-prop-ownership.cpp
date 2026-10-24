#include "Core/BuildingPropOwnership.h"
#include <cassert>
#include <cstring>
using namespace PS::BuildingPropOwnership;
template<class F>bool Rejects(F&& f){try{f();return false;}catch(...){return true;}}
int main(){
    const Record furnace{"Dye_Quests","null_silver_furnace","/Game/Vanilla/Smelter.Smelter_C","/Game/Vanilla/BUILDPIECE_ProcessingStation_Smelter.BUILDPIECE_ProcessingStation_Smelter","BuildingPieceData"};
    const Record other{"OtherMod","station","/Game/Mod/Station.Station_C","/Game/Mod/Data.Data","BuildingData"};
    const auto first=Remember({}, {furnace,other},true);
    assert(Decode(Encode(first))==first);
    const auto disabled=Remember(first,{other},true);assert(disabled==first);
    const auto removed=Remember(disabled,{},true);assert(removed==first);
    assert(Remember(removed,{furnace,other},true)==first);
    auto changed=furnace;changed.Building="/Game/Vanilla/Other.Other";
    const auto generations=Remember(first,{changed},true);assert(generations.size()==3);
    assert(Rejects([&]{Remember(first,{},false);}));
    assert(Rejects([&]{Encode({furnace,furnace});}));
    auto bad=furnace;bad.Id="a|actor|b";assert(Rejects([&]{Encode({bad});}));
    auto malformed=Encode(first);malformed["Version"]=2;assert(Rejects([&]{Decode(malformed);}));
    malformed=Encode(first);malformed["Records"][0]["Binding"]="PersistenceID";assert(Rejects([&]{Decode(malformed);}));
    const Proof proof{true,true,true,true,true,true};
    assert(Decide(proof,true,true,true,true)==Action::Keep);
    assert(Decide(proof,false,true,true,true)==Action::Retire);
    assert(Decide(proof,true,false,false,false)==Action::Retire);
    assert(Decide(proof,true,true,false,true)==Action::Retire);
    assert(Decide(proof,true,true,true,false)==Action::Retire);
    for(int i=0;i<6;++i){auto incomplete=proof;switch(i){case 0:incomplete.Authority=false;break;case 1:incomplete.RegistryReady=false;break;case 2:incomplete.SameWorld=false;break;case 3:incomplete.ExactClass=false;break;case 4:incomplete.ExactBinding=false;break;case 5:incomplete.UniqueIdentity=false;break;}
        assert(Decide(incomplete,false,false,false,false)==Action::Preserve);
    }
    // Stable GUID bytes must remain compatible with pre-ownership runtime builds.
    for(const auto& record:first){uint32_t old[]={2166136261u,2166136261u^0x9E3779B9u,2166136261u^0x85EBCA6Bu,2166136261u^0xC2B2AE35u};
        for(unsigned char byte:record.Identity())for(auto& word:old)word=(word^byte)*16777619u;
        const auto legacy=ActorGuid(record.Identity(),true);assert(std::memcmp(old,legacy.data(),16)==0);
        old[0]=0x48435352;const auto current=ActorGuid(record.Identity());assert(std::memcmp(old,current.data(),16)==0);
    }
}
