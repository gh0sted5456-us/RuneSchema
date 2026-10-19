#pragma once
#include "SDK/Helper/EquipmentDelivery.h"
#include "Generator/AssetSearch.h"
namespace PS::PlayerLoadoutPresentation {
inline nlohmann::json Skills(RC::Unreal::UObject* controller){
    using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;namespace C=DragonWilds::ReflectedContract;
    try{auto* component=EquipmentDelivery::Owned(controller,TEXT("SkillComponent"),TEXT("/Script/Dominion.SkillComponent"));
        auto* field=CastField<FArrayProperty>(PH::GetPropertyByName(component->GetClassPrivate(),TEXT("Skills")));auto* inner=field?CastField<FStructProperty>(field->GetInner()):nullptr;
        auto* type=AH::ResolveClass(TEXT("/Script/Dominion.SkillData"));
        if(!type||!field||!C::Fits(field,component->GetClassPrivate()->GetPropertiesSize())||field->GetSize()!=sizeof(FScriptArray)||!inner||!inner->GetStruct()||inner->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.Skill"))return nullptr;
        const auto stride=inner->GetElementSize(),size=inner->GetStruct()->GetPropertiesSize();auto* skill=CastField<FObjectProperty>(PH::GetPropertyByName(inner->GetStruct().Get(),TEXT("SkillData")));
        auto* array=field->ContainerPtrToValuePtr<FScriptArray>(component);
        if(!array||array->Num()<0||array->Num()>32||(array->Num()&&!array->GetData())||stride!=size||size<1||size>512||!skill||!C::Fits(skill,size)||skill->GetPropertyClass().Get()!=type)return nullptr;
        auto rows=nlohmann::json::array();std::set<std::string> seen;
        for(int i=0;i<array->Num();++i){auto* entry=static_cast<uint8_t*>(array->GetData())+size_t(i)*stride;auto* data=*skill->ContainerPtrToValuePtr<UObject*>(entry);if(!data||!data->IsA(type))continue;
            const auto path=RC::to_string(data->GetPathName());if(!seen.insert(path).second)continue;
            EquipmentDelivery::Call level(component,TEXT("/Script/Dominion.SkillComponent:GetSkillLevel"),2);level.Object(TEXT("SkillData"),data);(void)level.Integer();level.Invoke();const auto value=level.Integer();if(value<0||value>10000)continue;
            std::string name=RC::to_string(data->GetName());if(auto* p=CastField<FTextProperty>(PH::GetPropertyByName(data->GetClassPrivate(),TEXT("Name")));p&&C::Fits(p,data->GetClassPrivate()->GetPropertiesSize())){const auto text=PH::GetTextAsString(*p->ContainerPtrToValuePtr<FText>(data));if(!text.empty()&&text.size()<=256)name=RC::to_string(text);}
            rows.push_back({{"Path",path},{"Name",name},{"Level",value},{"Icon",AssetSearch::PresentationIcon(data)}});
        }return {{"Items",std::move(rows)},{"ObservedUnixMs",PlayerVitals::Now()}};
    }catch(...){return nullptr;}
}
// Read only the actual loadout inventory, never the bag, cosmetic appearance,
// class defaults or an inferred list of possible slots. No equip/use RPCs.
inline nlohmann::json Read(RC::Unreal::UObject* controller){
    using Json=nlohmann::json;Json result={{"Reason","Native equipped inventory unavailable"}};
    try{auto* loadout=EquipmentDelivery::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));
        const auto slots=EquipmentDelivery::Slots(loadout);if(slots.size()>32)return result;
        auto rows=Json::array();for(size_t i=0;i<slots.size();++i){auto* item=slots[i];if(!item)continue;
            auto* data=EquipmentDelivery::Data(item);auto* type=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
            if(!type||!data||!data->IsA(type))continue;
            const auto path=RC::to_string(data->GetPathName());const auto meta=AssetSearch::ItemMetadata(path);
            Json row={{"Slot",i},{"Path",path},{"Name",meta.value("Name",std::string{})},{"Icon",meta.value("Icon",std::string{})}};
            try{
                // Native, non-networked integer getters verified against the
                // item instance, not base ItemData durability or a guessed %.
                EquipmentDelivery::Call current(item,TEXT("/Script/Dominion.Item:GetDurability"),1),maximum(item,TEXT("/Script/Dominion.Item:GetMaxDurability"),1);
                (void)current.Integer();(void)maximum.Integer();current.Invoke();maximum.Invoke();
                const auto value=current.Integer(),limit=maximum.Integer();
                if(value>=0&&limit>0&&limit<=1000000000&&value<=limit)row["Durability"]={{"Current",value},{"Maximum",limit},{"ObservedUnixMs",PlayerVitals::Now()}};
                else row["DurabilityReason"]="No valid positive native durability range reported";
            }catch(...){row["DurabilityReason"]="Native durability contract unavailable";}
            rows.push_back(std::move(row));
        }return {{"Items",std::move(rows)},{"ObservedUnixMs",PlayerVitals::Now()}};
    }catch(...){return result;}
}
inline nlohmann::json Cached(RC::Unreal::UObject* controller,const std::string& guid,uint64_t generation){
    using Clock=std::chrono::steady_clock;struct Entry{std::string Guid;nlohmann::json Value;Clock::time_point Next;};
    static uint64_t scope=0;static std::map<std::string,Entry> cache;if(scope!=generation){scope=generation;cache.clear();}
    if(!controller)return nullptr;const auto key=RC::to_string(controller->GetPathName());auto found=cache.find(key);const auto now=Clock::now();
    if(found!=cache.end()&&found->second.Guid==guid&&now<found->second.Next)return found->second.Value;
    if(cache.size()>=256)cache.clear();auto value=Read(controller);value["Skills"]=Skills(controller);cache[key]={guid,value,now+std::chrono::seconds(3)};return value;
}
}
