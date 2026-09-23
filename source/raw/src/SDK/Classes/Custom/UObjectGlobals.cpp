#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/DragonWildsSignatures.h"
#include "Utility/Logging.h"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UObjectHashTables.hpp"
#include "Runtime/Storefront.h"
#include <string_view>
#ifdef RUNESCHEMA_PLUGIN_CLIENT
#include "Runtime/PluginRuntimeServices.h"
#endif

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    namespace {
        bool IsShortObjectName(const TCHAR* name)
        {
            if(!name || !*name)return false;
            const std::basic_string_view<TCHAR> value{name};
            return value.find(TEXT('/'))==value.npos
                && value.find(TEXT('.'))==value.npos
                && value.find(TEXT(':'))==value.npos;
        }

        bool IsPackageObjectPath(const TCHAR* name)
        {
            if(!name || *name!=TEXT('/'))return false;
            const std::basic_string_view<TCHAR> value{name};
            return value.find(TEXT('.'))==value.npos
                && value.find(TEXT(':'))==value.npos;
        }

        bool IsWithinOuter(UObject* object,UObject* requested)
        {
            if(!requested || requested==reinterpret_cast<UObject*>(RC::Unreal::UObjectGlobals::ANY_PACKAGE))return true;
            for(auto* outer=object?object->GetOuterPrivate():nullptr;outer;outer=outer->GetOuterPrivate())
                if(outer==requested)return true;
            return false;
        }

        UObject* FindShortObject(UClass* objectClass,UObject* outer,const TCHAR* name,bool exactClass)
        {
            const FName shortName{name,FNAME_Find};
            if(shortName==NAME_None)return nullptr;
            UObject* found=nullptr;
            if(FUObjectHashTables::IsAvailable()) {
                auto& tables=FUObjectHashTables::Get();
                FUObjectHashTables::FScopedLock lock(tables);
                tables.ForEachObjectWithNameHash(GetObjectHash(shortName),[&](UObjectBase* base) {
                    auto* object=static_cast<UObject*>(base);
                    if(found || !object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))
                        || object->GetFName()!=shortName || !IsWithinOuter(object,outer))return;
                    if(objectClass && (exactClass?object->GetClassPrivate()!=objectClass:!object->IsA(objectClass)))return;
                    found=object;
                });
                return found;
            }
            RC::Unreal::UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32)->LoopAction {
                if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))
                    || object->GetFName()!=shortName || !IsWithinOuter(object,outer))return LoopAction::Continue;
                if(objectClass && (exactClass?object->GetClassPrivate()!=objectClass:!object->IsA(objectClass)))
                    return LoopAction::Continue;
                found=object;return LoopAction::Break;
            });
            return found;
        }

        UObject* FindPathObject(UClass* objectClass,UObject* outer,const TCHAR* path,bool exactClass)
        {
            if(!path || !*path)return nullptr;
            if(FUObjectHashTables::IsAvailable() && !outer)
                return FUObjectHashTables::StaticFindObject(objectClass,nullptr,FName(path,FNAME_Find),exactClass);
            UObject* found=nullptr;
            RC::Unreal::UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32)->LoopAction {
                if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))
                    || object->GetPathName()!=path || !IsWithinOuter(object,outer))return LoopAction::Continue;
                if(objectClass && (exactClass?object->GetClassPrivate()!=objectClass:!object->IsA(objectClass)))
                    return LoopAction::Continue;
                found=object;return LoopAction::Break;
            });
            return found;
        }
    }

    UObject* UObjectGlobals::StaticFindObject(UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool bExactClass)
    {
#ifdef RUNESCHEMA_PLUGIN_CLIENT
        const auto* host=PS::PluginRuntimeServices::Host;
        return host&&host->FindObject?static_cast<UObject*>(host->FindObject(ObjectClass,InObjectPackage,OrigInName,bExactClass?1:0)):nullptr;
#else
        // Steam's validated game signature uses the native object hash tables.
        // The public UE4SS finder rejects bare names on this host, while a
        // full object-array walk for every lookup stalls mod loading.
        using NativeFind=UObject*(*)(UClass*,UObject*,const TCHAR*,bool);
        static const auto nativeFind=reinterpret_cast<NativeFind>(
            DragonWilds::SignatureManager::GetSignature("UObjectGlobals::StaticFindObject"));
        if(PS::Storefront::AllowsSteamNativeSignatures() && nativeFind)
            return nativeFind(ObjectClass,InObjectPackage,OrigInName,bExactClass);
        // UE4SS f6d5f942's public finder misclassifies some bare asset names
        // as long package names, then throws because no '.' delimiter exists.
        // RuneSchema documents intentionally allow stable short references,
        // so resolve those through the public object array and reserve the
        // fast UE4SS hash/path route for canonical paths.
        if(IsShortObjectName(OrigInName))
            return FindShortObject(ObjectClass,InObjectPackage,OrigInName,bExactClass);
        // A package has a valid Unreal path without the dot required by an
        // exported object's canonical path. UE4SS f6d5f942 routes these
        // package names through GetPackageNameFromLongName and throws. Look
        // them up directly, which also covers the engine transient package.
        if(IsPackageObjectPath(OrigInName))
            return FindPathObject(ObjectClass,InObjectPackage,OrigInName,bExactClass);
        try {
            return RC::Unreal::UObjectGlobals::StaticFindObject<UObject*>(
                ObjectClass, InObjectPackage, OrigInName, bExactClass);
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("[DEGRADED][RESOLVER:path] UE4SS rejected object path '{}': {}.\n"),
                OrigInName?OrigInName:TEXT("<null>"),PS::ToWideSafe(error.what()));
            return nullptr;
        }
