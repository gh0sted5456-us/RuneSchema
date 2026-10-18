#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/World.hpp>
#include "Utility/Logging.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Core/GroundSurface.h"

#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    namespace
    {
        bool IsValidParameter(
            FProperty* property, size_t parameterSize, size_t valueSize)
        {
            return property && property->GetOffset_Internal() >= 0
                && static_cast<size_t>(property->GetOffset_Internal()) + valueSize
                    <= parameterSize;
        }

        void SetNumericParameter(
            UFunction* function, std::vector<uint8>& parameters,
            const CharType* name, int64 value)
        {
            auto* property = function->FindProperty(FName(name, FNAME_Find));
            if(!DragonWilds::ReflectedContract::Fits(property,static_cast<int>(parameters.size())))throw std::runtime_error("line-trace numeric parameter extent changed");
            auto* address = property
                ? property->ContainerPtrToValuePtr<void>(parameters.data()) : nullptr;
            if (auto* enumProperty = CastField<FEnumProperty>(property))
            {
                auto* underlying = enumProperty->GetUnderlyingProperty();
                if (!address || !underlying)
                    throw std::runtime_error("line-trace enum parameter metadata was invalid");
                underlying->SetIntPropertyValue(address, value);
                return;
            }
            if (auto* numeric = CastField<FNumericProperty>(property))
            {
                if (!address || !numeric->IsInteger())
                    throw std::runtime_error("line-trace numeric parameter metadata was invalid");
                numeric->SetIntPropertyValue(address, value);
                return;
            }
            throw std::runtime_error("line-trace numeric parameter was unavailable");
        }

        void SetBooleanParameter(
            UFunction* function, std::vector<uint8>& parameters,
            const CharType* name, bool value)
        {
            auto* property = CastField<FBoolProperty>(
                function->FindProperty(FName(name, FNAME_Find)));
            if (!DragonWilds::ReflectedContract::Fits(property,static_cast<int>(parameters.size())))
                throw std::runtime_error("line-trace boolean parameter was unavailable");
            property->SetPropertyValue(
                property->ContainerPtrToValuePtr<void>(parameters.data()), value);
        }
    }

    RC::Unreal::UObject* UKismetSystemLibrary::LoadAsset_Blocking(UECustom::TSoftObjectPtr<UObject> Asset)
    {
        static auto Function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, TEXT("/Script/Engine.KismetSystemLibrary:LoadAsset_Blocking"));

        if (!Function)
        {
            PS::Log<LogLevel::Error>(STR("Function /Script/Engine.KismetSystemLibrary:LoadAsset_Blocking was invalid.\n"));
            return nullptr;
        }

        DragonWilds::ReflectedContract::Frame(Function);
        auto* assetProperty = CastField<FSoftObjectProperty>(Function->FindProperty(FName(TEXT("Asset"), FNAME_Find)));
        auto* returnProperty = CastField<FObjectPropertyBase>(Function->GetReturnProperty());
        if (!DragonWilds::ReflectedContract::Input(assetProperty,Function) || assetProperty->GetSize()!=sizeof(Asset)
            || !DragonWilds::ReflectedContract::Output(returnProperty,Function) || returnProperty->GetSize()!=sizeof(UObject*)
            || DragonWilds::ReflectedContract::Count(Function)!=2)
        {
            PS::Log<LogLevel::Error>(STR("LoadAsset_Blocking parameter metadata was invalid.\n"));
            return nullptr;
        }

        const auto paramsSize = Function->GetParmsSize();
        std::vector<uint8> params(paramsSize, 0);
        const auto assetOffset = assetProperty->GetOffset_Internal();
        const auto returnOffset = returnProperty->GetOffset_Internal();
        if (assetOffset + sizeof(Asset) > params.size()
            || returnOffset + sizeof(UObject*) > params.size())
        {
            PS::Log<LogLevel::Error>(STR("LoadAsset_Blocking parameter layout was out of bounds.\n"));
            return nullptr;
        }

        assetProperty->InitializeValue_InContainer(params.data());
        try {
            assetProperty->CopyCompleteValue(assetProperty->ContainerPtrToValuePtr<void>(params.data()),&Asset);
            GetDefaultObj()->ProcessEvent(Function, params.data());
            auto* result=returnProperty->GetObjectPropertyValue(returnProperty->ContainerPtrToValuePtr<void>(params.data()));
            assetProperty->DestroyValue_InContainer(params.data());
            return result;
        }catch(...) {assetProperty->DestroyValue_InContainer(params.data());throw;}
    }

    RC::Unreal::UClass* UKismetSystemLibrary::LoadClassAsset_Blocking(UECustom::TSoftClassPtr<UObject> AssetClass)
    {
        static auto Function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.KismetSystemLibrary:LoadClassAsset_Blocking"));

        if (!Function)
        {
            PS::Log<LogLevel::Error>(STR("Function /Script/Engine.KismetSystemLibrary:LoadClassAsset_Blocking was invalid.\n"));
            return nullptr;
        }

        auto* assetClassProperty = Function->FindProperty(FName(TEXT("AssetClass"), FNAME_Find));
        auto* returnProperty = Function->GetReturnProperty();
        if (!assetClassProperty || !returnProperty)
        {
            PS::Log<LogLevel::Error>(STR("LoadClassAsset_Blocking parameter metadata was invalid.\n"));
            return nullptr;
        }

        const auto paramsSize = Function->GetParmsSize();
        std::vector<uint8> params(paramsSize, 0);
        const auto assetClassOffset = assetClassProperty->GetOffset_Internal();
        const auto returnOffset = returnProperty->GetOffset_Internal();
        if (assetClassOffset + sizeof(AssetClass) > params.size()
            || returnOffset + sizeof(UClass*) > params.size())
        {
            PS::Log<LogLevel::Error>(STR("LoadClassAsset_Blocking parameter layout was out of bounds.\n"));
            return nullptr;
        }

        new (params.data() + assetClassOffset) UECustom::TSoftClassPtr<UObject>(AssetClass);
        GetDefaultObj()->ProcessEvent(Function, params.data());
        return *reinterpret_cast<UClass**>(params.data() + returnOffset);
    }

    bool UKismetSystemLibrary::LineTraceGround(
        UObject* WorldContext, const FVector& Start, const FVector& End,
        const std::vector<AActor*>& ActorsToIgnore,
        FVector& ImpactPoint, std::string& Error, UObject** HitComponent,bool TraceComplex,
        FVector* ImpactNormal,bool* StartPenetrating)
    {
        return TraceCollision(WorldContext,Start,End,ActorsToIgnore,ImpactPoint,Error,
            HitComponent,TraceComplex,ImpactNormal,StartPenetrating);
    }

    bool UKismetSystemLibrary::CapsuleBlocked(UObject* world,const FVector& centre,
        double radius,double halfHeight,const std::vector<AActor*>& ignored,std::string& error)
    {
        error.clear();
        if(!std::isfinite(radius)||!std::isfinite(halfHeight)||radius<0.1||radius>10000
            ||halfHeight<radius||halfHeight>100000||!std::isfinite(centre.X())
            ||!std::isfinite(centre.Y())||!std::isfinite(centre.Z())){
            error="Collision clearance requires a finite bounded capsule";return true;
        }
        FVector hit{};
        // A tiny real sweep also reports starting penetration; no zero-length
        // ray shortcut and no overlap/collision flags on live actors changed.
        for(bool complex:{false,true}) {
            const bool blocked=TraceCollision(world,centre,FVector(centre.X(),centre.Y(),centre.Z()+0.1),
                ignored,hit,error,nullptr,complex,nullptr,nullptr,radius,halfHeight);
            if(blocked||!error.empty())return true;
        }
        return false;
    }

    bool UKismetSystemLibrary::TraceCollision(
        UObject* WorldContext,const FVector& Start,const FVector& End,
        const std::vector<AActor*>& ActorsToIgnore,FVector& ImpactPoint,std::string& Error,
        UObject** HitComponent,bool TraceComplex,FVector* ImpactNormal,bool* StartPenetrating,
        double Radius,double HalfHeight)
    {
        if (HitComponent) *HitComponent = nullptr;
        try
        {
            if (!WorldContext) throw std::runtime_error("world context was unavailable");
            const bool capsule=Radius>0;
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr,capsule?TEXT("/Script/Engine.KismetSystemLibrary:CapsuleTraceSingleByProfile")
                    :TEXT("/Script/Engine.KismetSystemLibrary:LineTraceSingle"));
            if (!function) throw std::runtime_error("Native collision query was unavailable");
            DragonWilds::ReflectedContract::Frame(function);

            std::vector<uint8> parameters(function->GetParmsSize(), 0);
            auto* worldProperty = function->FindProperty(
                FName(TEXT("WorldContextObject"), FNAME_Find));
            auto* startProperty = CastField<FStructProperty>(function->FindProperty(
                FName(TEXT("Start"), FNAME_Find)));
            auto* endProperty = CastField<FStructProperty>(function->FindProperty(
                FName(TEXT("End"), FNAME_Find)));
            auto* outHitProperty = CastField<FStructProperty>(function->FindProperty(
                FName(TEXT("OutHit"), FNAME_Find)));
            auto* ignoredProperty = CastField<FArrayProperty>(function->FindProperty(
                FName(TEXT("ActorsToIgnore"), FNAME_Find)));
            auto* returnProperty = CastField<FBoolProperty>(function->GetReturnProperty());
            if (!CastField<FObjectProperty>(worldProperty)||worldProperty->GetArrayDim()!=1||worldProperty->GetElementSize()!=sizeof(UObject*)
                ||!startProperty||startProperty->GetElementSize()!=sizeof(FVector)||!endProperty||endProperty->GetElementSize()!=sizeof(FVector)
                ||!IsValidParameter(worldProperty, parameters.size(), sizeof(UObject*))
                || !IsValidParameter(startProperty, parameters.size(), sizeof(FVector))
                || !IsValidParameter(endProperty, parameters.size(), sizeof(FVector))
                || !outHitProperty || !outHitProperty->GetStruct()
                || !ignoredProperty
                || !CastField<FObjectPropertyBase>(ignoredProperty->GetInner())
                || !IsValidParameter(ignoredProperty, parameters.size(),
                    sizeof(TArray<AActor*>))
                || !IsValidParameter(outHitProperty, parameters.size(),
                    static_cast<size_t>(outHitProperty->GetElementSize()))
                || !returnProperty
                || !IsValidParameter(returnProperty, parameters.size(),
                    static_cast<size_t>(returnProperty->GetElementSize())))
                throw std::runtime_error("LineTraceSingle parameter layout did not match");

            *worldProperty->ContainerPtrToValuePtr<UObject*>(parameters.data()) = WorldContext;
            std::memcpy(startProperty->ContainerPtrToValuePtr<void>(parameters.data()),
                &Start, sizeof(Start));
            std::memcpy(endProperty->ContainerPtrToValuePtr<void>(parameters.data()),
                &End, sizeof(End));
            if(!capsule)SetNumericParameter(function, parameters, TEXT("TraceChannel"), 0);
            else {
                // Ground rays use visibility; body clearance must also see
                // Pawn-profile obstacles which can ignore that ray channel.
                auto* profile=CastField<FNameProperty>(function->FindProperty(FName(TEXT("ProfileName"),FNAME_Find)));
                if(!DragonWilds::ReflectedContract::Input(profile,function)||profile->GetSize()!=sizeof(FName))
                    throw std::runtime_error("Capsule trace profile-name contract changed");
                const FName name(TEXT("BlockAll"),FNAME_Add);
                profile->CopyCompleteValue(profile->ContainerPtrToValuePtr<void>(parameters.data()),&name);
            }
            SetNumericParameter(function, parameters, TEXT("DrawDebugType"), 0);
            SetBooleanParameter(function, parameters, TEXT("bTraceComplex"), TraceComplex);
            SetBooleanParameter(function, parameters, TEXT("bIgnoreSelf"), false);
            if(capsule)for(const auto& argument:{std::pair{TEXT("Radius"),Radius},std::pair{TEXT("HalfHeight"),HalfHeight}}) {
                auto* scalar=CastField<FFloatProperty>(function->FindProperty(FName(argument.first,FNAME_Find)));
                if(!DragonWilds::ReflectedContract::Input(scalar,function)||scalar->GetSize()!=sizeof(float))
                    throw std::runtime_error("Capsule trace floating parameter contract changed");
                scalar->SetFloatingPointPropertyValue(scalar->ContainerPtrToValuePtr<void>(parameters.data()),argument.second);
            }

            auto* ignored = new (
                ignoredProperty->ContainerPtrToValuePtr<void>(parameters.data()))
                TArray<AActor*>();
            struct IgnoredArrayGuard
            {
                TArray<AActor*>* Array{};
                ~IgnoredArrayGuard() { if (Array) Array->~TArray(); }
            } ignoredGuard{ignored};
            for (auto* actor : ActorsToIgnore)
                if (actor) ignored->Add(actor);

            auto* defaultObject = GetDefaultObj();
            if (!defaultObject)
                throw std::runtime_error("KismetSystemLibrary default object was unavailable");
            defaultObject->ProcessEvent(function, parameters.data());
            const auto hit = returnProperty->GetPropertyValue(
                returnProperty->ContainerPtrToValuePtr<void>(parameters.data()));
            if (!hit)
            {
                Error.clear();
                return false;
            }

            auto* impactProperty = CastField<FStructProperty>(
                DragonWilds::PropertyHelper::GetPropertyByName(
                    outHitProperty->GetStruct().Get(), TEXT("ImpactPoint")));
            if (!impactProperty
                || impactProperty->GetElementSize() != sizeof(FVector)
                || impactProperty->GetOffset_Internal() < 0
                || static_cast<size_t>(impactProperty->GetOffset_Internal())
                    + sizeof(FVector)
                    > static_cast<size_t>(outHitProperty->GetElementSize()))
                throw std::runtime_error("FHitResult.ImpactPoint layout did not match FVector");
            auto* hitData = outHitProperty->ContainerPtrToValuePtr<void>(parameters.data());
            std::memcpy(&ImpactPoint,
                impactProperty->ContainerPtrToValuePtr<void>(hitData), sizeof(ImpactPoint));
            if(ImpactNormal){
                auto* normal=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(outHitProperty->GetStruct().Get(),TEXT("ImpactNormal")));
                if(!normal||normal->GetElementSize()!=sizeof(FVector)||!IsValidParameter(normal,outHitProperty->GetElementSize(),sizeof(FVector)))
                    throw std::runtime_error("FHitResult.ImpactNormal layout changed");
                std::memcpy(ImpactNormal,normal->ContainerPtrToValuePtr<void>(hitData),sizeof(FVector));
            }
            if(StartPenetrating){
                auto* penetrating=CastField<FBoolProperty>(DragonWilds::PropertyHelper::GetPropertyByName(outHitProperty->GetStruct().Get(),TEXT("bStartPenetrating")));
                if(!penetrating||!IsValidParameter(penetrating,outHitProperty->GetElementSize(),penetrating->GetElementSize()))
                    throw std::runtime_error("FHitResult.bStartPenetrating layout changed");
                *StartPenetrating=penetrating->GetPropertyValue(penetrating->ContainerPtrToValuePtr<void>(hitData));
            }
            if (HitComponent) {
                auto* component = CastField<FObjectPropertyBase>(
                    DragonWilds::PropertyHelper::GetPropertyByName(
                        outHitProperty->GetStruct().Get(), TEXT("Component")));
                if (!component || !IsValidParameter(component,
                        outHitProperty->GetElementSize(), component->GetElementSize()))
                    throw std::runtime_error("FHitResult.Component metadata was unavailable");
                *HitComponent = component->GetObjectPropertyValue(
                    component->ContainerPtrToValuePtr<void>(hitData));
            }
            Error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            Error = exception.what();
            return false;
        }
    }

    bool UKismetSystemLibrary::FindGroundSurface(UObject* world,const FVector& start,
        const FVector& end,const std::vector<AActor*>& ignored,FVector& impact,std::string& error,
        std::optional<double> referenceZ,PS::Grounding::QueryState* state)
    {
        error.clear();
        if(state)*state={};
        const PS::Grounding::SearchRange range{start.Z(),end.Z()};
        if(!range.Valid()||!std::isfinite(start.X())||!std::isfinite(start.Y())
            ||start.X()!=end.X()||start.Y()!=end.Y()){
            error="Ground placement requires a finite downward vertical query";return false;
        }
        // Without an authored height, find landscape beneath overhead actors.
        // Ignoring an actor is query-local; no actor or collision is changed.
        if(!referenceZ){
            auto* landscape=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Landscape.LandscapeHeightfieldCollisionComponent"));
            if(!landscape){error="Terrain collision metadata unavailable; supply GroundReferenceZ to select a floor";return false;}
            auto terrainIgnored=ignored;bool found=false;
            for(int attempt=0;attempt<16;++attempt){
                UObject* component=nullptr;FVector terrain{};bool contractFailure=false;
                const bool hit=PS::Grounding::QuerySurface([&](bool complex){
                    if(contractFailure)return false;
                    if(LineTraceGround(world,start,end,terrainIgnored,terrain,error,&component,complex))return true;
                    contractFailure=!error.empty();return false;
                });
                if(!hit)return false;
                if(!component||component->GetWorld()!=world){error="Ground collision owner could not be verified";return false;}
                if(component->IsA(landscape)){
                    if(!range.Contains(terrain.Z())||!std::isfinite(terrain.X())||!std::isfinite(terrain.Y())
                        ||std::abs(terrain.X()-start.X())>1||std::abs(terrain.Y()-start.Y())>1){error="Terrain hit lies outside the search column";return false;}
                    referenceZ=terrain.Z();found=true;break;
                }
                auto* owner=component->GetOuterPrivate();
                if(!owner||!owner->IsA(AActor::StaticClass())||owner->GetWorld()!=world){error="GroundReferenceZ is required for this stacked collision layout";return false;}
                auto* actor=static_cast<AActor*>(owner);
                if(std::find(terrainIgnored.begin(),terrainIgnored.end(),actor)!=terrainIgnored.end()){
                    error="Ground collision query did not advance";return false;
                }
                terrainIgnored.push_back(actor);
            }
            if(!found){error="Terrain search exceeded its bounded collision budget; supply GroundReferenceZ";return false;}
        }
        PS::Grounding::SearchRange floor;
        try{floor=PS::Grounding::FloorRange(range,*referenceZ);}catch(const std::exception& e){error=e.what();return false;}
        if(state)state->FloorReference=referenceZ;
        bool contractFailure=false;FVector normal{};bool penetrating=false;
        const bool hit=PS::Grounding::QuerySurface([&](bool complex){
            if(contractFailure)return false;
            if(!LineTraceGround(world,FVector(start.X(),start.Y(),floor.Top),FVector(end.X(),end.Y(),floor.Bottom),ignored,impact,error,nullptr,complex,&normal,&penetrating)){
                contractFailure=!error.empty();return false;
            }
            if(!floor.Contains(impact.Z())||!std::isfinite(impact.X())||!std::isfinite(impact.Y())
                ||std::abs(impact.X()-start.X())>1||std::abs(impact.Y()-start.Y())>1){
                error="Ground hit lies outside the requested vertical segment";contractFailure=true;return false;
            }
            const bool acceptable=!penetrating&&std::isfinite(normal.Z())&&normal.Z()>=0.5;
            if(state&&!acceptable)state->Obstructed=true;
            return acceptable;
        });
        if(state&&hit)state->Obstructed=false;
        return hit;
    }

	UKismetSystemLibrary* UKismetSystemLibrary::GetDefaultObj()
	{
		static auto Self = UECustom::UObjectGlobals::StaticFindObject<UKismetSystemLibrary*>(nullptr, nullptr, TEXT("/Script/Engine.Default__KismetSystemLibrary"));
		return Self;
	}
}
