#pragma once
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/String.hpp"
#include <vector>
#include <string>
#include <stdexcept>

namespace PS::RecipeAssetCatalog {
// Metadata query only, on the game thread. Use the LIVE reflected parameters,
// not the host's legacy GetAssetsByClass(FName,...) binary frame. Subclass search
// includes unloaded Blueprint-derived RecipeData without loading those assets.
inline std::vector<std::string> MountedPaths(){
    using namespace RC::Unreal;using namespace DragonWilds;
    auto interface=UAssetRegistryHelpers::GetAssetRegistry();auto* registry=static_cast<UAssetRegistry*>(interface.ObjectPointer);
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/AssetRegistry.AssetRegistry:GetAssetsByClass"),false);
    if(!registry||!function)throw std::runtime_error("Recipe subclass metadata function is unavailable");
    ReflectedContract::Frame(function);
    struct Frame {
        std::vector<uint8_t> Bytes;std::vector<FProperty*> Fields;
        ~Frame(){for(auto it=Fields.rbegin();it!=Fields.rend();++it)(*it)->DestroyValue_InContainer(Bytes.data());}
    } frame;
    frame.Bytes.resize(function->GetParmsSize());
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)){
        if(!ReflectedContract::Fits(field,int(frame.Bytes.size())))throw std::runtime_error("Recipe metadata parameter extent changed");
        field->InitializeValue_InContainer(frame.Bytes.data());frame.Fields.push_back(field);
    }
    const auto property=[&](const TCHAR* name)->FProperty*{for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->GetName()==name)return field;return nullptr;};
    auto* classPath=property(TEXT("ClassPathName"));if(!classPath)classPath=property(TEXT("ClassName"));
    auto* subclasses=CastField<FBoolProperty>(property(TEXT("bSearchSubClasses")));
    auto* output=CastField<FArrayProperty>(property(TEXT("OutAssetData")));
    auto* returned=CastField<FBoolProperty>(function->GetReturnProperty());
    if(!classPath||!ReflectedContract::Input(classPath,function)||!subclasses||!ReflectedContract::Input(subclasses,function)
        ||!output||!ReflectedContract::Fits(output,int(frame.Bytes.size()))||!output->HasAnyPropertyFlags(CPF_OutParm)
        ||!returned||!ReflectedContract::Output(returned,function))throw std::runtime_error("Recipe metadata signature changed");
    if(auto* structure=CastField<FStructProperty>(classPath)){
        auto* type=structure->GetStruct().Get();
        if(!type||type->GetName()!=TEXT("TopLevelAssetPath"))throw std::runtime_error("Recipe class-path type changed");
        auto* package=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PackageName")));
        auto* asset=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("AssetName")));
        if(!package||!asset||!ReflectedContract::Fits(package,structure->GetElementSize())||!ReflectedContract::Fits(asset,structure->GetElementSize()))throw std::runtime_error("Recipe class-path fields changed");
        auto* value=structure->ContainerPtrToValuePtr<void>(frame.Bytes.data());
        *package->ContainerPtrToValuePtr<FName>(value)=FName(TEXT("/Script/Dominion"),FNAME_Add);
        *asset->ContainerPtrToValuePtr<FName>(value)=FName(TEXT("RecipeData"),FNAME_Add);
    }else if(auto* name=CastField<FNameProperty>(classPath))
        *name->ContainerPtrToValuePtr<FName>(frame.Bytes.data())=FName(TEXT("RecipeData"),FNAME_Add);
    else throw std::runtime_error("Recipe class-path parameter is unsupported");
    auto* element=CastField<FStructProperty>(output->GetInner());auto* type=element?element->GetStruct().Get():nullptr;
    auto* package=type?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PackageName"))):nullptr;
    auto* asset=type?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("AssetName"))):nullptr;
    if(!type||type->GetName()!=TEXT("AssetData")||!package||!asset||!ReflectedContract::Fits(package,element->GetElementSize())
        ||!ReflectedContract::Fits(asset,element->GetElementSize()))throw std::runtime_error("Recipe metadata output layout changed");
    subclasses->SetPropertyValue(subclasses->ContainerPtrToValuePtr<void>(frame.Bytes.data()),true);
    registry->ProcessEvent(function,frame.Bytes.data());
    if(!returned->GetPropertyValue(returned->ContainerPtrToValuePtr<void>(frame.Bytes.data())))throw std::runtime_error("Recipe subclass metadata is not ready");
    auto* array=output->ContainerPtrToValuePtr<FScriptArray>(frame.Bytes.data());
    if(!array||array->Num()<0||array->Num()>1048576)throw std::runtime_error("Recipe metadata exceeds its safety bound");
    FScriptArrayHelper values(output,array);std::vector<std::string> paths;paths.reserve(array->Num());
    for(int i=0;i<array->Num();++i){
        auto* row=values.GetRawPtr(i);const auto root=RC::to_string(package->ContainerPtrToValuePtr<FName>(row)->ToString());
        const auto leaf=RC::to_string(asset->ContainerPtrToValuePtr<FName>(row)->ToString());
        if(!root.starts_with('/')||root.size()>2048||leaf.empty()||leaf=="None"||leaf.size()>512)throw std::runtime_error("Recipe metadata contains an invalid object path");
        paths.push_back(root+"."+leaf);
    }
    return paths;
}
}