#endif
    }

    void UObjectGlobals::GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIncludeDerivedClasses,
                                           EObjectFlags ExcludeFlags, EInternalObjectFlags ExclusionInternalFlags)
    {
#ifdef RUNESCHEMA_PLUGIN_CLIENT
        const auto* host=PS::PluginRuntimeServices::Host;if(!host||!host->ForEachObjectOfClass)return;
        const auto visitor=[](void* context,void* object)->int32_t {
            static_cast<TArray<UObject*>*>(context)->Add(static_cast<UObject*>(object));return RS_PLUGIN_OK;
        };
        host->ForEachObjectOfClass(const_cast<UClass*>(ClassToLookFor),bIncludeDerivedClasses?1:0,
            static_cast<uint64_t>(ExcludeFlags),static_cast<uint32_t>(ExclusionInternalFlags),visitor,&Results);
#else
        if(!ClassToLookFor)return;
        using NativeGetObjects=void(*)(const UClass*,TArray<UObject*>&,bool,EObjectFlags,EInternalObjectFlags);
        static const auto nativeGetObjects=reinterpret_cast<NativeGetObjects>(
            DragonWilds::SignatureManager::GetSignature("GetObjectsOfClass"));
        if(PS::Storefront::AllowsSteamNativeSignatures() && nativeGetObjects) {
            nativeGetObjects(ClassToLookFor,Results,bIncludeDerivedClasses,ExcludeFlags,ExclusionInternalFlags);
            return;
        }
        if(FUObjectHashTables::IsAvailable()) {
            auto& tables=FUObjectHashTables::Get();
            FUObjectHashTables::FScopedLock lock(tables);
            const auto collect=[&](UObjectBase* base) {
                auto* object=static_cast<UObject*>(base);
                if(object && !object->HasAnyFlags(ExcludeFlags) && !object->HasAnyInternalFlags(ExclusionInternalFlags))
                    Results.Add(object);
            };
            if(bIncludeDerivedClasses)
                tables.ForEachObjectOfClassIncludingDerived(const_cast<UClass*>(ClassToLookFor),collect);
            else tables.ForEachObjectOfClass(const_cast<UClass*>(ClassToLookFor),collect);
            return;
        }
        RC::Unreal::UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32)->LoopAction {
            if(object&&!object->HasAnyFlags(ExcludeFlags)&&!object->HasAnyInternalFlags(ExclusionInternalFlags)
                &&(bIncludeDerivedClasses?object->IsA(ClassToLookFor):object->GetClassPrivate()==ClassToLookFor))Results.Add(object);
            return LoopAction::Continue;
        });
#endif
    }
}
