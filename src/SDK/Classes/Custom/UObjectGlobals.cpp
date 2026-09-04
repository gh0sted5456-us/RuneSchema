#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/DragonWildsSignatures.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    UObject* UObjectGlobals::StaticFindObject(UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool bExactClass)
    {
        using StaticFindObject_Signature = UObject*(*)(UClass*, UObject*, const TCHAR*, bool);
        static StaticFindObject_Signature StaticFindObject_Internal = nullptr;

        if (!StaticFindObject_Internal)
        {
            StaticFindObject_Internal = reinterpret_cast<StaticFindObject_Signature>(
                DragonWilds::SignatureManager::GetSignature("UObjectGlobals::StaticFindObject")
            );
        }

        if (!StaticFindObject_Internal)
        {
            PS::Log<LogLevel::Error>(STR("Failed to call UObjectGlobals::StaticFindObject because function address was invalid.\n"));
            return nullptr;
        }

        return StaticFindObject_Internal(ObjectClass, InObjectPackage, OrigInName, bExactClass);
    }

    void UObjectGlobals::GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIncludeDerivedClasses, 
                                           EObjectFlags ExcludeFlags, EInternalObjectFlags ExclusionInternalFlags)
    {
        using GetObjectsOfClass_Signature = void(*)(const UClass*, TArray<UObject*>&, bool, EObjectFlags, EInternalObjectFlags);
        static GetObjectsOfClass_Signature GetObjectsOfClass_Internal = nullptr;

        if (!GetObjectsOfClass_Internal)
        {
            GetObjectsOfClass_Internal = reinterpret_cast<GetObjectsOfClass_Signature>(
                DragonWilds::SignatureManager::GetSignature("GetObjectsOfClass")
            );
        }

        if (!GetObjectsOfClass_Internal)
        {
            PS::Log<LogLevel::Error>(STR("Failed to call UObjectGlobals::GetObjectsOfClass because function address was invalid.\n"));
            return;
        }

        return GetObjectsOfClass_Internal(ClassToLookFor, Results, bIncludeDerivedClasses, ExcludeFlags, ExclusionInternalFlags);
    }
}
