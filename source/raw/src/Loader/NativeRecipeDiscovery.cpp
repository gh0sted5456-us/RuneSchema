#include "Loader/NativeRecipeDiscovery.h"
#include "Loader/RegistryPatchPlan.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include <cstring>
#include <map>
#include <memory>

namespace DragonWilds::NativeRecipeDiscovery {
using namespace RC::Unreal;
using Json=nlohmann::json;
namespace {
UDataTable* ProgressionTable(UClass* progressClass) {
    if(!progressClass)throw std::runtime_error("Native progress component class is unavailable");
    TArray<UObject*> components;UECustom::UObjectGlobals::GetObjectsOfClass(progressClass,components,true);
    std::set<RC::StringType> paths;
    for(auto* component:components){
        if(!component||component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        auto* field=CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),TEXT("ProgressionTable")));
        if(!field||!ReflectedContract::Fits(field,component->GetClassPrivate()->GetPropertiesSize())
            ||field->GetElementSize()!=sizeof(UECustom::FSoftObjectPtr))continue;
        const auto* value=static_cast<UECustom::FSoftObjectPtr*>(field->ContainerPtrToValuePtr<void>(component));
        const auto package=value->ObjectID.AssetPath.GetPackageName(),asset=value->ObjectID.AssetPath.GetAssetName();
        if(package!=NAME_None&&asset!=NAME_None)paths.insert(package.ToString()+TEXT(".")+asset.ToString());
    }
    // The normal native/BP defaults may not be instantiated at GameInstanceInit.
    // This is a stable asset path, not an executable address or version gate.
    if(paths.empty())paths.insert(TEXT("/Game/Gameplay/Progress/DT_Composite_Progression.DT_Composite_Progression"));
    if(paths.size()!=1)throw std::runtime_error("Multiple active progression tables; Discovery override was not applied");
    auto* object=ActorHelper::ResolveObject(*paths.begin());
    auto* table=object&&object->IsA<UDataTable>()?static_cast<UDataTable*>(object):nullptr;
    if(!table)throw std::runtime_error("The active native progression table could not be loaded");
    return table;
}
FProperty* Field(UScriptStruct* row,const TCHAR* name) {
    auto* field=PropertyHelper::GetPropertyByName(row,name);
    if(!ReflectedContract::Fits(field,row->GetStructureSize()))throw std::runtime_error("Native progression row field/layout changed");
    return field;
}
std::vector<UObject*> Outputs(FArrayProperty* field,void* row) {
    const auto* array=static_cast<FScriptArray*>(field->ContainerPtrToValuePtr<void>(row));
    if(array->Num()<0||array->Num()>100000||(array->Num()&&!array->GetData()))throw std::runtime_error("Native progression recipe array is invalid");
    auto* element=CastField<FObjectPropertyBase>(field->GetInner());
    if(!element||element->GetArrayDim()!=1||element->GetOffset_Internal()!=0||element->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Native progression recipe reference layout changed");
    std::vector<UObject*> result;result.reserve(array->Num());
    for(int32 i=0;i<array->Num();++i)
        result.push_back(element->GetObjectPropertyValue(static_cast<const uint8*>(array->GetData())+i*element->GetElementSize()));
    return result;
}
void RetainNativeSlots(FArrayProperty* field,void* destination,void* source,const std::vector<std::size_t>& indices) {
    auto* before=field->ContainerPtrToValuePtr<FScriptArray>(source);
    UECustom::FScriptArrayHelper after(field->ContainerPtrToValuePtr<void>(destination),field);after.Empty();
    for(const auto index:indices){
        if(index>=static_cast<std::size_t>(before->Num()))throw std::runtime_error("Native recipe slot changed during preparation");
        // Add performs reflected CopySingleValue, retaining nulls and native
        // reference representations without loading or dereferencing them.
        after.Add(static_cast<uint8*>(before->GetData())+index*field->GetInner()->GetElementSize());
    }
}
struct Prepared {FName Name;std::unique_ptr<FManagedStruct> Before,After;bool Existing=false;};
bool Identical(UScriptStruct* row,void* left,void* right){
    for(auto* field:TFieldRange<FProperty>(row))if(!field->Identical(field->ContainerPtrToValuePtr<void>(left),field->ContainerPtrToValuePtr<void>(right)))return false;
    return true;
}
}
Result Apply(const std::vector<Request>& requests,UClass* progressClass,std::set<std::string>& ownedRows) {
    if(requests.empty())return {};
    if(requests.size()>100000)throw std::runtime_error("Native recipe discovery batch exceeds the safety bound");
    auto* table=ProgressionTable(progressClass);auto* row=table->GetRowStruct().Get();
    if(!row||row->GetPathName()!=TEXT("/Script/Dominion.ProgressionBundleRow"))throw std::runtime_error("Active table does not use native ProgressionBundleRow");
    if(table->GetRowMap().Num()>100000)throw std::runtime_error("Native progression table exceeds the safety bound");
    auto* recipes=CastField<FArrayProperty>(Field(row,TEXT("UnlockedRecipes")));
    auto* element=recipes?CastField<FObjectPropertyBase>(recipes->GetInner()):nullptr;
    auto* query=CastField<FStructProperty>(Field(row,TEXT("UnlockQuery")));
    auto* manual=CastField<FBoolProperty>(Field(row,TEXT("bManuallyUnlocked")));
    auto* milestone=CastField<FBoolProperty>(Field(row,TEXT("bMilestoneUnlock")));
    auto* description=CastField<FStrProperty>(Field(row,TEXT("UnlockQueryString")));
    if(!element||element->GetElementSize()!=sizeof(UObject*)||!element->GetPropertyClass().Get()
        ||!query||!query->GetStruct()||query->GetStruct()->GetName()!=TEXT("UnlockMatchQuery")||!manual||!milestone||!description)
        throw std::runtime_error("Native progression query/recipe layout is unavailable");
    std::map<std::string,const Request*> unique;
    for(const auto& request:requests){
        if(!request.Recipe||!request.Recipe->IsA(element->GetPropertyClass().Get()))throw std::runtime_error("Discovery target is not native RecipeData");
        const auto path=RC::to_string(request.Recipe->GetPathName());
        const auto [found,inserted]=unique.emplace(path,&request);
        if(!inserted&&found->second->Rule.Query!=request.Rule.Query)throw std::runtime_error("Conflicting recipe Discovery policies for "+path);
    }
    std::set<UObject*> replaced;for(const auto& [path,request]:unique)replaced.insert(request->Recipe);
    std::vector<Prepared> prepared;Result result;
    // Prepare replacements first. No live row is changed until every query,
    // prerequisite and target has passed reflected conversion/type checks.
    std::size_t inspected=0;
    for(const auto& [name,source]:table->GetRowMap())try{
        const auto before=Outputs(recipes,source);
        if((inspected+=before.size())>1000000)throw std::runtime_error("Native progression inspection exceeds one million recipe references");
        const auto retained=PS::RecipeDiscovery::RetainOtherRecipeIndices(before,replaced);
        if(before.size()==retained.size())continue;
        std::vector<UObject*> after;for(const auto index:retained)after.push_back(before[index]);
        Prepared change{name,std::make_unique<FManagedStruct>(row),std::make_unique<FManagedStruct>(row),true};
        row->CopyScriptStruct(change.Before->GetData(),source);row->CopyScriptStruct(change.After->GetData(),source);
        RetainNativeSlots(recipes,change.After->GetData(),source,retained);
        if(Outputs(recipes,change.After->GetData())!=after)throw std::runtime_error("Discovery recipe detachment did not verify");
        prepared.push_back(std::move(change));result.Detached+=before.size()-after.size();
    }catch(const std::exception& error){throw std::runtime_error("Progression table '"+RC::to_string(table->GetPathName())+"', row '"+RC::to_string(name.ToString())+"', UnlockedRecipes: "+error.what());}
    std::set<std::string> newOwned;
    for(const auto& [path,request]:unique){
        const auto name=RegistryPatch::StableRowName("RS_RECIPE_DISCOVERY",path);
        const auto key=RC::to_string(table->GetPathName())+":"+name;
        const FName rowName(RC::to_generic_string(name),FNAME_Add);
        if(table->FindRowUnchecked(rowName))throw std::runtime_error("Discovery row already exists; restart required or ownership collision: "+name);
        Prepared addition{rowName,{},std::make_unique<FManagedStruct>(row),false};
        PropertyHelper::CopyJsonValueToContainer(addition.After->GetData(),query,request->Rule.Query);
        PropertyHelper::CopyJsonValueToContainer(addition.After->GetData(),manual,false);
        PropertyHelper::CopyJsonValueToContainer(addition.After->GetData(),milestone,false);
        PropertyHelper::CopyJsonValueToContainer(addition.After->GetData(),description,request->Rule.Description);
        PropertyHelper::CopyJsonValueToContainer(addition.After->GetData(),recipes,Json::array({path}));
        if(Outputs(recipes,addition.After->GetData())!=std::vector<UObject*>{request->Recipe})throw std::runtime_error("Discovery output recipe did not verify: "+path);
        prepared.push_back(std::move(addition));newOwned.insert(key);++result.Added;
    }
    std::size_t committed=0;
    try{
        for(auto& change:prepared){
            if(change.Existing)row->CopyScriptStruct(table->FindRowUnchecked(change.Name),change.After->GetData());
            else table->AddRow(change.Name,*reinterpret_cast<FTableRowBase*>(change.After->GetData()));
            ++committed;
            auto* actual=table->FindRowUnchecked(change.Name);
            if(!actual||!Identical(row,actual,change.After->GetData()))throw std::runtime_error("Native discovery commit failed readback");
        }
    }catch(...){
        while(committed){auto& change=prepared[--committed];
            if(change.Existing)row->CopyScriptStruct(table->FindRowUnchecked(change.Name),change.Before->GetData());else table->RemoveRow(change.Name);}
        throw;
    }
    table->SetRootSet();ownedRows.insert(newOwned.begin(),newOwned.end());
    return result;
}
}
