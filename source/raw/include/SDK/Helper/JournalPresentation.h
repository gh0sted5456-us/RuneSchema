#pragma once
#include "Generator/AssetSearch.h"
#include "Core/HelpySourceCache.h"
#include "Core/ItemSource.h"
#include "Loader/DragonWildsJournalModLoader.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/Property/FTextProperty.hpp"
namespace PS::JournalPresentation {
// Called on the game thread. Sliced object inventory, no LoadAsset, unlock,
// read/unread mutation, registry insertion or character-save serialization.
inline nlohmann::json Window(const nlohmann::json& request) {
    using namespace RC::Unreal;namespace PH=DragonWilds::PropertyHelper;
    namespace C=DragonWilds::ReflectedContract;using Json=nlohmann::json;
    const auto count=FUObjectArray::GetNumElements();
    if(count<0||count>4'000'000)throw std::runtime_error("Journal object inventory exceeds its bound");
    auto* base=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.JournalEntryData"));
    if(!base)throw std::runtime_error("Native journal classes are unavailable");
    const auto cursor=request.value("Cursor",0);if(cursor<0||cursor>count)throw std::runtime_error("Journal cursor expired");
    int at=cursor;Json rows=Json::array();const auto begin=std::chrono::steady_clock::now();
    const auto plan=HelpySourceCache::ItemPlan();const auto plugins=HelpySourceCache::NativePluginRoots();
    for(int scanned=0;at<count&&scanned<4096&&rows.size()<12&&std::chrono::steady_clock::now()-begin<std::chrono::microseconds(1500);++scanned) {
        auto* slot=FUObjectArray::IndexToObject(at++);auto* object=slot?slot->GetUObject():nullptr;
        if(!object||!slot->IsValid(false)||!object->IsA(base)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        const auto size=object->GetClassPrivate()->GetPropertiesSize();
        const auto text=[](FProperty* p,void* data,int size)->std::string {
            if(!p||!C::Fits(p,size))return {};
            if(auto* field=CastField<FTextProperty>(p);field&&C::Text(field))return RC::to_string(PH::GetTextAsString(*field->ContainerPtrToValuePtr<FText>(data)));
            if(auto* field=CastField<FStrProperty>(p);field&&field->GetElementSize()==sizeof(FString)){
                const auto& value=*field->ContainerPtrToValuePtr<FString>(data);const auto& chars=value.GetCharArray();
                if(chars.Num()>0&&chars.Num()<=65536&&chars.GetData()&&chars.GetData()[chars.Num()-1]==0)return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
            }return {};
        };
        const auto read=[&](const TCHAR* name){auto value=text(PH::GetPropertyByName(object->GetClassPrivate(),name),object,size);if(value.size()>65536)value.resize(65536);return value;};
        const auto path=RC::to_string(object->GetPathName());auto ownership=DragonWilds::DragonWildsJournalModLoader::PresentationOwnership(object);
        const auto owner=ownership.value("CreatingMod",std::string{}),patch=ownership.value("ModifiedBy",std::string{});
        auto source=ItemSource::Describe(path,plan,plugins,owner,false,!patch.empty(),patch);
        if((source.Group==ItemSource::Kind::Vanilla||source.Group==ItemSource::Kind::Expansion)&&!object->HasAnyFlags(RF_WasLoaded))source.Group=ItemSource::Kind::Unknown;
        Json pages=Json::array();auto* field=CastField<FArrayProperty>(PH::GetPropertyByName(object->GetClassPrivate(),TEXT("PageDescriptions")));
        auto* inner=field?CastField<FStructProperty>(field->GetInner()):nullptr;
        if(field&&C::Fits(field,size)&&field->GetSize()==sizeof(FScriptArray)&&inner&&inner->GetStruct()) {
            const auto stride=inner->GetElementSize(),bytes=inner->GetStruct()->GetPropertiesSize();auto* array=field->ContainerPtrToValuePtr<FScriptArray>(object);
            auto* description=PH::GetPropertyByName(inner->GetStruct().Get(),TEXT("Description"));size_t total=0;
            if(array&&array->Num()>=0&&array->Num()<=128&&(!array->Num()||array->GetData())&&stride==bytes&&bytes>0&&bytes<=4096)
                for(int i=0;i<array->Num();++i){auto value=text(description,static_cast<uint8_t*>(array->GetData())+size_t(i)*stride,bytes);total+=value.size();if(value.size()>65536||total>262144)break;pages.push_back(std::move(value));}
        }
        auto name=read(TEXT("DisplayName"));if(name.empty())name=RC::to_string(object->GetName());
        std::string kind="Journal",cls;for(auto* type=static_cast<UStruct*>(object->GetClassPrivate());type;type=type->GetSuperStruct())cls+=" "+RC::to_string(type->GetPathName());
        for(const auto* k:{"Lore","People","Place","Treasure","World","Recipe"})if(cls.find(k)!=cls.npos){kind=k;break;}
        Json row={{"Path",path},{"Name",name},{"Type",kind},{"PersistenceID",read(TEXT("PersistenceID"))},{"InternalName",read(TEXT("InternalName"))},{"Pages",pages},{"Description",read(TEXT("Description"))},{"Icon",AssetSearch::PresentationIcon(object)},{"Native",true}};
        row.update(ItemSource::Fields(source));rows.push_back(std::move(row));
    }
    return {{"Entries",rows},{"Cursor",at},{"HasMore",at<count},{"SourceKey",HelpySourceCache::Current.CatalogKey},{"Coverage","Loaded native journal assets only; not a complete vanilla manifest or character discovery state."}};
}
}
