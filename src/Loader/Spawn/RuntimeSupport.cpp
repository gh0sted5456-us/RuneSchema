#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Transform.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/PlayerAttributeNames.h"
#include "Core/JsonPatchDirective.h"
#include "Core/JsonLoadOrderMerge.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

#include "Loader/Spawn/RuntimeSupport.h"
using namespace DragonWilds::SpawnRuntime;
namespace DragonWilds::SpawnRuntime {
    UObject* CallWorldContextGetter(const TCHAR* functionPath, const TCHAR* objectPath, UObject* worldContext)
    {
        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, functionPath);
        auto* self = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, objectPath);
        if (!function || !self)
        {
            throw std::runtime_error(std::format("{} was unavailable", RC::to_string(functionPath)));
        }

        std::vector<uint8> params(function->GetParmsSize(), 0);
        auto* contextProperty = function->FindProperty(FName(TEXT("WorldContextObject"), FNAME_Find));
        auto* returnProperty = function->GetReturnProperty();
        if (!contextProperty || !returnProperty
            || contextProperty->GetOffset_Internal() < 0
            || returnProperty->GetOffset_Internal() < 0
            || static_cast<size_t>(contextProperty->GetOffset_Internal()) + sizeof(worldContext) > params.size()
            || static_cast<size_t>(returnProperty->GetOffset_Internal()) + sizeof(UObject*) > params.size())
        {
            throw std::runtime_error(std::format("{} metadata was invalid", RC::to_string(functionPath)));
        }

        std::memcpy(params.data() + contextProperty->GetOffset_Internal(), &worldContext, sizeof(worldContext));
        self->ProcessEvent(function, params.data());
        return *reinterpret_cast<UObject**>(params.data() + returnProperty->GetOffset_Internal());
    }

    UObject* GetGameMode(UObject* worldContext)
    {
        return CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameMode"),
            TEXT("/Script/Engine.Default__GameplayStatics"), worldContext);
    }

}
