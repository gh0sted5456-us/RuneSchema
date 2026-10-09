#include "Utility/NativeFunctionHook.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <format>
#include <iomanip>
#include <iterator>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Runtime/Storefront.h"
#include "Loader/DragonWildsBuildingModLoader.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        constexpr const TCHAR* BuildingPieceClassPath =
            TEXT("/Script/Dominion.BuildingPieceData");
        constexpr const TCHAR* BuildingPieceSubsystemClassPath =
            TEXT("/Script/Dominion.BuildingPieceSubsystem");
        constexpr const TCHAR* ItemDataClassPath =
            TEXT("/Script/Dominion.ItemData");
        constexpr const TCHAR* ProgressComponentClassPath =
            TEXT("/Script/Dominion.ProgressComponent");
        constexpr const TCHAR* CataloguePath =
            TEXT("/Game/Gameplay/BaseBuilding_New/BuildingPieces/"
                 "DA_BuildPieceCatalogue_Default.DA_BuildPieceCatalogue_Default");
        constexpr const TCHAR* StabilityProfilePath =
            TEXT("/Game/Gameplay/BaseBuilding_New/"
                 "DT_StabilityProfile.DT_StabilityProfile");
        std::string OwnedProfileName(std::string_view prefix,
            const RC::StringType& owner, const RC::StringType& key)
        {
            uint64_t hash = 14695981039346656037ull;
            const auto add = [&](const std::string& value) {
                for (const unsigned char byte : value)
                {
                    hash ^= byte;
                    hash *= 1099511628211ull;
                }
                hash ^= 0xff;
                hash *= 1099511628211ull;
            };
            add(RC::to_string(owner));
            add(RC::to_string(key));
            std::ostringstream result;
            result << prefix << '_' << std::hex << std::setfill('0') << std::setw(16) << hash;
            return result.str();
        }

        constexpr const TCHAR* UnlockHookPaths[] = {
            TEXT("/Script/Dominion.ProgressComponent:"
                 "Client_HandleNewBuildingPiecesLoadedFromPersistence"),
            TEXT("/Script/Dominion.ProgressComponent:Client_OnBuildingsUnlocked"),
        };
        constexpr const TCHAR* PlayerRestartPath =
            TEXT("/Script/Engine.PlayerController:ClientRestart");
        constexpr float BuildingRecoveryIntervalSeconds = 0.25f;
        constexpr float BuildingRecoveryTimeoutSeconds = 15.0f;
        bool IsFrontEndWorld(UWorld* world)
        {
            if (!world) return true;
            const auto name = world->GetName();
            return name.find(TEXT("FrontEnd")) != RC::StringType::npos
                || name.find(TEXT("MainMenu")) != RC::StringType::npos;
        }
        bool SameSoftObject(const UECustom::FSoftObjectPtr& soft, UObject* object)
        {
            if (!object)
            {
                return false;
            }

            const auto target = UECustom::FSoftObjectPath(object->GetPathName());
            return soft.ObjectID.AssetPath.GetPackageName() == target.AssetPath.GetPackageName()
                && soft.ObjectID.AssetPath.GetAssetName() == target.AssetPath.GetAssetName();
        }

        void InitializeSoftObject(void* destination, UObject* object)
        {
            auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(destination);
            soft->ObjectID = UECustom::FSoftObjectPath(object->GetPathName());
        }

        UObject* ResolveItem(const RC::StringType& reference)
        {
            if (reference.starts_with(TEXT("/")))
            {
                UECustom::TSoftObjectPtr<UObject> soft{
                    UECustom::FSoftObjectPath(reference) };
                return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
            }

            auto* itemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, ItemDataClassPath);
            if (!itemClass)
            {
                return nullptr;
            }

            TArray<UObject*> items;
            UECustom::UObjectGlobals::GetObjectsOfClass(itemClass, items, true);
            const FName referenceName(reference,FNAME_Add);
            for (auto* item : items)
            {
                if (item && item->GetFName() == referenceName
                    && !item->HasAnyFlags(static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject)))
                {
                    return item;
                }
            }

            return nullptr;
        }

    }

    DragonWildsBuildingModLoader::DragonWildsBuildingModLoader()
        : DragonWildsModLoaderBase("buildings")
    {
        SetDisplayName(TEXT("Building Loader"));
    }

    void DragonWildsBuildingModLoader::OnLoad(const std::filesystem::path& loaderPath,
        const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                ReadDefinitions(data, modName);
            });
            return;
        }

        if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            ApplyDefinitions();
        }
    }

    void DragonWildsBuildingModLoader::OnAutoReload(const RC::StringType& modName,
        const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            ReadDefinitions(data, modName);
        });
        ApplyDefinitions();
    }

    bool DragonWildsBuildingModLoader::CanInitialize(
        const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit;
    }

    bool DragonWildsBuildingModLoader::OnInitialize()
    {
        m_buildingPieceClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, BuildingPieceClassPath);
        m_buildingPieceSubsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, BuildingPieceSubsystemClassPath);
        m_progressComponentClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, ProgressComponentClassPath);

        if (!m_buildingPieceClass || !m_buildingPieceSubsystemClass
            || !m_progressComponentClass)
        {
            PS::Log<LogLevel::Error>(
                STR("Unable to initialize Building Loader: required Dominion types were not found.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsBuildingModLoader::ActivateWorldRegistration()
    {
        // Do not install world-entry/world-teardown registry hooks for an
        // empty building configuration.  The loader is present in every
        // RuneSchema install, but touching the native registry is only needed
        // when at least one valid building definition was applied.
        if (m_applied.empty())
        {
            return;
        }

        RegisterHooks();
    }

    DragonWildsBuildingModLoader::~DragonWildsBuildingModLoader()
    {
        if (m_initGameStateCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_initGameStateCallbackId);
        if (m_unlockGameStateCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_unlockGameStateCallbackId);
        if (m_recoveryTickCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_recoveryTickCallbackId);
    }

    void DragonWildsBuildingModLoader::ReadDefinitions(
        const nlohmann::json& data, const RC::StringType& modName)
    {
        if(data.is_object() && data.value("schema",std::string{})=="rsdwtools.buildings.v1") {
            if(!data.contains("pieces") || !data.at("pieces").is_array()) {
                PS::Log<LogLevel::Error>(STR("{}: RSDW Base Builder import requires a pieces array.\n"),modName);
                return;
            }
            if(data.at("pieces").size()>4096) {
                PS::Log<LogLevel::Error>(STR("{}: RSDW Base Builder import exceeds the 4096-piece safety limit.\n"),modName);
                return;
            }
            double destinationX=0,destinationY=0,destinationZ=0,layoutYaw=0;
            double sourceX=0,sourceY=0,sourceZ=0;
            bool allowDeconstruction=false,includeGhosted=false;
            std::string originMode="LocalOrigin",importMode="NativeBuildingPieces",assemblyId;
            std::unordered_set<int64_t> nativePieceIds;
            if(data.contains("RuneSchemaPlacement")) {
                const auto& placement=data.at("RuneSchemaPlacement");
                if(!placement.is_object()) {
                    PS::Log<LogLevel::Error>(STR("{}: RuneSchemaPlacement must be an object.\n"),modName);return;
                }
                const auto finite=[&](const nlohmann::json& object,const char* key,double fallback){
                    if(!object.contains(key))return fallback;
                    if(!object.at(key).is_number())throw std::runtime_error(std::string("RuneSchemaPlacement.")+key+" must be numeric");
                    const auto value=object.at(key).get<double>();
                    if(!std::isfinite(value))throw std::runtime_error(std::string("RuneSchemaPlacement.")+key+" must be finite");
                    return value;
                };
                try {
                    if(placement.contains("Location")) {
                        const auto& location=placement.at("Location");
                        if(!location.is_object())throw std::runtime_error("RuneSchemaPlacement.Location must be an object");
                        destinationX=finite(location,"X",0);destinationY=finite(location,"Y",0);destinationZ=finite(location,"Z",0);
                    }
                    if(placement.contains("Rotation")) {
                        const auto& rotation=placement.at("Rotation");
                        if(!rotation.is_object())throw std::runtime_error("RuneSchemaPlacement.Rotation must be an object");
                        layoutYaw=finite(rotation,"Yaw",0);
                    }
                    originMode=placement.value("OriginMode",originMode);
                    if(originMode!="LocalOrigin" && originMode!="BoundsCenter" && originMode!="AnchorPiece")
                        throw std::runtime_error("RuneSchemaPlacement.OriginMode must be LocalOrigin, BoundsCenter, or AnchorPiece");
                    importMode=placement.value("ImportMode",importMode);
                    if(importMode!="NativeBuildingPieces" && importMode!="StaticAssembly")
                        throw std::runtime_error("RuneSchemaPlacement.ImportMode must be NativeBuildingPieces or StaticAssembly");
                    assemblyId=placement.value("AssemblyId",std::string{});
                    if(!assemblyId.empty() && (assemblyId.size()>128 || assemblyId.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos))
                        throw std::runtime_error("RuneSchemaPlacement.AssemblyId must use 1-128 letters, digits, '.', '_' or '-'");
                    if(placement.contains("NativePieceIds")) {
                        const auto& ids=placement.at("NativePieceIds");
                        if(!ids.is_array() || ids.size()>4096)
                            throw std::runtime_error("RuneSchemaPlacement.NativePieceIds must be an array of at most 4096 integers");
                        for(const auto& id:ids) {
                            if(!id.is_number_integer())throw std::runtime_error("RuneSchemaPlacement.NativePieceIds entries must be integers");
                            nativePieceIds.insert(id.get<int64_t>());
                        }
                    }
                    if(placement.contains("AllowDeconstruction")) {
                        if(!placement.at("AllowDeconstruction").is_boolean())throw std::runtime_error("RuneSchemaPlacement.AllowDeconstruction must be boolean");
                        allowDeconstruction=placement.at("AllowDeconstruction").get<bool>();
                    }
                    if(placement.contains("IncludeGhosted")) {
                        if(!placement.at("IncludeGhosted").is_boolean())throw std::runtime_error("RuneSchemaPlacement.IncludeGhosted must be boolean");
                        includeGhosted=placement.at("IncludeGhosted").get<bool>();
                    }
                } catch(const std::exception& error) {
                    PS::Log<LogLevel::Error>(STR("{}: RSDW Base Builder placement rejected: {}.\n"),modName,PS::ToWideSafe(error.what()));return;
                }
            }
            if(originMode=="BoundsCenter" && !data.at("pieces").empty()) {
                double minX=std::numeric_limits<double>::max(),minY=minX,minZ=minX;
                double maxX=std::numeric_limits<double>::lowest(),maxY=maxX,maxZ=maxX;
                bool found=false;
                for(const auto& piece:data.at("pieces"))if(piece.is_object()
                    && (!piece.contains("x")||piece.at("x").is_number())
                    && (!piece.contains("y")||piece.at("y").is_number())
                    && (!piece.contains("z")||piece.at("z").is_number())) {
                    const auto x=piece.value("x",0.0),y=piece.value("y",0.0),z=piece.value("z",0.0);found=true;
                    minX=std::min(minX,x);minY=std::min(minY,y);minZ=std::min(minZ,z);
                    maxX=std::max(maxX,x);maxY=std::max(maxY,y);maxZ=std::max(maxZ,z);
                }
                if(!found){PS::Log<LogLevel::Error>(STR("{}: BoundsCenter origin could not find any valid piece transforms.\n"),modName);return;}
                sourceX=(minX+maxX)/2;sourceY=(minY+maxY)/2;sourceZ=(minZ+maxZ)/2;
            } else if(originMode=="AnchorPiece") {
                if(!data.contains("anchor_piece_id") || !data.at("anchor_piece_id").is_number_integer()) {
                    PS::Log<LogLevel::Error>(STR("{}: AnchorPiece origin requires anchor_piece_id in the Base Builder export.\n"),modName);return;
                }
                const auto anchor=data.at("anchor_piece_id").get<int64_t>();bool found=false;
                for(const auto& piece:data.at("pieces"))if(piece.is_object() && piece.value("piece_id",int64_t{})==anchor) {
                    sourceX=piece.value("x",0.0);sourceY=piece.value("y",0.0);sourceZ=piece.value("z",0.0);found=true;break;
                }
                if(!found){PS::Log<LogLevel::Error>(STR("{}: Base Builder anchor_piece_id was not found in pieces.\n"),modName);return;}
            }
            const auto radians=layoutYaw*std::numbers::pi/180.0;
            const auto cosine=std::cos(radians),sine=std::sin(radians);
            nlohmann::json placements=nlohmann::json::array();
            nlohmann::json staticPieces=nlohmann::json::array();
            std::size_t rejected=0;
            for(const auto& piece:data.at("pieces")) try {
                if(!piece.is_object() || !piece.contains("piece_id") || !piece.at("piece_id").is_number_integer()
                    || !piece.contains("piece_data_name") || !piece.at("piece_data_name").is_string())
                    throw std::runtime_error("piece_id and piece_data_name are required");
                auto building=piece.at("piece_data_name").get<std::string>();
                constexpr std::string_view prefix="BuildingPieceData ";
                if(building.starts_with(prefix))building.erase(0,prefix.size());
                if(building.empty() || building[0]!='/')throw std::runtime_error("piece_data_name is not an Unreal asset path");
                const auto number=[&](const char* key,double fallback){
                    if(!piece.contains(key))return fallback;
                    if(!piece.at(key).is_number())throw std::runtime_error(std::string(key)+" must be numeric");
                    const auto value=piece.at(key).get<double>();
                    if(!std::isfinite(value))throw std::runtime_error(std::string(key)+" must be finite");
                    return value;
                };
                if(piece.value("is_ghosted",false) && !includeGhosted)continue;
                const auto localX=number("x",0)-sourceX,localY=number("y",0)-sourceY;
                const auto localZ=number("z",0)-sourceZ;
                const auto pieceId=piece.at("piece_id").get<int64_t>();
                const auto id=std::format("basebuilder-{}",pieceId);
                const bool native=importMode=="NativeBuildingPieces" || nativePieceIds.contains(pieceId);
                if(native) {
                    const auto worldX=destinationX+(localX*cosine-localY*sine);
                    const auto worldY=destinationY+(localX*sine+localY*cosine);
                    placements.push_back({{"Type","BuildingProp"},{"Id",id},{"Building",building},
                        {"Location",{{"X",worldX},{"Y",worldY},{"Z",destinationZ+localZ}}},
                        {"Rotation",{{"Pitch",number("pitch",0)},{"Yaw",number("yaw",0)+layoutYaw},{"Roll",number("roll",0)}}},
                        {"Scale",{{"X",number("scale_x",1)},{"Y",number("scale_y",1)},{"Z",number("scale_z",1)}}},
                        {"GroundToSurface",false},{"AllowDeconstruction",allowDeconstruction}});
                } else {
                    std::string classPath;
                    if(piece.contains("class_name") && piece.at("class_name").is_string()) {
                        classPath=piece.at("class_name").get<std::string>();
                        constexpr std::string_view classPrefix="BlueprintGeneratedClass ";
                        if(classPath.starts_with(classPrefix))classPath.erase(0,classPrefix.size());
                    }
                    staticPieces.push_back({{"PieceId",pieceId},{"Building",building},{"Class",classPath},
                        {"Location",{{"X",localX},{"Y",localY},{"Z",localZ}}},
                        {"Rotation",{{"Pitch",number("pitch",0)},{"Yaw",number("yaw",0)},{"Roll",number("roll",0)}}},
                        {"Scale",{{"X",number("scale_x",1)},{"Y",number("scale_y",1)},{"Z",number("scale_z",1)}}}});
                }
            } catch(const std::exception& error) {
                ++rejected;PS::Log<LogLevel::Error>(STR("{}: RSDW Base Builder piece rejected: {}.\n"),modName,PS::ToWideSafe(error.what()));
            }
            if(!staticPieces.empty()) {
                if(assemblyId.empty()) {
                    auto sourceName=data.value("name",std::string("basebuilder"));
                    assemblyId="basebuilder-assembly-";
                    for(const auto value:sourceName)assemblyId.push_back(std::isalnum(static_cast<unsigned char>(value))?value:'-');
                    if(assemblyId.size()>120)assemblyId.resize(120);
                }
                placements.push_back({{"Type","StaticAssembly"},{"Id",assemblyId},
                    {"Location",{{"X",destinationX},{"Y",destinationY},{"Z",destinationZ}}},
                    {"Rotation",{{"Pitch",0},{"Yaw",layoutYaw},{"Roll",0}}},
                    {"Scale",{{"X",1},{"Y",1},{"Z",1}}},{"GroundToSurface",false},
                    {"CollisionProfile","BlockAll"},{"Pieces",std::move(staticPieces)}});
            }
            if(ImportPlacements && !placements.empty())ImportPlacements(placements,modName);
            else if(!placements.empty())PS::Log<LogLevel::Error>(STR("{}: RSDW Base Builder placement service is unavailable; definitions and other loaders continue.\n"),modName);
            const auto extras=(data.contains("items")&&data.at("items").is_array()?data.at("items").size():0)
                +(data.contains("actors")&&data.at("actors").is_array()?data.at("actors").size():0);
            if(extras)PS::Log<LogLevel::Warning>(STR("{}: RSDW Base Builder import skipped {} item/actor record(s); only validated building pieces are imported.\n"),modName,extras);
            PS::Log<LogLevel::Normal>(STR("{}: imported {} RSDW Base Builder placement(s); {} rejected.\n"),modName,placements.size(),rejected);
            return;
        }
        if (data.is_array())
        {
            for (const auto& entry : data) ReadDefinitions(entry, modName);
            return;
        }
        if (!data.is_object())
        {
            PS::Log<LogLevel::Error>(
                STR("{}: building file must contain a JSON object.\n"), modName);
            return;
        }

        if (data.contains("$Patch")) {
            ApplyPatch(data, modName);
            return;
        }
        for (const auto& [key, body] : data.items())
        {
            if (key.starts_with("$"))
            {
                continue;
            }

            if (!body.is_object())
            {
                PS::Log<LogLevel::Error>(
                    STR("{}: Building '{}' must be a JSON object.\n"),
                    modName, RC::to_generic_string(key));
                continue;
            }

            const bool hasAsset = body.contains("Asset") && body.at("Asset").is_string();
            const bool hasClone = body.contains("$Clone") && body.at("$Clone").is_string();
            if (hasAsset == hasClone)
            {
                PS::Log<LogLevel::Error>(
                    STR("{}: Building '{}' requires exactly one string 'Asset' or '$Clone' path.\n"),
                    modName, RC::to_generic_string(key));
                continue;
            }

            BuildingDefinition definition{};
            definition.Owner = modName;
            definition.Key = RC::to_generic_string(key);
            definition.Clone = hasClone;
            definition.AssetPath = RC::to_generic_string(
                body.at(hasClone ? "$Clone" : "Asset").get<std::string>());

            if (body.contains("Properties"))
            {
                if (!body.at("Properties").is_object())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Properties' must be a JSON object.\n"),
                        modName, definition.Key);
                    continue;
                }
                definition.Properties = body.at("Properties");
                bool safe = true;
                for (const auto* protectedField : {
                         "PersistenceID", "InternalName", "BuildingPieceDataIndex",
                         "Requirements" })
                {
                    if (definition.Properties.contains(protectedField))
                    {
                        PS::Log<LogLevel::Error>(
                            STR("{}: Building '{}.Properties.{}' is RuneSchema-managed; use Requirements for cost and let RuneSchema assign identity/index fields.\n"),
                            modName, definition.Key,
                            RC::to_generic_string(protectedField));
                        safe = false;
                    }
                }
                if (definition.Properties.contains("BuildableActor")
                    && !definition.Properties.at("BuildableActor").is_string())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Properties.BuildableActor' must be a cooked Blueprint generated-class path ending in '_C'.\n"),
                        modName, definition.Key);
                    safe = false;
                }
                if (!safe) continue;
            }

            if (body.contains("Requirements"))
            {
                const auto& requirements = body.at("Requirements");
                if (!requirements.is_array())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Requirements' must be an array.\n"),
                        modName, definition.Key);
                    continue;
                }

                bool valid = true;
                for (const auto& requirement : requirements)
                {
                    if (!requirement.is_object()
                        || !requirement.contains("ItemData")
                        || !requirement.at("ItemData").is_string()
                        || !requirement.contains("Amount")
                        || !requirement.at("Amount").is_number_integer()
                        || requirement.at("Amount").get<int64_t>() <= 0)
                    {
                        valid = false;
                        break;
                    }
                }

                if (!valid)
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Requirements' entries require an ItemData string and positive Amount.\n"),
                        modName, definition.Key);
                    continue;
                }

                definition.Requirements = requirements;
            }

            if(body.contains("Overrides")) {
                const auto& overrides=body.at("Overrides");
                bool valid=overrides.is_object();
                for(const auto& [section,value]:overrides.items()) {
                    if(section!="Names"&&section!="Placement"&&section!="Stability"
                        &&section!="DerivedData"&&section!="Shelter"&&section!="Health"
                        &&section!="Snapping"&&section!="Processing")valid=false;
                    if(!value.is_object())valid=false;
                }
                if(overrides.contains("Names"))for(const auto& [name,value]:overrides.at("Names").items())
                    if((name!="Catalogue"&&name!="World"&&name!="Interact"&&name!="Menu")||!value.is_string()||value.get<std::string>().empty())valid=false;
                if(overrides.contains("Placement"))for(const auto& [name,value]:overrides.at("Placement").items()) {
                    if(name=="Profile")valid=valid&&value.is_string()&&!value.get<std::string>().empty();
                    else if(name=="RequiresFoundation"||name=="RequiresRoof"||name=="RequiresShelter")valid=valid&&value.is_boolean();
                    else if(name=="bAllowUserHeightModification"||name=="bAllowUserRotationModification"
                        ||name=="bCanOnlyBeSnapped"||name=="bForcePlugRotation"
                        ||name=="bCanSharePlugWithSamePieces"||name=="bCanOnlyBePlacedOnDefinedSurface"
                        ||name=="bCanOnlyBePlacedOnCertainPhysicalSurfaces"||name=="bOverrideRotationFromHitSurface"
                        ||name=="bCanOnlyBePlacedOnGround"||name=="bOverrideProjectionNormal"
                        ||name=="bShouldOffsetFromNonBuildingSurface"||name=="bAllowOverlappingWithBuildingPieces"
                        ||name=="bForceBuildingBlockerOverlapDuringPlacement"||name=="bOverrideSnappingMode")
                        valid=valid&&value.is_boolean();
                    else if(name=="MagnetizingMultiplier"||name=="OverlappingBoundsMultiplier")
                        valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=0&&value.get<double>()<=100;
                    else if(name=="SurfaceRotationOffset"||name=="SurfacePlacementNormal"||name=="OverrideProjectionNormal"
                        ||name=="RegionBlockList")valid=valid&&value.is_object();
                    else if(name=="AcceptedPhysicalSurfaces"||name=="OverlapExceptionFilter") {
                        valid=valid&&value.is_array()&&value.size()<=128;
                        if(value.is_array())for(const auto& entry:value)valid=valid&&entry.is_string();
                    }
                    else if(name=="SnappingModeOverride")valid=valid&&value.is_string()&&!value.get<std::string>().empty();
                    else valid=false;
                }
                if(overrides.contains("Stability"))for(const auto& [name,value]:overrides.at("Stability").items()) {
                    if(name=="Profile")valid=valid&&value.is_string()&&!value.get<std::string>().empty();
                    else if(name=="MaxStability"||name=="MinStability"||name=="VerticalLoss"||name=="HorizontalLoss")
                        valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=0&&value.get<double>()<=1000000;
                    else valid=false;
                }
                if(overrides.contains("DerivedData"))for(const auto& [name,value]:overrides.at("DerivedData").items()) {
                    if(name=="PlacementZOffset"||name=="PhysicalSurfaceExtentNeg"||name=="PhysicalSurfaceExtentPos")
                        valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=-100000&&value.get<double>()<=100000;
                    else valid=false;
                }
                if(overrides.contains("Shelter"))for(const auto& [name,value]:overrides.at("Shelter").items()) {
                    if(name=="InteractionRequirements")valid=valid&&value.is_string()&&!value.get<std::string>().empty();
                    else if(name=="bShelterCheckedOnPlacement"||name=="bIncludeNonBuildingPartActors")valid=valid&&value.is_boolean();
                    else if(name=="RequiresRoofText"||name=="RequiresShelterText"||name=="RequiresNoRoofText"||name=="RequiresNoShelterText")valid=valid&&value.is_string();
                    else if(name=="RoofRays"||name=="ShelterRays")valid=valid&&value.is_array()&&value.size()<=128;
                    else if(name=="RoofTraceExclusionFilter"||name=="ShelterTraceExclusionFilter") {
                        valid=valid&&value.is_array()&&value.size()<=128;
                        if(value.is_array())for(const auto& entry:value)valid=valid&&entry.is_string();
                    }
                    else if(name=="SweepRayThickness"||name=="SweepRayDistance"||name=="ValidityPercentage")
                        valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=0&&value.get<double>()<=100000;
                    else valid=false;
                }
                if(overrides.contains("Health"))for(const auto& [name,value]:overrides.at("Health").items()) {
                    if(name=="MaxHealth")valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>0&&value.get<double>()<=100000000;
                    else if(name=="bCanDie")valid=valid&&value.is_boolean();
                    else valid=false;
                }
                if(overrides.contains("Snapping"))for(const auto& [name,value]:overrides.at("Snapping").items()) {
                    if(name=="SnappingRadius"||name=="SnappingRadiusInBasicSnappingMode")
                        valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=0&&value.get<double>()<=100000;
                    else if(name=="bUseSocketsForPlugGeneration")valid=valid&&value.is_boolean();
                    else valid=false;
                }
                if(overrides.contains("Processing"))for(const auto& [name,value]:overrides.at("Processing").items()) {
                    if(name=="Rate")valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>0&&value.get<double>()<=100;
                    else if(name=="AcceptedFuels") {
                        valid=valid&&value.is_object()&&value.contains("Mode")&&value.at("Mode").is_string();
                        if(!value.is_object())continue;
                        for(const auto& [field,unused]:value.items())if(field!="Mode"&&field!="Items")valid=false;
                        const auto mode=value.value("Mode",std::string{});
                        if(mode!="Replace"&&mode!="Append"&&mode!="Clear")valid=false;
                        if(mode=="Clear")valid=valid&&(!value.contains("Items")||(value.at("Items").is_array()&&value.at("Items").empty()));
                        else valid=valid&&value.contains("Items")&&value.at("Items").is_array()&&!value.at("Items").empty()&&value.at("Items").size()<=64;
                        if(value.contains("Items") && value.at("Items").is_array())for(const auto& item:value.at("Items"))
                            if(!item.is_string()||item.get<std::string>().empty()||item.get<std::string>().front()!='/')valid=false;
                    }
                    else if(name=="StartingFuelItem")valid=valid&&(value.is_null()||(value.is_string()&&!value.get<std::string>().empty()&&value.get<std::string>().front()=='/'));
                    else if(name=="StartingFuelCount")valid=valid&&value.is_number_integer()&&value.get<int64_t>()>=0&&value.get<int64_t>()<=100000;
                    else if(name=="MaxFuelSlots")valid=valid&&value.is_number_integer()&&value.get<int64_t>()>=0&&value.get<int64_t>()<=64;
                    else if(name=="MaxResourceSlots")valid=valid&&value.is_number_integer()&&value.get<int64_t>()>=1&&value.get<int64_t>()<=64;
                    else if(name=="InfluenceRange")valid=valid&&value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>=0&&value.get<double>()<=100000;
                    else if(name=="IgnitesBurning"||name=="StopsWhenRecipeChanges"||name=="CanProcessBeStartedThroughUI"||name=="AutoStartProcess")valid=valid&&value.is_boolean();
                    else valid=false;
                }
                if(!valid) {
                    PS::Log<LogLevel::Error>(STR("{}: Building '{}.Overrides' contains an unsupported field or value.\n"),modName,definition.Key);
                    continue;
                }
                definition.Overrides=overrides;
            }

            if (body.contains("Unlock"))
            {
                if (!body.at("Unlock").is_boolean())
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.Unlock' must be true or false.\n"),
                        modName, definition.Key);
                    continue;
                }
                definition.Unlock = body.at("Unlock").get<bool>();
            }

            if (body.contains("AddTo"))
            {
                const auto& addTo = body.at("AddTo");
                const auto entries = addTo.is_array()
                    ? addTo : nlohmann::json::array({addTo});
                bool valid = !entries.empty();
                for (const auto& entry : entries)
                {
                    if (!entry.is_object() || !entry.contains("Collection")
                        || !entry.at("Collection").is_string()
                        || (entry.contains("PageIndex")
                            && !entry.at("PageIndex").is_number_integer()))
                    {
                        valid = false;
                        break;
                    }
                    Placement placement{};
                    placement.Collection = RC::to_generic_string(
                        entry.at("Collection").get<std::string>());
                    if (entry.contains("PageIndex"))
                        placement.PageIndex = entry.at("PageIndex").get<int32>();
                    definition.Targets.push_back(std::move(placement));
                }
                if (!valid)
                {
                    PS::Log<LogLevel::Error>(
                        STR("{}: Building '{}.AddTo' requires an object (or array of objects) with a Collection string and optional integer PageIndex.\n"),
                        modName, definition.Key);
                    continue;
                }
                definition.InheritSourcePlacement = false;
            }

            auto existing = std::find_if(
                m_definitions.begin(), m_definitions.end(),
                [&](const BuildingDefinition& value) {
                    return value.Owner == definition.Owner
                        && value.Key == definition.Key;
                });

            if (existing == m_definitions.end())
            {
                m_definitions.push_back(std::move(definition));
            }
            else
            {
                *existing = std::move(definition);
            }

            m_applied.erase(Identity(modName, RC::to_generic_string(key)));
        }
    }

    void DragonWildsBuildingModLoader::ApplyPatch(
        const nlohmann::json& patch, const RC::StringType& modName)
    {
        if (!patch.contains("$Patch") || !patch.at("$Patch").is_string()
            || !patch.contains("$Target") || !patch.at("$Target").is_object())
        {
            PS::Log<LogLevel::Error>(STR("{}: Building '$Patch' requires a string identity and object '$Target'.\n"), modName);
            return;
        }

        auto target = RC::to_generic_string(patch.at("$Patch").get<std::string>());
        const auto separator = target.find(TEXT(':'));
        const auto targetOwner = separator == RC::StringType::npos
            ? modName : target.substr(0, separator);
        const auto targetKey = separator == RC::StringType::npos
            ? target : target.substr(separator + 1);
        const auto displayTarget = targetOwner + TEXT(":") + targetKey;

        auto registered = std::find_if(m_definitions.begin(), m_definitions.end(),
            [&](const BuildingDefinition& definition) {
                return definition.Owner == targetOwner && definition.Key == targetKey;
            });
        if (registered == m_definitions.end())
        {
            PS::Log<LogLevel::Error>(STR("{}: Building patch target '{}' was not found.\n"), modName, displayTarget);
            return;
        }

        auto candidate = *registered;
        auto* existing = &candidate;
        const auto& body = patch.at("$Target");
        for (const auto& [name, value] : body.items())
        {
            if (name == "Properties")
            {
                if (!value.is_object())
                {
                    PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.Properties' must be an object.\n"), modName, displayTarget);
                    return;
                }
                if (!existing->Properties.is_object()) existing->Properties = nlohmann::json::object();
                for (const auto& [property, propertyValue] : value.items())
                {
                    if (property == "PersistenceID" || property == "InternalName"
                        || property == "BuildingPieceDataIndex"
                        || property == "Requirements"
                        || (property == "BuildableActor" && !propertyValue.is_string()))
                    {
                        PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.Properties.{}' violates the managed clone contract.\n"),
                            modName, displayTarget, RC::to_generic_string(property));
                        return;
                    }
                    existing->Properties[property] = propertyValue;
                }
            }
            else if (name == "Requirements")
            {
                if (!value.is_array())
                {
                    PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.Requirements' must be an array.\n"), modName, displayTarget);
                    return;
                }
                existing->Requirements = value;
            }
            else if (name == "Unlock")
            {
                if (!value.is_boolean())
                {
                    PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.Unlock' must be boolean.\n"), modName, displayTarget);
                    return;
                }
                existing->Unlock = value.get<bool>();
            }
            else if (name == "AddTo")
            {
                const auto entries = value.is_array()
                    ? value : nlohmann::json::array({value});
                if (entries.empty())
                {
                    PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.AddTo' cannot be empty.\n"), modName, displayTarget);
                    return;
                }
                std::vector<Placement> placements;
                for (const auto& entry : entries)
                {
                    if (!entry.is_object() || !entry.contains("Collection")
                        || !entry.at("Collection").is_string()
                        || (entry.contains("PageIndex")
                            && !entry.at("PageIndex").is_number_integer()))
                    {
                        PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.AddTo' requires Collection strings and optional integer PageIndex values.\n"), modName, displayTarget);
                        return;
                    }
                    Placement placement{};
                    placement.Collection = RC::to_generic_string(
                        entry.at("Collection").get<std::string>());
                    if (entry.contains("PageIndex"))
                        placement.PageIndex = entry.at("PageIndex").get<int32>();
                    placements.push_back(std::move(placement));
                }
                existing->Targets = std::move(placements);
                existing->InheritSourcePlacement = false;
            }
            else if(name=="Overrides") {
                if(!value.is_object()) {
                    PS::Log<LogLevel::Error>(STR("{}: Building patch '{}.Overrides' must be an object.\n"),modName,displayTarget);
                    return;
                }
                existing->Overrides=value;
            }
            else
            {
                PS::Log<LogLevel::Error>(STR("{}: Building patch '{}' cannot change '{}'.\n"), modName, displayTarget, RC::to_generic_string(name));
                return;
            }
        }
        auto writes = body;
        if (writes.contains("Properties") && writes.at("Properties").is_object()) {
            // This loader replaces each named property, rather than recursively merging it.
            for (auto& [key, value] : writes["Properties"].items()) value = nullptr;
        }
        WarnPatchConflicts(m_patchConflicts, "buildings:" + RC::to_string(displayTarget), writes, RC::to_string(modName), false);
        m_applied.erase(Identity(existing->Owner, existing->Key));
        *registered = std::move(candidate);
    }

    void DragonWildsBuildingModLoader::ApplyDefinitions()
    {
        if (!m_catalogue)
        {
            m_catalogue = LoadObject(CataloguePath);
        }

        if (!m_catalogue)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings cannot be loaded because the default build catalogue is unavailable.\n"));
            return;
        }

        LoadResult result{};
        for (const auto& definition : m_definitions)
        {
            const auto identity = Identity(definition.Owner, definition.Key);
            if (m_applied.contains(identity))
            {
                continue;
            }

            UObject* source = nullptr;
            auto* building = LoadBuilding(definition, result, &source);
            if (!building)
            {
                continue;
            }

            const auto fail = [&] {
                if (definition.Clone)
                {
                    RememberBuilding(identity,nullptr);
                    DiscardUncommittedClone(building);
                }
                result.Errors++;
            };
            if (!ValidateBuildableActor(source, definition)
                || !ApplyProperties(building, definition, result)
                || !ApplyRequirements(building, definition)
                || !ApplyOverrides(building,definition,result))
            {
                fail();
                continue;
            }

            // A source Lightweight piece embeds its vanilla mesh in cooked
            // DerivedData. A replacement actor must use the actor-backed path
            // unless the author explicitly supplies another representation.
            if (definition.Properties.contains("BuildableActor")
                && !definition.Properties.contains("RepresentationCategory"))
            {
                auto* representation = PropertyHelper::GetPropertyByName(
                    building->GetClassPrivate(), TEXT("RepresentationCategory"));
                try
                {
                    if (!representation) throw std::runtime_error(
                        "RepresentationCategory is unavailable");
                    PropertyHelper::CopyJsonValueToContainer(
                        building, representation, "ManagedActor");
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building '{}': custom cooked actor could not select ManagedActor representation: {}.\n"),
                        definition.Key, PS::ToWideSafe(error.what()));
                    fail();
                    continue;
                }
            }

            if (!EnsureStabilityProfile(building))
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': stability profile is unavailable.\n"),
                    definition.Key);
                fail();
                continue;
            }

            if (!AddPersistenceIdentity(building))
            {
                fail();
                continue;
            }

            auto placements = definition.InheritSourcePlacement
                ? FindSourcePlacements(source) : definition.Targets;
            if (placements.empty() && !definition.Declared)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': source has no default catalogue location; provide AddTo explicitly.\n"),
                    definition.Key);
                fail();
                continue;
            }
            bool placed = true;
            for (const auto& placement : placements)
                placed = AddToMenu(building, placement) && placed;
            if (!placed) { fail(); continue; }

            RememberBuilding(identity, building);

            if (definition.Unlock)
            {
                m_unlocks.insert(identity);
            }
            else
            {
                m_unlocks.erase(identity);
            }

            m_applied.insert(identity);
            result.Loaded++;
        }

        // Do not install world-entry/world-teardown registry hooks for an
        // empty building configuration.  The loader is present in every
        // RuneSchema install, but touching the native registry is only needed
        // when at least one valid building definition was applied.
        if (m_applied.empty())
        {
            PS::LoaderSummary("buildings", 0, 0, 0, 0, result.Errors);
            return;
        }

        RegisterHooks();
        ApplyUnlocksToWorld(nullptr, !PS::Storefront::IsDedicatedServer());

        if (result.Loaded || result.Errors)
        {
            PS::LoaderSummary("buildings", result.Loaded,
                result.Loaded, 0, 0, result.Errors);
        }
    }

    UObject* DragonWildsBuildingModLoader::LoadBuilding(
        const BuildingDefinition& definition, LoadResult& result, UObject** sourceOut)
    {
        const auto identity = Identity(definition.Owner, definition.Key);
        if (auto* cached = GetValidBuilding(identity))
        {
            if (sourceOut) *sourceOut = LoadObject(definition.AssetPath);
            return cached;
        }

        auto* source = LoadObject(definition.AssetPath);
        if (sourceOut) *sourceOut = source;
        if (!source || !source->IsA(m_buildingPieceClass))
        {
            PS::Log<LogLevel::Error>(
                STR("{}: Building '{}' source '{}' resolved as '{}' instead of BuildingPieceData.\n"),
                definition.Owner, definition.Key, definition.AssetPath,
                source && source->GetClassPrivate()
                    ? source->GetClassPrivate()->GetPathName()
                    : TEXT("<unresolved>"));
            result.Errors++;
            return nullptr;
        }

        PS::Log<LogLevel::Verbose>(
            STR("{}: Building '{}' source resolved as '{}'.\n"),
            definition.Owner, definition.Key, source->GetClassPrivate()->GetPathName());

        auto* building = definition.Clone
            ? CloneBuilding(source, definition.Owner, definition.Key)
            : source;
        if (!building || !building->IsA(m_buildingPieceClass))
        {
            PS::Log<LogLevel::Error>(
                STR("{}: Building '{}' clone did not produce a BuildingPieceData object; resolved as '{}'.\n"),
                definition.Owner, definition.Key,
                building && building->GetClassPrivate()
                    ? building->GetClassPrivate()->GetPathName()
                    : TEXT("<unresolved>"));
            result.Errors++;
            return nullptr;
        }

        if(definition.Declared) {
            auto* id=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(building->GetClassPrivate(),TEXT("PersistenceID")));
            auto* name=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(building->GetClassPrivate(),TEXT("InternalName")));
            const auto actualId=id?RC::to_string(*id->GetPropertyValue(id->ContainerPtrToValuePtr<void>(building))):std::string{};
            const auto actualName=name?RC::to_string(*name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(building))):std::string{};
            if(actualId!=definition.DeclaredPersistenceID || actualName.empty()
                || (definition.DeclaredInternalNameAsserted && actualName!=definition.DeclaredInternalName)) {
                PS::Log<LogLevel::Error>(STR("{}: Building declaration '{}' does not match its cooked PersistenceID/InternalName.\n"),
                    definition.Owner,definition.AssetPath);
                result.Errors++;
                return nullptr;
            }
        }

        building->SetRootSet();
        RememberBuilding(identity,building);
        PS::Log<LogLevel::Verbose>(
            STR("[BUILDING-ASSET][RETAINED] owner='{}' key='{}' configured='{}' object='{}' class='{}' root={} mode={}.\n"),
            definition.Owner,definition.Key,definition.AssetPath,building->GetPathName(),
            building->GetClassPrivate()->GetPathName(),building->IsRootSet(),
            definition.Clone?TEXT("clone"):TEXT("direct"));
        return building;
    }

    UObject* DragonWildsBuildingModLoader::GetValidBuilding(const RC::StringType& identity)
    {
        const auto found=m_buildingHandles.find(identity);
        if(found!=m_buildingHandles.end())if(auto* object=found->second.Get())return object;

        // StaticDuplicateObject can assign/finalize an FUObject serial after a
        // transient clone is rooted. Recover only when the exact retained
        // pointer still occupies its original slot and remains rooted. The
        // slot check happens before the raw token is dereferenced.
        const auto retained=m_buildings.find(identity);
        const auto indexed=m_buildingIndices.find(identity);
        if(retained==m_buildings.end()||indexed==m_buildingIndices.end())return nullptr;
        auto* slot=indexed->second>=0?FUObjectArray::IndexToObject(indexed->second):nullptr;
        if(!slot||slot->GetUObject()!=retained->second)return nullptr;
        auto* object=retained->second;
        if(!object->IsRootSet()||!m_buildingPieceClass||!object->IsA(m_buildingPieceClass))return nullptr;
        auto& refreshed=m_buildingHandles[identity];
        refreshed.Assign(object);
        if(refreshed.Get()!=object)return nullptr;
        PS::Log<LogLevel::Verbose>(
            STR("[BUILDING-ASSET][HANDLE-REFRESH] object='{}' retained_slot={} root=true.\n"),
            object->GetPathName(),indexed->second);
        return object;
    }

    void DragonWildsBuildingModLoader::RememberBuilding(
        const RC::StringType& identity,UObject* object)
    {
        if(!object){
            m_buildings.erase(identity);
            m_buildingIndices.erase(identity);
            m_buildingHandles.erase(identity);
            return;
        }
        const auto index=object->GetInternalIndex();
        auto* slot=index>=0?FUObjectArray::IndexToObject(index):nullptr;
        if(!slot||slot->GetUObject()!=object)
            throw std::runtime_error("Building retention slot is unavailable");
        m_buildings[identity]=object;
        m_buildingIndices[identity]=index;
        m_buildingHandles[identity]=PS::WeakObjectHandle(object);
    }

    bool DragonWildsBuildingModLoader::RefreshBuildingReferencesForWorld()
    {
        bool valid=true;
        for(const auto& definition:m_definitions)
        {
            const auto identity=Identity(definition.Owner,definition.Key);
            if(!m_applied.contains(identity))continue;
            auto* previous=GetValidBuilding(identity);
            UObject* object=previous;
            if(definition.Clone)
            {
                if(!object)
                {
                    PS::Log<LogLevel::Error>(
                        STR("[BUILDING-ASSET][INVALID] owner='{}' key='{}' clone lifetime was lost; registration refused.\n"),
                        definition.Owner,definition.Key);
                    valid=false;
                    continue;
                }
            }
            else
            {
                // A direct cooked export is package-owned. Resolve its stable
                // asset path at every world boundary instead of trusting a raw
                // pointer retained across frontend/world package activity.
                object=LoadObject(definition.AssetPath);
                if(!object||!m_buildingPieceClass||!object->IsA(m_buildingPieceClass))
                {
                    PS::Log<LogLevel::Error>(
                        STR("[BUILDING-ASSET][INVALID] owner='{}' key='{}' configured='{}' could not be re-resolved as BuildingPieceData.\n"),
                        definition.Owner,definition.Key,definition.AssetPath);
                    RememberBuilding(identity,nullptr);
                    valid=false;
                    continue;
                }
                object->SetRootSet();
                RememberBuilding(identity,object);
            }

            // From this point on the object came from a current weak handle or
            // a fresh synchronous resolve; no stale raw pointer is dereferenced.
            auto* type=object->GetClassPrivate();
            auto* idProperty=type?CastField<FStrProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PersistenceID"))):nullptr;
            auto* nameProperty=type?CastField<FStrProperty>(PropertyHelper::GetPropertyByName(type,TEXT("InternalName"))):nullptr;
            if(!idProperty||!nameProperty)
            {
                PS::Log<LogLevel::Error>(
                    STR("[BUILDING-ASSET][INVALID] owner='{}' key='{}' object='{}' lacks PersistenceID/InternalName after world resolve.\n"),
                    definition.Owner,definition.Key,object->GetPathName());
                valid=false;
                continue;
            }
            const auto persistence=RC::to_string(*idProperty->GetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(object)));
            const auto internal=RC::to_string(*nameProperty->GetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(object)));
            if(persistence.empty()||internal.empty()
                || (definition.Declared&&persistence!=definition.DeclaredPersistenceID)
                || (definition.DeclaredInternalNameAsserted&&internal!=definition.DeclaredInternalName))
            {
                PS::Log<LogLevel::Error>(
                    STR("[BUILDING-ASSET][INVALID] owner='{}' key='{}' cooked identity changed: PersistenceID='{}' InternalName='{}'.\n"),
                    definition.Owner,definition.Key,PS::ToWideSafe(persistence.c_str()),PS::ToWideSafe(internal.c_str()));
                valid=false;
                continue;
            }
            PS::Log<LogLevel::Verbose>(
                STR("[BUILDING-ASSET][WORLD] owner='{}' key='{}' configured='{}' object='{}' class='{}' action={} root={} PersistenceID='{}' InternalName='{}'.\n"),
                definition.Owner,definition.Key,definition.AssetPath,object->GetPathName(),type->GetPathName(),
                previous==object?TEXT("reused"):TEXT("re-resolved"),object->IsRootSet(),
                PS::ToWideSafe(persistence.c_str()),PS::ToWideSafe(internal.c_str()));
        }
        return valid;
    }

    UObject* DragonWildsBuildingModLoader::CloneBuilding(
        UObject* source, const RC::StringType& owner, const RC::StringType& key)
    {
        if (!source || !source->GetClassPrivate()) return nullptr;
        auto* transientPackage = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, TEXT("/Engine/Transient"), false);
        if (!transientPackage) return nullptr;

        static uint32 sequence = 0;
        const auto name = std::format(STR("RuneSchemaBuilding_{}_{}"), key, ++sequence);
        FStaticConstructObjectParameters params(source->GetClassPrivate(), transientPackage);
        params.Name = FName(name, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(RF_Public | RF_Standalone | RF_Transactional);
        auto* created = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!created) return nullptr;

        constexpr std::uint64_t unsafeFlags =
            CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient
            | CPF_InstancedReference | CPF_ContainsInstancedReference
            | CPF_Deprecated | CPF_EditorOnly;
        std::size_t copied = 0;
        for (auto* property : TFieldRange<FProperty>(
                 source->GetClassPrivate(), EFieldIterationFlags::Default))
        {
            if (!property || property->HasAnyPropertyFlags(unsafeFlags)) continue;
            property->CopyCompleteValue_InContainer(created, source);
            ++copied;
        }

        // BuildingPieceData points at a shared cooked DerivedData asset.  A
        // placement override must never rewrite that shared object because it
        // would also change the vanilla source and every other piece using it.
        // Give every RuneSchema clone a private transient copy instead.
        UObject* privateDerived = nullptr;
        for (const auto* field : { TEXT("DerivedData"), TEXT("BuildingPieceDerivedData") })
        {
            auto* property = CastField<FSoftObjectProperty>(
                PropertyHelper::GetPropertyByName(source->GetClassPrivate(), field));
            if (!property) continue;
            auto* sourceSoft = property->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(source);
            if (!sourceSoft || sourceSoft->ObjectID.AssetPath.GetPackageName() == NAME_None) break;
            auto* sourceDerived = LoadObject(sourceSoft->ObjectID.AssetPath.GetPackageName().ToString()
                + TEXT(".") + sourceSoft->ObjectID.AssetPath.GetAssetName().ToString());
            if (!sourceDerived || !sourceDerived->GetClassPrivate()) break;

            FStaticConstructObjectParameters derivedParams(sourceDerived->GetClassPrivate(), transientPackage);
            derivedParams.Name = FName(name + TEXT("_DerivedData"), FNAME_Add);
            derivedParams.SetFlags = static_cast<EObjectFlags>(RF_Public | RF_Standalone | RF_Transactional);
            privateDerived = UObjectGlobals::StaticConstructObject<UObject*>(derivedParams);
            if (!privateDerived) break;
            for (auto* derivedProperty : TFieldRange<FProperty>(
                     sourceDerived->GetClassPrivate(), EFieldIterationFlags::Default))
            {
                if (!derivedProperty || derivedProperty->HasAnyPropertyFlags(unsafeFlags)) continue;
                derivedProperty->CopyCompleteValue_InContainer(privateDerived, sourceDerived);
            }
            InitializeSoftObject(property->ContainerPtrToValuePtr<void>(created), privateDerived);
            break;
        }

        const auto stableIdentity = std::format(STR("RuneSchema:{}:{}"), owner, key);
        for (const auto* field : { TEXT("PersistenceID"), TEXT("InternalName") })
        {
            if (auto* property = PropertyHelper::GetPropertyByName(
                    created->GetClassPrivate(), field))
            {
                PropertyHelper::CopyJsonValueToContainer(created, property,
                    RC::to_string(stableIdentity));
            }
        }
        if (!created->IsA(source->GetClassPrivate())
            || !PropertyHelper::GetPropertyByName(
                created->GetClassPrivate(), TEXT("PersistenceID"))
            || !PropertyHelper::GetPropertyByName(
                created->GetClassPrivate(), TEXT("InternalName")))
        {
            PS::Log<LogLevel::Error>(
                STR("{}: cloned building '{}' failed post-copy type/identity validation; resolved as '{}'.\n"),
                owner, key,
                created->GetClassPrivate()
                    ? created->GetClassPrivate()->GetPathName()
                    : TEXT("<unresolved>"));
            return nullptr;
        }
        created->SetRootSet();
        if(privateDerived) {
            privateDerived->SetRootSet();
            m_createdBuildings.push_back(privateDerived);
        }
        m_createdBuildings.push_back(created);
        PS::Log<LogLevel::Verbose>(
            STR("{}: cloned building '{}' as '{}' using {} reflected properties.\n"),
            owner, source->GetPathName(), created->GetPathName(), copied);
        return created;
    }

    bool DragonWildsBuildingModLoader::ValidateBuildableActor(
        UObject* source, const BuildingDefinition& definition)
    {
        RC::StringType path;
        if (definition.Properties.contains("BuildableActor"))
        {
            if (!definition.Clone)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': BuildableActor replacement requires '$Clone'; mutating a shared native data asset is refused.\n"),
                    definition.Key);
                return false;
            }
            path = RC::to_generic_string(
                definition.Properties.at("BuildableActor").get<std::string>());
        }
        else
        {
            auto* property = source ? CastField<FSoftObjectProperty>(
                PropertyHelper::GetPropertyByName(source->GetClassPrivate(), TEXT("BuildableActor"))) : nullptr;
            auto* soft = property
                ? property->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(source) : nullptr;
            if (!soft || soft->ObjectID.AssetPath.GetPackageName() == NAME_None
                || soft->ObjectID.AssetPath.GetAssetName() == NAME_None)
            {
                PS::Log<LogLevel::Error>(STR("Building '{}': baked BuildableActor is empty.\n"), definition.Key);
                return false;
            }
            path = soft->ObjectID.AssetPath.GetPackageName().ToString()
                + TEXT(".") + soft->ObjectID.AssetPath.GetAssetName().ToString();
        }
        auto* actorClass = ActorHelper::ResolveClass(path);
        auto* baseClass = ActorHelper::ResolveClass(
            TEXT("/Game/Gameplay/BaseBuilding/Actors/"
                 "BP_BaseBuilding_BaseActor.BP_BaseBuilding_BaseActor_C"));
        if (!source || !actorClass || !baseClass
            || !actorClass->IsChildOf(baseClass)
            || ActorHelper::IsAbstract(actorClass))
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': cooked BuildableActor '{}' must resolve to a concrete BP_BaseBuilding_BaseActor child. Ensure the mod pak is mounted on server and every client.\n"),
                definition.Key, path);
            return false;
        }

        bool binding = false;
        for (const auto* name : {
                 TEXT("BuildingPieceData"), TEXT("BuildingData"),
                 TEXT("BuildingPiece"), TEXT("BuildingPieceDataIndex") })
        {
            auto* property = PropertyHelper::GetPropertyByName(actorClass, name);
            if (CastField<FObjectProperty>(property)
                || CastField<FSoftObjectProperty>(property)
                || (CastField<FNumericProperty>(property)
                    && CastField<FNumericProperty>(property)->IsInteger()))
            {
                binding = true;
                break;
            }
        }
        auto* defaults = actorClass->GetClassDefaultObject().Get();
        if (!binding || !defaults
            || defaults->HasAnyFlags(static_cast<EObjectFlags>(
                RF_BeginDestroyed | RF_FinishDestroyed | RF_NeedLoad
                | RF_NeedPostLoad | RF_NeedInitialization)))
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': cooked BuildableActor '{}' has no usable BuildingPieceData binding/default object.\n"),
                definition.Key, path);
            return false;
        }
        PS::Log<LogLevel::Normal>(
            STR("[BUILDING-MATERIALIZATION][VERIFIED] owner='{}' key='{}' actor='{}' class='{}' concrete=true binding=true.\n"),
            definition.Owner, definition.Key, path, actorClass->GetPathName());
        return true;
    }

    bool DragonWildsBuildingModLoader::ApplyProperties(
        UObject* building, const BuildingDefinition& definition, LoadResult& result)
    {
        bool valid = true;
        for (const auto& [name, value] : definition.Properties.items())
        {
            auto propertyName = RC::to_generic_string(name);
            auto* property =
                PropertyHelper::GetPropertyByName(building->GetClassPrivate(), propertyName);
            if (!property)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': property '{}' was not found.\n"),
                    definition.Key, propertyName);
                result.Errors++;
                valid = false;
                continue;
            }

            try
            {
                PropertyHelper::CopyJsonValueToContainer(building, property, value);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': failed to set '{}': {}\n"),
                    definition.Key, propertyName, PS::ToWideSafe(error.what()));
                result.Errors++;
                valid = false;
            }
        }
        return valid;
    }

    bool DragonWildsBuildingModLoader::ApplyOverrides(
        UObject* building,const BuildingDefinition& definition,LoadResult& result)
    {
        if(definition.Overrides.is_null()||definition.Overrides.empty())return true;
        const auto fail=[&](const std::string& message) {
            ++result.Errors;
            PS::Log<LogLevel::Error>(STR("Building '{}': override rejected: {}.\n"),
                definition.Key,PS::ToWideSafe(message.c_str()));
            return false;
        };
        const auto writeText=[&](UObject* object,const TCHAR* field,const std::string& value)->bool {
            auto* property=object?CastField<FTextProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),field)):nullptr;
            if(!property)return false;
            PropertyHelper::CopyJsonValueToContainer(object,property,nlohmann::json(value));
            return true;
        };
        const auto resolveActorClass=[&]() -> UClass* {
            auto* property=building?CastField<FSoftObjectProperty>(
                PropertyHelper::GetPropertyByName(building->GetClassPrivate(),TEXT("BuildableActor"))):nullptr;
            auto* soft=property?property->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(building):nullptr;
            if(!soft||soft->ObjectID.AssetPath.GetPackageName()==NAME_None)return nullptr;
            return ActorHelper::ResolveClass(soft->ObjectID.AssetPath.GetPackageName().ToString()
                +TEXT(".")+soft->ObjectID.AssetPath.GetAssetName().ToString());
        };
        auto* actorClass=resolveActorClass();
        auto* actorDefaults=actorClass?actorClass->GetClassDefaultObject().Get():nullptr;
        if(!actorClass||!actorDefaults)return fail("BuildableActor class/default object is unavailable");
        const auto resolveDerived=[&]() -> UObject* {
            for(const auto* field:{TEXT("BuildingPieceDerivedData"),TEXT("DerivedData")}) {
                auto* property=PropertyHelper::GetPropertyByName(building->GetClassPrivate(),field);
                if(auto* softProperty=CastField<FSoftObjectProperty>(property)) {
                    auto* soft=softProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(building);
                    if(soft&&soft->ObjectID.AssetPath.GetPackageName()!=NAME_None)
                        return LoadObject(soft->ObjectID.AssetPath.GetPackageName().ToString()
                            +TEXT(".")+soft->ObjectID.AssetPath.GetAssetName().ToString());
                }
                else if(auto* object=CastField<FObjectPropertyBase>(property))
                    if(auto* value=object->GetObjectPropertyValue(object->ContainerPtrToValuePtr<void>(building)))return value;
            }
            return nullptr;
        };
        const auto findComponent=[&](const TCHAR* path) -> UObject* {
            auto* componentClass=ActorHelper::ResolveClass(path);
            TArray<UObject*> components;
            if(componentClass)UECustom::UObjectGlobals::GetObjectsOfClass(componentClass,components,true);
            for(auto* candidate:components)if(candidate) {
                for(auto* outer=candidate->GetOuterPrivate();outer;outer=outer->GetOuterPrivate())
                    if(outer==actorClass||outer==actorDefaults)return candidate;
            }
            return nullptr;
        };
        const auto applyFields=[&](void* container,auto* type,const nlohmann::json& values,
            const std::unordered_set<std::string>& ignored={}) -> bool {
            for(const auto& [name,value]:values.items()) {
                if(ignored.contains(name))continue;
                auto* property=type?PropertyHelper::GetPropertyByName(type,RC::to_generic_string(name)):nullptr;
                if(!property)return false;
                PropertyHelper::CopyJsonValueToContainer(container,property,value);
            }
            return true;
        };

        std::string catalogue,world,interact,menu,stationRow;
        const auto& names=definition.Overrides.contains("Names")
            ?definition.Overrides.at("Names"):nlohmann::json::object();
        if(definition.Clone&&!definition.Properties.contains("BuildableActor")
            &&(names.contains("World")||names.contains("Interact")))
            return fail("World/Interact name overrides on a $Clone require a private cooked BuildableActor; sharing the vanilla actor would rename the source piece");
        if(names.contains("Catalogue")) {
            catalogue=names.at("Catalogue").get<std::string>();
            if(!writeText(building,TEXT("DisplayName"),catalogue))return fail("BuildingPieceData.DisplayName is unavailable");
        }
        if(names.contains("Interact")) {
            interact=names.at("Interact").get<std::string>();
            if(!writeText(actorDefaults,TEXT("InteractionName"),interact))return fail("BuildableActor InteractionName is unavailable");
        }
        if(names.contains("World")) {
            world=names.at("World").get<std::string>();
            bool written=false;
            for(const auto* field:{TEXT("WorldDisplayName"),TEXT("BuildingDisplayName"),TEXT("DisplayName")})
                if(writeText(actorDefaults,field,world)){written=true;break;}
            if(!written) {
                if(!catalogue.empty()&&catalogue!=world)return fail("World and Catalogue names differ but the actor has no distinct world-name field");
                if(!writeText(building,TEXT("DisplayName"),world))return fail("World name has no supported target");
            }
        }

        const auto& placement=definition.Overrides.contains("Placement")
            ?definition.Overrides.at("Placement"):nlohmann::json::object();
        std::string profile=placement.value("Profile",std::string{});
        if(placement.contains("RequiresFoundation")&&!placement.at("RequiresFoundation").get<bool>()&&profile.empty())profile="PropProfile";
        UObject* profileOwner=building;
        auto* placementHandle=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            building->GetClassPrivate(),TEXT("PlacementProfileRowHandle")));
        if(!placementHandle)if(auto* derived=resolveDerived()) {
            if(auto* candidate=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                derived->GetClassPrivate(),TEXT("PlacementProfileRowHandle")))) {
                profileOwner=derived;placementHandle=candidate;
            }
        }
        auto* placementHandleType=placementHandle?placementHandle->GetStruct().Get():nullptr;
        auto* placementRow=placementHandleType?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(placementHandleType,TEXT("RowName"))):nullptr;
        auto* placementTableField=placementHandleType?CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(placementHandleType,TEXT("DataTable"))):nullptr;
        auto* placementHandleData=placementHandle?placementHandle->ContainerPtrToValuePtr<void>(profileOwner):nullptr;
        auto* placementTableObject=placementTableField&&placementHandleData
            ?placementTableField->GetObjectPropertyValue(placementTableField->ContainerPtrToValuePtr<void>(placementHandleData)):nullptr;
        auto* placementTable=placementTableObject&&placementTableObject->IsA<UDataTable>()
            ?static_cast<UDataTable*>(placementTableObject):nullptr;
        const std::unordered_set<std::string> placementControl{
            "Profile","RequiresFoundation","RequiresRoof","RequiresShelter"};
        bool hasPlacementFields=false;
        for(const auto& [name,unused]:placement.items())
            if(!placementControl.contains(name)){hasPlacementFields=true;break;}
        if(!profile.empty()||hasPlacementFields) {
            if(!placementRow||!placementTable||!placementTable->GetRowStruct())
                return fail("placement profile handle is unavailable");
            FName sourceRow=profile.empty()
                ?placementRow->GetPropertyValue(placementRow->ContainerPtrToValuePtr<void>(placementHandleData))
                :FName(RC::to_generic_string(profile),FNAME_Find);
            auto* sourceData=sourceRow!=NAME_None?placementTable->FindRowUnchecked(sourceRow):nullptr;
            if(!sourceData)return fail("placement profile row '"+(profile.empty()?RC::to_string(sourceRow.ToString()):profile)+"' is unavailable");
            if(hasPlacementFields) {
                try {
                    auto* rowType=placementTable->GetRowStruct().Get();
                    FManagedStruct owned(rowType);
                    rowType->CopyScriptStruct(owned.GetData(),sourceData);
                    if(!applyFields(owned.GetData(),rowType,placement,placementControl))
                        return fail("placement profile layout differs from the mapped UE 5.6.1 fields");
                    const auto generated=OwnedProfileName("RS_PLACE",definition.Owner,definition.Key);
                    const FName generatedName(RC::to_generic_string(generated),FNAME_Add);
                    const auto ownershipKey=RC::to_string(placementTable->GetPathName())+":"+generated;
                    if(placementTable->FindRowUnchecked(generatedName)
                        &&!m_ownedProfileRows.contains(ownershipKey))
                        return fail("generated placement-profile row is already owned by external content");
                    placementTable->AddRow(generatedName,*reinterpret_cast<FTableRowBase*>(owned.GetData()));
                    m_ownedProfileRows.insert(ownershipKey);
                    placementRow->SetPropertyValue(placementRow->ContainerPtrToValuePtr<void>(placementHandleData),generatedName);
                    profile=generated;
                } catch(const std::exception& error) {
                    return fail("placement profile could not be written: "+std::string(error.what()));
                }
            } else {
                placementRow->SetPropertyValue(
                    placementRow->ContainerPtrToValuePtr<void>(placementHandleData),sourceRow);
            }
        }

        if(placement.contains("RequiresFoundation")&&placement.at("RequiresFoundation").get<bool>()&&profile.empty())
            return fail("RequiresFoundation=true needs an explicit verified placement profile");
        const bool roof=placement.contains("RequiresRoof"),shelter=placement.contains("RequiresShelter");
        const auto& derivedFields=definition.Overrides.contains("DerivedData")
            ?definition.Overrides.at("DerivedData"):nlohmann::json::object();
        if(!derivedFields.empty()) {
            auto* derived=resolveDerived();
            if(!derived)return fail("BuildingPieceDerivedData is unavailable");
            try {if(!applyFields(derived,derived->GetClassPrivate(),derivedFields))
                return fail("BuildingPieceDerivedData layout differs from the mapped UE 5.6.1 fields");}
            catch(const std::exception& error){return fail("derived placement data could not be written: "+std::string(error.what()));}
        }

        auto shelterFields=definition.Overrides.contains("Shelter")
            ?definition.Overrides.at("Shelter"):nlohmann::json::object();
        if(roof||shelter) {
            const bool requiresShelter=(roof&&placement.at("RequiresRoof").get<bool>())
                ||(shelter&&placement.at("RequiresShelter").get<bool>());
            shelterFields["InteractionRequirements"]=requiresShelter
                ?"EBuildingRequirements::InteractInShelterOnly"
                :"EBuildingRequirements::InteractAnywhere";
        }
        const auto actorComponentRequested=!shelterFields.empty()
            ||definition.Overrides.contains("Health")||definition.Overrides.contains("Snapping");
        if(actorComponentRequested&&definition.Clone&&!definition.Properties.contains("BuildableActor"))
            return fail("actor-component overrides on a $Clone require a private cooked BuildableActor; sharing the vanilla actor would alter the source piece");
        if(!shelterFields.empty()) {
            auto* component=findComponent(TEXT("/Script/Dominion.BuildingShelterComponent"));
            if(!component)return fail("BuildableActor has no BuildingShelterComponent");
            try {if(!applyFields(component,component->GetClassPrivate(),shelterFields))
                return fail("BuildingShelterComponent layout differs from the mapped UE 5.6.1 fields");}
            catch(const std::exception& error){return fail("shelter fields could not be written: "+std::string(error.what()));}
        }
        if(definition.Overrides.contains("Health")) {
            auto* component=findComponent(TEXT("/Script/Dominion.HealthComponent"));
            if(!component)return fail("BuildableActor has no HealthComponent");
            try {if(!applyFields(component,component->GetClassPrivate(),definition.Overrides.at("Health")))
                return fail("HealthComponent layout differs from the mapped UE 5.6.1 fields");}
            catch(const std::exception& error){return fail("health fields could not be written: "+std::string(error.what()));}
        }
        if(definition.Overrides.contains("Snapping")) {
            auto* component=findComponent(TEXT("/Script/Dominion.BuildingSnapComponent"));
            if(!component)return fail("BuildableActor has no BuildingSnapComponent");
            try {if(!applyFields(component,component->GetClassPrivate(),definition.Overrides.at("Snapping")))
                return fail("BuildingSnapComponent layout differs from the mapped UE 5.6.1 fields");}
            catch(const std::exception& error){return fail("snapping fields could not be written: "+std::string(error.what()));}
        }

        const auto& stability=definition.Overrides.contains("Stability")
            ?definition.Overrides.at("Stability"):nlohmann::json::object();
        if(!stability.empty()) {
            if(!EnsureStabilityProfile(building))return fail("stability profile table is unavailable");
            auto* handle=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                building->GetClassPrivate(),TEXT("BuildingStabilityProfileRowHandle")));
            auto* type=handle?handle->GetStruct().Get():nullptr;
            auto* row=type?CastField<FNameProperty>(PropertyHelper::GetPropertyByName(type,TEXT("RowName"))):nullptr;
            auto* tableField=type?CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(type,TEXT("DataTable"))):nullptr;
            auto* data=handle?handle->ContainerPtrToValuePtr<void>(building):nullptr;
            auto* tableObject=tableField&&data?tableField->GetObjectPropertyValue(tableField->ContainerPtrToValuePtr<void>(data)):nullptr;
            auto* table=tableObject&&tableObject->IsA<UDataTable>()?static_cast<UDataTable*>(tableObject):nullptr;
            const auto requested=stability.value("Profile",std::string{});
            const FName sourceRow=requested.empty()
                ?(row?row->GetPropertyValue(row->ContainerPtrToValuePtr<void>(data)):NAME_None)
                :FName(RC::to_generic_string(requested),FNAME_Find);
            auto* source=table&&sourceRow!=NAME_None?table->FindRowUnchecked(sourceRow):nullptr;
            if(!row||!table||!table->GetRowStruct()||!source)
                return fail("stability profile row '"+(requested.empty()?RC::to_string(sourceRow.ToString()):requested)+"' is unavailable");
            const bool hasFields=stability.size()>(stability.contains("Profile")?1u:0u);
            if(hasFields) {
                try {
                    auto* rowType=table->GetRowStruct().Get();
                    FManagedStruct owned(rowType);
                    rowType->CopyScriptStruct(owned.GetData(),source);
                    if(!applyFields(owned.GetData(),rowType,stability,{"Profile"}))
                        return fail("stability profile layout differs from the mapped UE 5.6.1 fields");
                    const auto generated=OwnedProfileName("RS_STABLE",definition.Owner,definition.Key);
                    const FName generatedName(RC::to_generic_string(generated),FNAME_Add);
                    const auto ownershipKey=RC::to_string(table->GetPathName())+":"+generated;
                    if(table->FindRowUnchecked(generatedName)
                        &&!m_ownedProfileRows.contains(ownershipKey))
                        return fail("generated stability-profile row is already owned by external content");
                    table->AddRow(generatedName,*reinterpret_cast<FTableRowBase*>(owned.GetData()));
                    m_ownedProfileRows.insert(ownershipKey);
                    row->SetPropertyValue(row->ContainerPtrToValuePtr<void>(data),generatedName);
                } catch(const std::exception& error) {
                    return fail("stability profile could not be written: "+std::string(error.what()));
                }
            } else {
                row->SetPropertyValue(row->ContainerPtrToValuePtr<void>(data),sourceRow);
            }
        }

        const auto& processing=definition.Overrides.contains("Processing")
            ?definition.Overrides.at("Processing"):nlohmann::json::object();
        const bool needsStation=names.contains("Menu")||!processing.empty();
        size_t matches=0;
        if(needsStation) {
            menu=names.value("Menu",std::string{});
            for(const auto* tableName:{"DT_CraftingStationsDataTable","DT_ProcessingStationDataTable"})
                for(auto* table:GetDatatablesByName(tableName)) {
                    if(!table||!table->GetRowStruct())continue;
                    auto* rowType=table->GetRowStruct().Get();
                    auto* stationProperty=PropertyHelper::GetPropertyByName(rowType,TEXT("StationBuildingPieceData"));
                    if(!stationProperty)continue;
                    for(const auto& [rowName,rowData]:table->GetRowMap()) {
                        bool same=false;
                        if(auto* soft=CastField<FSoftObjectProperty>(stationProperty))
                            same=SameSoftObject(*soft->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(rowData),building);
                        else if(auto* object=CastField<FObjectPropertyBase>(stationProperty))
                            same=object->GetObjectPropertyValue(object->ContainerPtrToValuePtr<void>(rowData))==building;
                        if(!same)continue;
                        ++matches;stationRow=RC::to_string(rowName.ToString());
                        const auto stationField=[&](const char* authored)->FProperty* {
                            const auto native=std::string_view(authored)=="Rate"?"ProcessingRate":
                                std::string_view(authored)=="IgnitesBurning"?"bIgnitesBurning":
                                std::string_view(authored)=="StopsWhenRecipeChanges"?"bStopsWhenRecipeChanges":
                                std::string_view(authored)=="CanProcessBeStartedThroughUI"?"bCanProcessBeStartedThroughUI":
                                std::string_view(authored)=="AutoStartProcess"?"bAutoStartProcess":authored;
                            return PropertyHelper::GetPropertyByName(rowType,RC::to_generic_string(native));
                        };
                        // Resolve every requested station field before applying
                        // any of this row's processing mutations.
                        for(const auto& [name,value]:processing.items())
                            if(name!="AcceptedFuels"&&!stationField(name.c_str()))
                                return fail("station row does not expose processing field '"+name+"'");
                        if(processing.contains("AcceptedFuels")
                            && !CastField<FArrayProperty>(stationField("AcceptedFuels")))
                            return fail("station row AcceptedFuels is unavailable or is not an array");
                        if(!menu.empty()) {
                            bool named=false;
                            for(const auto* field:{TEXT("DisplayName"),TEXT("StationDisplayName"),TEXT("Name")})
                                if(auto* text=CastField<FTextProperty>(PropertyHelper::GetPropertyByName(rowType,field))) {
                                    PropertyHelper::CopyJsonValueToContainer(rowData,text,nlohmann::json(menu));named=true;break;
                                }
                            if(!named) {
                                if(!catalogue.empty()&&catalogue!=menu)return fail("Menu and Catalogue names differ but the station row has no distinct menu-name field");
                                if(!writeText(building,TEXT("DisplayName"),menu))return fail("station menu name has no supported target");
                            }
                        }
                        if(processing.contains("Rate")) {
                            auto* rate=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(rowType,TEXT("ProcessingRate")));
                            if(!rate||!rate->IsFloatingPoint())return fail("station row has no floating-point ProcessingRate");
                            rate->SetFloatingPointPropertyValue(rate->ContainerPtrToValuePtr<void>(rowData),processing.at("Rate").get<double>());
                        }
                        if(processing.contains("AcceptedFuels")) {
                            const auto& rule=processing.at("AcceptedFuels");
                            auto* fuels=CastField<FArrayProperty>(stationField("AcceptedFuels"));
                            nlohmann::json values=rule.value("Items",nlohmann::json::array());
                            if(rule.at("Mode").get<std::string>()=="Append")
                                values=PropertyHelper::BuildAppendValue(fuels,values);
                            PropertyHelper::CopyJsonValueToContainer(rowData,fuels,values);
                        }
                        for(const auto& [name,value]:processing.items()) {
                            if(name=="Rate"||name=="AcceptedFuels")continue;
                            PropertyHelper::CopyJsonValueToContainer(rowData,stationField(name.c_str()),value);
                        }
                    }
                }
            if(matches!=1)return fail(matches?"building is linked by multiple station rows":"no station row references this building");
        }
        PS::Log<LogLevel::Normal>(STR("[BUILDING-OVERRIDE][OK] asset='{}' actor='{}' profile='{}' station='{}' names[catalogue='{}',world='{}',interact='{}',menu='{}'].\n"),
            building->GetPathName(),actorClass->GetPathName(),PS::ToWideSafe(profile.c_str()),PS::ToWideSafe(stationRow.c_str()),
            PS::ToWideSafe(catalogue.c_str()),PS::ToWideSafe(world.c_str()),PS::ToWideSafe(interact.c_str()),PS::ToWideSafe(menu.c_str()));
        return true;
    }

    void DragonWildsBuildingModLoader::DiscardUncommittedClone(UObject* building)
    {
        if (!building) return;
        const auto derivedName=building->GetName()+TEXT("_DerivedData");
        for(auto* object:m_createdBuildings)if(object&&object->GetName()==derivedName)
            object->ClearRootSet();
        std::erase_if(m_createdBuildings,[&](UObject* object){
            return object&&object->GetName()==derivedName;
        });
        building->ClearRootSet();
        std::erase(m_createdBuildings, building);
    }

    bool DragonWildsBuildingModLoader::ApplyRequirements(
        UObject* building, const BuildingDefinition& definition)
    {
        if (definition.Requirements.is_null())
        {
            return true;
        }

        auto* arrayProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                building->GetClassPrivate(), TEXT("Requirements")));
        auto* structProperty = arrayProperty
            ? CastField<FStructProperty>(arrayProperty->GetInner()) : nullptr;
        auto* requirementStruct = structProperty ? structProperty->GetStruct().Get() : nullptr;
        auto* amountProperty = requirementStruct ? CastField<FNumericProperty>(
            PropertyHelper::GetPropertyByName(requirementStruct, TEXT("Amount"))) : nullptr;
        auto* itemProperty = requirementStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(requirementStruct, TEXT("ItemData"))) : nullptr;
        if (!arrayProperty || !structProperty || !amountProperty || !itemProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': Requirements layout is incompatible with RuneSchema.\n"),
                definition.Key);
            return false;
        }

        struct ResolvedRequirement
        {
            UObject* Item = nullptr;
            int64_t Amount = 0;
        };
        std::vector<ResolvedRequirement> resolved;
        resolved.reserve(definition.Requirements.size());

        for (const auto& requirement : definition.Requirements)
        {
            const auto reference = RC::to_generic_string(
                requirement.at("ItemData").get<std::string>());
            auto* item = ResolveItem(reference);
            if (!item)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{}': requirement item '{}' could not be resolved.\n"),
                    definition.Key, reference);
                return false;
            }

            resolved.push_back({
                item,
                requirement.at("Amount").get<int64_t>()
            });
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(building);
        UECustom::FScriptArrayHelper helper(array, arrayProperty);
        helper.Empty();

        for (const auto& requirement : resolved)
        {
            UECustom::FManagedValue value;
            helper.InitializeValue(value);
            amountProperty->SetIntPropertyValue(
                amountProperty->ContainerPtrToValuePtr<void>(value.GetData()),
                requirement.Amount);
            auto* itemAddress = itemProperty->ContainerPtrToValuePtr<void>(value.GetData());
            std::memcpy(itemAddress, &requirement.Item, sizeof(requirement.Item));
            helper.Add(value);
        }

        return true;
    }

    bool DragonWildsBuildingModLoader::EnsureStabilityProfile(UObject* building)
    {
        auto* handleProperty = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                building->GetClassPrivate(), TEXT("BuildingStabilityProfileRowHandle")));
        auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
        auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = handleStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
        if (!handleProperty || !tableProperty || !rowProperty)
        {
            return false;
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(building);
        auto* tableAddress = tableProperty->ContainerPtrToValuePtr<void>(handle);
        UObject* currentTable = nullptr;
        std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        if (!currentTable)
        {
            auto* table = LoadObject(StabilityProfilePath);
            if (!table || !table->IsA(UDataTable::StaticClass()))
            {
                return false;
            }

            std::memcpy(tableAddress, &table, sizeof(table));
            currentTable = nullptr;
            std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        }

        if (!currentTable || !currentTable->IsA(UDataTable::StaticClass()))
        {
            return false;
        }

        const auto rowName = rowProperty->GetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(handle));
        auto* table = static_cast<UDataTable*>(currentTable);
        if (rowName == NAME_None || !table->FindRowUnchecked(rowName))
        {
            PS::Log<LogLevel::Error>(
                STR("[BUILDING-STABILITY][MISSING] '{}' references row '{}' in '{}', but that row was not registered. Add the row to the vanilla DT_StabilityProfile table through /raw before /buildings loads.\n"),
                building->GetName(), rowName.ToString(), table->GetPathName());
            return false;
        }

        return true;
    }

    bool DragonWildsBuildingModLoader::AddPersistenceIdentity(UObject* building)
    {
        auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            building->GetClassPrivate(), TEXT("PersistenceID")));
        auto* setProperty = m_catalogue ? CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("AllPiecesInCatalogue"))) : nullptr;
        if (!idProperty || !setProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}' has no usable persistence identity.\n"),
                building->GetName());
            return false;
        }

        auto persistenceId = idProperty->GetPropertyValue(
            idProperty->ContainerPtrToValuePtr<void>(building));
        if (persistenceId.GetCharArray().Num() <= 1)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}' has an empty PersistenceID.\n"),
                building->GetName());
            return false;
        }

        UECustom::FScriptSetHelper set(
            setProperty, setProperty->ContainerPtrToValuePtr<void>(m_catalogue));
        set.Add(&persistenceId);
        return true;
    }

    bool DragonWildsBuildingModLoader::ProtectWorldRegistry(UObject* subsystem)
    {
        if (!subsystem) return false;
        auto* subsystemClass = subsystem->GetClassPrivate();
        auto* arrayProperty = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("NetIdToData")));
        auto* reverseProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("DataToNetIdMap")));
        auto* persistenceMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("PersistenceIDToDataMap")));
        auto* internalMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("InternalNameToDataMap")));
        auto* arrayInner = arrayProperty
            ? CastField<FObjectPropertyBase>(arrayProperty->GetInner()) : nullptr;
        if (!arrayProperty || !arrayInner || arrayInner->GetElementSize() != sizeof(UObject*)
            || !reverseProperty || !persistenceMapProperty || !internalMapProperty)
            return false;

        struct ActiveDefinition {
            UObject* Object = nullptr;
            std::string PersistenceId;
            std::string InternalName;
        };
        std::vector<ActiveDefinition> active;
        std::unordered_set<std::string> activeIds;
        std::unordered_set<UObject*> activeObjects;
        for (const auto& definition : m_definitions)
        {
            const auto identity = Identity(definition.Owner, definition.Key);
            if (!m_applied.contains(identity)) continue;
            auto* object = GetValidBuilding(identity);
            auto* idProperty = object ? CastField<FStrProperty>(
                PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("PersistenceID"))) : nullptr;
            auto* nameProperty = object ? CastField<FStrProperty>(
                PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("InternalName"))) : nullptr;
            if (!object || !idProperty || !nameProperty) return false;
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto nameValue = nameProperty->GetPropertyValue(
                nameProperty->ContainerPtrToValuePtr<void>(object));
            const auto id = RC::to_string(*idValue);
            if (id.empty() || !activeIds.insert(id).second
                || !activeObjects.insert(object).second)
            {
                PS::Log<LogLevel::Error>(STR(
                    "Buildings contain a duplicate or empty PersistenceID; deterministic registration aborted.\n"));
                return false;
            }
            active.push_back({object, id, RC::to_string(*nameValue)});
        }
        std::sort(active.begin(), active.end(),
            [](const auto& left, const auto& right) {
                return left.PersistenceId < right.PersistenceId;
            });

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        std::vector<UObject*> vanilla;
        std::unordered_set<std::string> nativeIds;
        for (int32 index = 0; index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object,
                static_cast<uint8*>(array->GetData())
                    + index * arrayInner->GetElementSize(), sizeof(object));
            auto* idProperty = object ? CastField<FStrProperty>(
                PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("PersistenceID"))) : nullptr;
            if (!object || !idProperty) return false;
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto id = RC::to_string(*idValue);
            if (id.empty() || !nativeIds.insert(id).second)
            {
                PS::Log<LogLevel::Error>(STR(
                    "The native Building registry contains a duplicate or empty PersistenceID.\n"));
                return false;
            }
            if (!activeIds.contains(id) && !activeObjects.contains(object))
                vanilla.push_back(object);
        }

        std::vector<UObject*> desired = vanilla;
        desired.reserve(vanilla.size() + active.size());
        for (const auto& definition : active)
            desired.push_back(definition.Object);
        if (desired.size() > std::numeric_limits<uint16>::max())
        {
            PS::Log<LogLevel::Error>(STR(
                "Building registry exceeds the native network index limit.\n"));
            return false;
        }
        if (!CaptureNativeRegistry(subsystem)) return false;

        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        arrayHelper.Empty();
        for (auto* object : desired)
        {
            UECustom::FManagedValue value;
            arrayHelper.InitializeValue(value);
            std::memcpy(value.GetData(), &object, sizeof(object));
            arrayHelper.Add(value);
        }

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        std::vector<UObject*> reverseKeys;
        reverse.ForEachPair([&](void* key, void*) {
            UObject* object = nullptr;
            std::memcpy(&object, key, sizeof(object));
            reverseKeys.push_back(object);
        });
        for (auto iterator = reverseKeys.rbegin(); iterator != reverseKeys.rend(); ++iterator)
        {
            auto* object = *iterator;
            reverse.Remove(&object);
        }

        const auto addStringMap = [&](FMapProperty* property,
            const FString& key, UObject* object) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            UECustom::FManagedValue pair;
            map.InitializePair(pair);
            *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
            std::memcpy(map.GetValuePtr(pair.GetData()), &object, sizeof(object));
            map.Add(pair);
            map.Rehash();
        };

        for (int32 index = 0; index < static_cast<int32>(desired.size()); ++index)
        {
            auto* object = desired[index];
            const auto netIndex = static_cast<uint16>(index);
            UECustom::FManagedValue pair;
            reverse.InitializePair(pair);
            std::memcpy(reverse.GetKeyPtr(pair.GetData()), &object, sizeof(object));
            std::memcpy(reverse.GetValuePtr(pair.GetData()), &netIndex, sizeof(netIndex));
            reverse.Add(pair);

            auto* indexProperty = CastField<FNumericProperty>(
                PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            if (!indexProperty) return false;
            indexProperty->SetIntPropertyValue(
                indexProperty->ContainerPtrToValuePtr<void>(object),
                static_cast<int64>(index));
        }
        reverse.Rehash();

        for (const auto& definition : active)
        {
            const FString id(RC::to_generic_string(definition.PersistenceId).c_str());
            addStringMap(persistenceMapProperty, id, definition.Object);
            addStringMap(internalMapProperty, id, definition.Object);
            if (!definition.InternalName.empty())
            {
                const FString name(
                    RC::to_generic_string(definition.InternalName).c_str());
                addStringMap(internalMapProperty, name, definition.Object);
            }
        }

        bool valid = array->Num() == static_cast<int32>(desired.size());
        for (int32 index = 0; valid && index < array->Num(); ++index)
        {
            UObject* forward = nullptr;
            std::memcpy(&forward,
                static_cast<uint8*>(array->GetData())
                    + index * arrayInner->GetElementSize(), sizeof(forward));
            int32 reverseIndex = -1;
            reverse.ForEachPair([&](void* key, void* value) {
                UObject* candidate = nullptr;
                std::memcpy(&candidate, key, sizeof(candidate));
                if (candidate == desired[index])
                {
                    uint16 found = 0;
                    std::memcpy(&found, value, sizeof(found));
                    reverseIndex = found;
                }
            });
            auto* indexProperty = CastField<FNumericProperty>(
                PropertyHelper::GetPropertyByName(
                    desired[index]->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            const auto reported = indexProperty ? static_cast<int32>(
                indexProperty->GetSignedIntPropertyValue(
                    indexProperty->ContainerPtrToValuePtr<void>(desired[index]))) : -1;
            valid = forward == desired[index] && reverseIndex == index
                && reported == index;
        }
        if (!valid)
        {
            PS::Log<LogLevel::Error>(STR(
                "Deterministic Building registry validation failed.\n"));
            return false;
        }

        PS::Log<LogLevel::Normal>(STR(
            "[REGISTRY][BUILDING][ADDED] count={} native={} deterministicPersistenceOrder=true verified=true.\n"),
            active.size(), vanilla.size());
        return true;
    }
    std::vector<DragonWildsBuildingModLoader::Placement>
    DragonWildsBuildingModLoader::FindSourcePlacements(UObject* source) const
    {
        std::vector<Placement> result;
        if (!source || !m_catalogue) return result;
        auto* pagesProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("Pages")));
        auto* pageProperty = pagesProperty
            ? CastField<FStructProperty>(pagesProperty->GetInner()) : nullptr;
        auto* pageType = pageProperty ? pageProperty->GetStruct().Get() : nullptr;
        auto* collectionsProperty = pageType ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(pageType, TEXT("Collection"))) : nullptr;
        auto* collectionProperty = collectionsProperty
            ? CastField<FStructProperty>(collectionsProperty->GetInner()) : nullptr;
        auto* collectionType = collectionProperty
            ? collectionProperty->GetStruct().Get() : nullptr;
        auto* labelProperty = collectionType ? PropertyHelper::GetPropertyByName(
            collectionType, TEXT("Label")) : nullptr;
        auto* piecesProperty = collectionType ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(collectionType, TEXT("Collection"))) : nullptr;
        if (!pagesProperty || !pageProperty || !collectionsProperty
            || !collectionProperty || !labelProperty || !piecesProperty
            || !CastField<FSoftObjectProperty>(piecesProperty->GetInner())
            || piecesProperty->GetInner()->GetElementSize()
                != sizeof(UECustom::FSoftObjectPtr))
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue layout cannot be scanned for source placement.\n"));
            return result;
        }

        auto* pages = pagesProperty->ContainerPtrToValuePtr<FScriptArray>(m_catalogue);
        const auto pageSize = pageProperty->GetElementSize();
        const auto collectionSize = collectionProperty->GetElementSize();
        const auto pieceSize = piecesProperty->GetInner()->GetElementSize();
        for (int32 pageIndex = 0; pageIndex < pages->Num(); ++pageIndex)
        {
            auto* page = static_cast<uint8*>(pages->GetData()) + pageIndex * pageSize;
            auto* collections = collectionsProperty->ContainerPtrToValuePtr<FScriptArray>(page);
            for (int32 collectionIndex = 0;
                collectionIndex < collections->Num(); ++collectionIndex)
            {
                auto* collection = static_cast<uint8*>(collections->GetData())
                    + collectionIndex * collectionSize;
                auto* pieces = piecesProperty->ContainerPtrToValuePtr<FScriptArray>(collection);
                bool found = false;
                for (int32 pieceIndex = 0; pieceIndex < pieces->Num(); ++pieceIndex)
                {
                    auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(
                        static_cast<uint8*>(pieces->GetData()) + pieceIndex * pieceSize);
                    if (SameSoftObject(*soft, source)) { found = true; break; }
                }
                if (!found) continue;
                auto* label = labelProperty->ContainerPtrToValuePtr<FText>(collection);
                if (!label) continue;
                Placement placement{};
                placement.PageIndex = pageIndex;
                placement.Collection = PropertyHelper::GetTextAsString(*label);
                result.push_back(std::move(placement));
            }
        }
        return result;
    }

    bool DragonWildsBuildingModLoader::AddToMenu(
        UObject* building, const Placement& placement)
    {
        auto* pagesProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("Pages")));
        auto* pageProperty =
            pagesProperty ? CastField<FStructProperty>(pagesProperty->GetInner()) : nullptr;
        if (!pagesProperty || !pageProperty || !pageProperty->GetStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue Pages layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* pages =
            pagesProperty->ContainerPtrToValuePtr<FScriptArray>(m_catalogue);
        if (!pages->IsValidIndex(placement.PageIndex))
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': menu page {} does not exist.\n"),
                building->GetName(), placement.PageIndex);
            return false;
        }

        auto* pageData = static_cast<uint8*>(pages->GetData())
            + placement.PageIndex * pageProperty->GetElementSize();
        auto* collectionsProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                pageProperty->GetStruct().Get(), TEXT("Collection")));
        auto* collectionProperty = collectionsProperty
            ? CastField<FStructProperty>(collectionsProperty->GetInner())
            : nullptr;
        if (!collectionsProperty || !collectionProperty || !collectionProperty->GetStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue Collection layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* labelProperty = PropertyHelper::GetPropertyByName(
            collectionProperty->GetStruct().Get(), TEXT("Label"));
        auto* piecesProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                collectionProperty->GetStruct().Get(), TEXT("Collection")));
        if (!labelProperty || !piecesProperty
            || !CastField<FSoftObjectProperty>(piecesProperty->GetInner())
            || piecesProperty->GetInner()->GetElementSize()
                != sizeof(UECustom::FSoftObjectPtr))
        {
            PS::Log<LogLevel::Error>(
                STR("Build catalogue item layout is incompatible with RuneSchema.\n"));
            return false;
        }

        auto* collections =
            collectionsProperty->ContainerPtrToValuePtr<FScriptArray>(pageData);
        const auto collectionSize = collectionProperty->GetElementSize();
        int32 collectionIndex = -1;

        for (int32 index = 0; index < collections->Num(); ++index)
        {
            auto* collection =
                static_cast<uint8*>(collections->GetData()) + index * collectionSize;
            auto* label = labelProperty->ContainerPtrToValuePtr<FText>(collection);
            if (label
                && PropertyHelper::GetTextAsString(*label) == placement.Collection)
            {
                collectionIndex = index;
                break;
            }
        }

        if (collectionIndex < 0)
        {
            UECustom::FScriptArrayHelper helper(collections, collectionsProperty);
            UECustom::FManagedValue value;
            helper.InitializeValue(value);
            PropertyHelper::CopyJsonValueToContainer(
                value.GetData(), labelProperty, RC::to_string(placement.Collection));
            collectionIndex = collections->Num();
            helper.Add(value);
        }

        auto* collection =
            static_cast<uint8*>(collections->GetData()) + collectionIndex * collectionSize;
        auto* pieces =
            piecesProperty->ContainerPtrToValuePtr<FScriptArray>(collection);
        const auto elementSize = piecesProperty->GetInner()->GetElementSize();

        for (int32 index = 0; index < pieces->Num(); ++index)
        {
            auto* soft = reinterpret_cast<UECustom::FSoftObjectPtr*>(
                static_cast<uint8*>(pieces->GetData()) + index * elementSize);
            if (SameSoftObject(*soft, building))
            {
                return true;
            }
        }

        UECustom::FScriptArrayHelper helper(pieces, piecesProperty);
        UECustom::FManagedValue value;
        helper.InitializeValue(value);
        InitializeSoftObject(value.GetData(), building);
        helper.Add(value);
        return true;
    }

    void DragonWildsBuildingModLoader::RegisterHooks()
    {
        if (m_hooksRegistered)
        {
            return;
        }

        for (auto* path : UnlockHookPaths)
        {
            auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, path);
            if (!function)
            {
                PS::Log<LogLevel::Warning>(
                    STR("Building unlock hook '{}' was not found.\n"), path);
                continue;
            }

            PS::RegisterNativePostHook(function,
                [](UnrealScriptFunctionCallableContext& context, void* customData) {
                    static_cast<DragonWildsBuildingModLoader*>(customData)
                        ->TryWorldRecovery(context.Context, "progress-callback");
                },
                this);
        }

        if (auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
                nullptr, nullptr, PlayerRestartPath))
        {
            PS::RegisterNativePostHook(function,
                [](UnrealScriptFunctionCallableContext& context, void* customData) {
                    static_cast<DragonWildsBuildingModLoader*>(customData)
                        ->TryWorldRecovery(context.Context, "player-restart");
                },
                this);
        }
        else
        {
            PS::Log<LogLevel::Warning>(STR(
                "Building late-world fallback hook '{}' was not found.\n"),
                PlayerRestartPath);
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("BuildingLoaderInitGameState");
        m_initGameStateCallbackId = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* gameMode) {
                ScheduleWorldRecovery(gameMode);
                try
                {
                    if (!EnsureWorldState(gameMode))
                        m_lastRecoveryFailure = "native building registry was not ready";
                }
                catch (const std::exception& error)
                {
                    m_lastRecoveryFailure = error.what();
                    PS::Log<LogLevel::Warning>(STR(
                        "[BUILDING-RECOVERY][DEFERRED] Pre-initialization registration raised '{}'; bounded recovery remains armed.\n"),
                        PS::ToWideSafe(error.what()));
                }
                catch (...)
                {
                    m_lastRecoveryFailure = "unknown pre-initialization exception";
                    PS::Log<LogLevel::Warning>(STR(
                        "[BUILDING-RECOVERY][DEFERRED] Pre-initialization registration raised an unknown exception; bounded recovery remains armed.\n"));
                }
            },
            options);

        if (m_initGameStateCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Warning>(
                STR("Building world initialization callback could not be registered.\n"));
            return;
        }

        Hook::FCallbackOptions unlockOptions{};
        unlockOptions.OwnerModName = TEXT("RuneSchema");
        unlockOptions.HookName = TEXT("BuildingLoaderUnlockInitGameState");
        m_unlockGameStateCallbackId = Hook::RegisterInitGameStatePostCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* gameMode) {
                TryWorldRecovery(gameMode, "world-ready");
            },
            unlockOptions);
        if (m_unlockGameStateCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Warning>(
                STR("Building post-initialization unlock callback could not be registered; native progress callbacks remain active.\n"));
        }

        Hook::FCallbackOptions retryOptions{};
        retryOptions.OwnerModName = TEXT("RuneSchema");
        retryOptions.HookName = TEXT("BuildingLoaderBoundedRecovery");
        m_recoveryTickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*,
                float deltaSeconds, bool) {
                if (m_pendingWorldContext.Get()) RetryWorldRecovery(deltaSeconds);
            },
            retryOptions);
        if (m_recoveryTickCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Warning>(STR(
                "Building bounded recovery could not be scheduled; normal world and player callbacks remain active.\n"));
        }

        m_hooksRegistered = true;
    }

    bool DragonWildsBuildingModLoader::PrepareWorldState(UObject* worldContext)
    {
        // The frontend may unload/reload package-owned assets before this
        // callback. Re-resolve both catalogue and direct definitions before
        // touching any reflected field.
        m_catalogue=LoadObject(CataloguePath);
        if(!m_catalogue||!RefreshBuildingReferencesForWorld()||!RefreshBuildingCatalogueForWorld())
        {
            PS::Log<LogLevel::Error>(STR("Buildings cannot be registered because one or more retained assets failed world-boundary validation.\n"));
            return false;
        }

        auto* subsystem = FindBuildingSubsystem(worldContext);
        auto* arrayProperty = subsystem ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                subsystem->GetClassPrivate(), TEXT("NetIdToData"))) : nullptr;
        auto* setProperty = m_catalogue ? CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                m_catalogue->GetClassPrivate(), TEXT("AllPiecesInCatalogue"))) : nullptr;
        if (!subsystem || !arrayProperty || !setProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings cannot be registered because the native registry is unavailable.\n"));
            return false;
        }

        // InitGameState can run again against the same persistent subsystem
        // after a frontend/world transition. Restore its native baseline before
        // taking a fresh snapshot; otherwise the previous protected state is
        // mistaken for an unsafe double reconstruction and registration aborts.
        if (m_nativeRegistrySnapshot.Subsystem == subsystem)
        {
            if (!RestoreNativeRegistry())
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry refresh could not restore its prior native baseline.\n"));
                return false;
            }
            PS::Log<LogLevel::Normal>(
                STR("[BUILDING-REGISTRY][REFRESH] Restored the persistent subsystem baseline before reconstruction.\n"));
        }
        else if (m_nativeRegistrySnapshot.Subsystem)
        {
            ClearWorldRegistryState();
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        auto* catalogueIds = setProperty->ContainerPtrToValuePtr<FScriptSet>(m_catalogue);
        if (array->Num() < 700 || catalogueIds->Num() < 700)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings were not registered because the native registry is incomplete.\n"));
            return false;
        }

        bool protectedRegistry = false;
        try
        {
            protectedRegistry = ProtectWorldRegistry(subsystem);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("Deterministic Building registry registration raised an error: {}\n"),
                PS::ToWideSafe(error.what()));
        }
        if (!protectedRegistry)
        {
            if (m_nativeRegistrySnapshot.Subsystem)
            {
                if (!RestoreNativeRegistry())
                {
                    PS::Log<LogLevel::Error>(
                        STR("Custom Building registry rollback failed; transient state was retained.\n"));
                }
            }
            else
            {
                ClearWorldRegistryState();
            }

            PS::Log<LogLevel::Error>(
                STR("Deterministic Building registry registration failed; Building registration was aborted.\n"));
            return false;
        }

        // Registry reconstruction belongs to the pre-initialization lane.
        // Player progress components are selected and updated by the matching
        // post callback, after the new world has finished replacing any stale
        // frontend/previous-world components.
        return true;
    }

    bool DragonWildsBuildingModLoader::EnsureWorldState(UObject* worldContext)
    {
        if (!worldContext || !worldContext->GetWorld()) return false;

        if (auto* registered = m_registeredWorldContext.Get();
            m_worldRegistryReady
            && registered
            && registered->GetWorld() == worldContext->GetWorld())
            return true;

        m_worldRegistryReady = false;
        if (!PrepareWorldState(worldContext)) return false;
        m_registeredWorldContext.Assign(worldContext);
        m_worldRegistryReady = true;
        return true;
    }

    void DragonWildsBuildingModLoader::ScheduleWorldRecovery(UObject* worldContext)
    {
        if (!worldContext || !worldContext->GetWorld()) return;
        if (IsFrontEndWorld(worldContext->GetWorld()))
        {
            // The menu has no live player progress. Cancel any prior world's
            // retry instead of reporting an expected absence as a timeout.
            m_pendingWorldContext.Reset();
            m_recoveryElapsed = 0.0f;
            m_recoveryInterval = 0.0f;
            m_lastRecoveryFailure.clear();
            return;
        }

        auto* pending = m_pendingWorldContext.Get();
        if (pending && pending->GetWorld() == worldContext->GetWorld()) return;

        auto* registered = m_registeredWorldContext.Get();
        if (!registered || registered->GetWorld() != worldContext->GetWorld())
            m_worldRegistryReady = false;

        m_pendingWorldContext.Assign(worldContext);
        m_recoveryElapsed = 0.0f;
        m_recoveryInterval = 0.0f;
        m_lastRecoveryFailure.clear();
    }

    bool DragonWildsBuildingModLoader::TryWorldRecovery(
        UObject* worldContext, const char* source)
    {
        if (!worldContext || !worldContext->GetWorld()) return false;
        if (IsFrontEndWorld(worldContext->GetWorld())) return true;
        ScheduleWorldRecovery(worldContext);

        try
        {
            if (!EnsureWorldState(worldContext))
            {
                m_lastRecoveryFailure = "native building registry was not ready";
                return false;
            }

            if (m_unlocks.empty())
            {
                m_pendingWorldContext.Reset();
                m_recoveryElapsed = 0.0f;
                m_recoveryInterval = 0.0f;
                m_lastRecoveryFailure.clear();
                return true;
            }

            const bool reportDeferred = !source
                || std::strcmp(source, "bounded-retry") != 0;
            if (!ApplyUnlocksToWorld(worldContext, reportDeferred))
            {
                if (PS::Storefront::IsDedicatedServer()
                    && source && std::strcmp(source, "world-ready") == 0)
                {
                    // A headless world normally has no player ProgressComponent
                    // during InitGameState. The native progress and ClientRestart
                    // callbacks will re-arm recovery when a player actually joins.
                    m_pendingWorldContext.Reset();
                    m_recoveryElapsed = 0.0f;
                    m_recoveryInterval = 0.0f;
                    m_lastRecoveryFailure.clear();
                    PS::Log<LogLevel::Normal>(STR(
                        "[SERVER][BUILDING-UNLOCK][AWAITING-PLAYER] World registry is ready; unlock delivery will begin when a player ProgressComponent becomes available.\n"));
                    return true;
                }
                m_lastRecoveryFailure = "live player ProgressComponent was not ready";
                return false;
            }

            const bool recoveredLate = m_recoveryElapsed > 0.0f
                || (source && std::strcmp(source, "world-ready") != 0);
            m_pendingWorldContext.Reset();
            m_recoveryElapsed = 0.0f;
            m_recoveryInterval = 0.0f;
            m_lastRecoveryFailure.clear();
            if (recoveredLate)
            {
                PS::Log<LogLevel::Normal>(STR(
                    "[BUILDING-RECOVERY][VERIFIED] source='{}' registry=true unlocks=true; bounded recovery stopped.\n"),
                    PS::ToWideSafe(source ? source : "unknown"));
            }
            return true;
        }
        catch (const std::exception& error)
        {
            m_lastRecoveryFailure = error.what();
            if (!source || std::strcmp(source, "bounded-retry") != 0)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "[BUILDING-RECOVERY][DEFERRED] source='{}' raised '{}'; bounded recovery remains armed.\n"),
                    PS::ToWideSafe(source ? source : "unknown"),
                    PS::ToWideSafe(error.what()));
            }
        }
        catch (...)
        {
            m_lastRecoveryFailure = "unknown world recovery exception";
            if (!source || std::strcmp(source, "bounded-retry") != 0)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "[BUILDING-RECOVERY][DEFERRED] source='{}' raised an unknown exception; bounded recovery remains armed.\n"),
                    PS::ToWideSafe(source ? source : "unknown"));
            }
        }
        return false;
    }

    void DragonWildsBuildingModLoader::RetryWorldRecovery(float deltaSeconds)
    {
        auto* worldContext = m_pendingWorldContext.Get();
        if (!worldContext)
        {
            m_pendingWorldContext.Reset();
            return;
        }

        const auto elapsed = std::max(0.0f, deltaSeconds);
        m_recoveryElapsed += elapsed;
        m_recoveryInterval += elapsed;
        if (m_recoveryInterval < BuildingRecoveryIntervalSeconds) return;
        m_recoveryInterval = 0.0f;

        if (TryWorldRecovery(worldContext, "bounded-retry")) return;
        if (m_recoveryElapsed < BuildingRecoveryTimeoutSeconds) return;

        PS::Log<LogLevel::Warning>(STR(
            "[BUILDING-RECOVERY][TIMEOUT] Recovery stopped after {} seconds: {}. Buildings remain isolated; other RuneSchema systems continue.\n"),
            BuildingRecoveryTimeoutSeconds,
            PS::ToWideSafe(m_lastRecoveryFailure.empty()
                ? "world readiness could not be verified"
                : m_lastRecoveryFailure.c_str()));
        m_pendingWorldContext.Reset();
        m_recoveryElapsed = 0.0f;
        m_recoveryInterval = 0.0f;
    }

    bool DragonWildsBuildingModLoader::RefreshBuildingCatalogueForWorld()
    {
        if(!m_catalogue)return false;
        for(const auto& definition:m_definitions) {
            const auto identity=Identity(definition.Owner,definition.Key);
            if(!m_applied.contains(identity))continue;
            auto* building=GetValidBuilding(identity);
            if(!building) {
                PS::Log<LogLevel::Error>(STR("[BUILDING-CATALOGUE][FAILED] owner='{}' key='{}' has no valid object.\n"),
                    definition.Owner,RC::to_generic_string(definition.Key));
                return false;
            }
            if(!AddPersistenceIdentity(building))return false;
            auto placements=definition.InheritSourcePlacement
                ? FindSourcePlacements(building) : definition.Targets;
            if(placements.empty()&&!definition.Declared) {
                PS::Log<LogLevel::Error>(STR("[BUILDING-CATALOGUE][FAILED] owner='{}' key='{}' has no placement.\n"),
                    definition.Owner,RC::to_generic_string(definition.Key));
                return false;
            }
            for(const auto& placement:placements)if(!AddToMenu(building,placement))return false;
            PS::Log<LogLevel::Verbose>(STR("[BUILDING-CATALOGUE][VERIFIED] owner='{}' key='{}' placements={} object='{}'.\n"),
                definition.Owner,RC::to_generic_string(definition.Key),placements.size(),building->GetPathName());
        }
        return true;
    }

    bool DragonWildsBuildingModLoader::ApplyUnlocks(UObject* progressComponent)
    {
        if (!progressComponent || m_unlocks.empty())
        {
            return false;
        }

        std::vector<UObject*> buildings;
        for (const auto& key : m_unlocks)
        {
            if(auto* building=GetValidBuilding(key))buildings.push_back(building);
        }
        if (buildings.empty()) return false;

        auto* unlockedProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                progressComponent->GetClassPrivate(), TEXT("BuildingsUnlocked")));
        std::vector<UObject*> newlyUnlocked;
        if (unlockedProperty
            && CastField<FObjectProperty>(unlockedProperty->GetInner()))
        {
            auto* unlocked =
                unlockedProperty->ContainerPtrToValuePtr<FScriptArray>(progressComponent);
            const auto elementSize = unlockedProperty->GetInner()->GetElementSize();
            UECustom::FScriptArrayHelper helper(unlocked, unlockedProperty);

            for (auto* building : buildings)
            {
                bool exists = false;
                for (int32 index = 0; index < unlocked->Num(); ++index)
                {
                    UObject* current = nullptr;
                    std::memcpy(
                        &current,
                        static_cast<uint8*>(unlocked->GetData()) + index * elementSize,
                        sizeof(current));
                    if (current == building)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    UECustom::FManagedValue value;
                    helper.InitializeValue(value);
                    std::memcpy(value.GetData(), &building, sizeof(building));
                    helper.Add(value);
                    newlyUnlocked.push_back(building);
                }
            }
        }

        auto* sessionOnlyProperty = CastField<FSetProperty>(
            PropertyHelper::GetPropertyByName(
                progressComponent->GetClassPrivate(),
                TEXT("BuildingsUnlockedThatShouldNotPersist")));
        if (sessionOnlyProperty)
        {
            UECustom::FScriptSetHelper helper(
                sessionOnlyProperty,
                sessionOnlyProperty->ContainerPtrToValuePtr<void>(progressComponent));
            for (auto* building : buildings)
            {
                helper.Add(&building);
            }
        }

        size_t visible = 0;
        if (unlockedProperty)
        {
            auto* unlocked =
                unlockedProperty->ContainerPtrToValuePtr<FScriptArray>(progressComponent);
            const auto elementSize = unlockedProperty->GetInner()->GetElementSize();
            for (auto* building : buildings)
            {
                for (int32 index = 0; index < unlocked->Num(); ++index)
                {
                    UObject* current = nullptr;
                    std::memcpy(&current,
                        static_cast<uint8*>(unlocked->GetData()) + index * elementSize,
                        sizeof(current));
                    if (current == building)
                    {
                        ++visible;
                        break;
                    }
                }
            }
        }
        size_t sessionOnly = 0;
        if (sessionOnlyProperty)
        {
            UECustom::FScriptSetHelper helper(
                sessionOnlyProperty,
                sessionOnlyProperty->ContainerPtrToValuePtr<void>(progressComponent));
            for (auto* building : buildings)
                if (helper.Contains(&building)) ++sessionOnly;
        }

        if (visible != buildings.size() || sessionOnly != buildings.size())
        {
            PS::Log<LogLevel::Error>(STR(
                "[BUILDING-UNLOCK][FAILED] component='{}' requested={} visible={} session_only={}.\n"),
                progressComponent->GetPathName(), buildings.size(), visible, sessionOnly);
            return false;
        }

        if (!newlyUnlocked.empty())
        {
            try
            {
                NotifyBuildingUnlocks(progressComponent, newlyUnlocked);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Building unlock state was verified, but its UI notification raised '{}'.\n"),
                    PS::ToWideSafe(error.what()));
            }
            catch (...)
            {
                PS::Log<LogLevel::Warning>(STR(
                    "Building unlock state was verified, but its UI notification raised an unknown exception.\n"));
            }
        }

        if (!newlyUnlocked.empty())
        {
            PS::Log<LogLevel::Normal>(STR(
                "[BUILDING-UNLOCK][VERIFIED] world='{}' component='{}' requested={} newly_visible={}.\n"),
                progressComponent->GetWorld() ? TEXT("active") : TEXT("<none>"),
                progressComponent->GetPathName(), buildings.size(), newlyUnlocked.size());
        }
        else
        {
            PS::Log<LogLevel::Verbose>(STR(
                "[BUILDING-UNLOCK][UNCHANGED] component='{}' already contains all {} requested building(s).\n"),
                progressComponent->GetPathName(), buildings.size());
        }
        return true;
    }

    size_t DragonWildsBuildingModLoader::ApplyUnlocksToWorld(
        UObject* worldContext, bool reportDeferred)
    {
        if (!m_progressComponentClass || m_unlocks.empty()) return 0;

        auto* targetWorld = worldContext ? worldContext->GetWorld() : nullptr;
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            m_progressComponentClass, candidates, true);

        size_t applied = 0;
        for (auto* candidate : candidates)
        {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject
                    | RF_BeginDestroyed | RF_FinishDestroyed)))
                continue;
            if (targetWorld && candidate->GetWorld() != targetWorld)
                continue;
            if (ApplyUnlocks(candidate)) ++applied;
        }

        if (!applied && reportDeferred)
        {
            PS::Log<LogLevel::Verbose>(STR(
                "[BUILDING-UNLOCK][DEFERRED] No live ProgressComponent was ready for world '{}'; native progress callbacks remain active.\n"),
                worldContext ? worldContext->GetPathName() : TEXT("<any>"));
        }
        return applied;
    }

    void DragonWildsBuildingModLoader::NotifyBuildingUnlocks(
        UObject* progressComponent, const std::vector<UObject*>& buildings) const
    {
        if (!progressComponent || buildings.empty()) return;

        auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr,
            TEXT("/Script/Dominion.ProgressComponent:BP_OnBuildingsUnlocked"));
        auto* arrayProperty = function ? CastField<FArrayProperty>(
            function->FindProperty(FName(TEXT("BuildingDataRefs"), FNAME_Find))) : nullptr;
        auto* loadedProperty = function ? CastField<FBoolProperty>(
            function->FindProperty(FName(TEXT("bFromLoadedState"), FNAME_Find))) : nullptr;
        if (!function || !arrayProperty || !loadedProperty
            || !CastField<FObjectProperty>(arrayProperty->GetInner()))
        {
            PS::Log<LogLevel::Warning>(STR(
                "Building unlock state was applied, but its native UI notification is unavailable.\n"));
            return;
        }

        std::vector<uint8_t> parameters(function->GetParmsSize());
        arrayProperty->InitializeValue_InContainer(parameters.data());
        try
        {
            auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(
                parameters.data());
            UECustom::FScriptArrayHelper helper(array, arrayProperty);
            for (auto* building : buildings)
            {
                UECustom::FManagedValue value;
                helper.InitializeValue(value);
                std::memcpy(value.GetData(), &building, sizeof(building));
                helper.Add(value);
            }
            loadedProperty->SetPropertyValueInContainer(parameters.data(), true);
            progressComponent->ProcessEvent(function, parameters.data());
            arrayProperty->DestroyValue_InContainer(parameters.data());
        }
        catch (...)
        {
            try { arrayProperty->DestroyValue_InContainer(parameters.data()); }
            catch (...) {}
            throw;
        }
    }

    UObject* DragonWildsBuildingModLoader::FindBuildingSubsystem(
        UObject* worldContext) const
    {
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            m_buildingPieceSubsystemClass, candidates, true);

        UObject* fallback = nullptr;
        int32 usable = 0;
        for (auto* candidate : candidates)
        {
            if (candidate && !candidate->HasAnyFlags(
                static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                fallback = candidate;
                usable++;
                if (worldContext && candidate->GetWorld() == worldContext->GetWorld())
                {
                    return candidate;
                }
            }
        }

        return usable == 1 ? fallback : nullptr;
    }

    bool DragonWildsBuildingModLoader::CaptureNativeRegistry(UObject* subsystem)
    {
        if (m_nativeRegistrySnapshot.Subsystem)
        {
            if (m_nativeRegistrySnapshot.Subsystem == subsystem)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection refused to snapshot an already reconstructed subsystem.\n"));
            }
            else
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection found stale world state from another subsystem.\n"));
            }
            return false;
        }

        auto* cls = subsystem ? subsystem->GetClassPrivate() : nullptr;
        auto* arrayProperty = cls ? CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("NetIdToData"))) : nullptr;
        auto* reverseProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("DataToNetIdMap"))) : nullptr;
        auto* persistenceProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("PersistenceIDToDataMap"))) : nullptr;
        auto* internalProperty = cls ? CastField<FMapProperty>(
            PropertyHelper::GetPropertyByName(cls, TEXT("InternalNameToDataMap"))) : nullptr;
        if (!arrayProperty || !reverseProperty || !persistenceProperty || !internalProperty)
        {
            return false;
        }

        NativeRegistrySnapshot snapshot;
        snapshot.Subsystem = subsystem;
        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        const auto elementSize = arrayProperty->GetInner()->GetElementSize();
        for (int32 index = 0; index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object, static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(object));
            if (!object) return false;
            snapshot.NetIdToData.push_back(object);
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            if (!indexProperty) return false;
            snapshot.BuildingPieceDataIndices.emplace(object, static_cast<int32>(
                indexProperty->GetSignedIntPropertyValue(
                    indexProperty->ContainerPtrToValuePtr<void>(object))));
        }

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        reverse.ForEachPair([&](void* key, void* value) {
            UObject* object = nullptr;
            uint16 index = 0;
            std::memcpy(&object, key, sizeof(object));
            std::memcpy(&index, value, sizeof(index));
            snapshot.DataToNetIdMap.emplace_back(object, index);
        });
        const auto captureStringMap = [&](FMapProperty* property,
                std::vector<RegistryStringMapEntry>& entries) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            map.ForEachPair([&](void* key, void* value) {
                UObject* object = nullptr;
                std::memcpy(&object, value, sizeof(object));
                entries.push_back({ *static_cast<FString*>(key), object });
            });
        };
        captureStringMap(persistenceProperty, snapshot.PersistenceIDToDataMap);
        captureStringMap(internalProperty, snapshot.InternalNameToDataMap);
        m_nativeRegistrySnapshot = std::move(snapshot);
        return true;
    }

    bool DragonWildsBuildingModLoader::RestoreNativeRegistry()
    {
        auto* subsystem = m_nativeRegistrySnapshot.Subsystem;
        if (!subsystem) return true;
        auto* cls = subsystem->GetClassPrivate();
        auto* arrayProperty = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("NetIdToData")));
        auto* reverseProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("DataToNetIdMap")));
        auto* persistenceProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("PersistenceIDToDataMap")));
        auto* internalProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            cls, TEXT("InternalNameToDataMap")));
        if (!arrayProperty || !reverseProperty || !persistenceProperty || !internalProperty)
            return false;

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        UECustom::FScriptArrayHelper arrayHelper(array, arrayProperty);
        arrayHelper.Empty();
        for (auto* object : m_nativeRegistrySnapshot.NetIdToData)
        {
            UECustom::FManagedValue value;
            arrayHelper.InitializeValue(value);
            std::memcpy(value.GetData(), &object, sizeof(object));
            arrayHelper.Add(value);
        }

        const auto clearObjectMap = [&](FMapProperty* property) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            std::vector<UObject*> keys;
            map.ForEachPair([&](void* key, void*) {
                UObject* object = nullptr; std::memcpy(&object, key, sizeof(object));
                keys.push_back(object);
            });
            for (auto iterator = keys.rbegin(); iterator != keys.rend(); ++iterator)
            {
                auto* key = *iterator;
                map.Remove(&key);
            }
        };
        const auto clearStringMap = [&](FMapProperty* property) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            std::vector<FString> keys;
            map.ForEachPair([&](void* key, void*) { keys.push_back(*static_cast<FString*>(key)); });
            for (auto iterator = keys.rbegin(); iterator != keys.rend(); ++iterator)
            {
                map.Remove(&*iterator);
            }
        };
        clearObjectMap(reverseProperty);
        clearStringMap(persistenceProperty);
        clearStringMap(internalProperty);

        UECustom::FScriptMapHelper reverse(
            reverseProperty, reverseProperty->ContainerPtrToValuePtr<void>(subsystem));
        for (const auto& [object, index] : m_nativeRegistrySnapshot.DataToNetIdMap)
        {
            UECustom::FManagedValue pair; reverse.InitializePair(pair);
            std::memcpy(reverse.GetKeyPtr(pair.GetData()), &object, sizeof(object));
            std::memcpy(reverse.GetValuePtr(pair.GetData()), &index, sizeof(index));
            reverse.Add(pair);
        }
        reverse.Rehash();
        const auto restoreStringMap = [&](FMapProperty* property,
                const std::vector<RegistryStringMapEntry>& entries) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            for (const auto& entry : entries)
            {
                UECustom::FManagedValue pair; map.InitializePair(pair);
                *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = entry.Key;
                std::memcpy(map.GetValuePtr(pair.GetData()), &entry.Value, sizeof(entry.Value));
                map.Add(pair);
            }
            map.Rehash();
        };
        restoreStringMap(persistenceProperty, m_nativeRegistrySnapshot.PersistenceIDToDataMap);
        restoreStringMap(internalProperty, m_nativeRegistrySnapshot.InternalNameToDataMap);
        for (const auto& [object, index] : m_nativeRegistrySnapshot.BuildingPieceDataIndices)
        {
            auto* property = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            property->SetIntPropertyValue(property->ContainerPtrToValuePtr<void>(object),
                static_cast<int64>(index));
        }

        size_t reverseCount = 0;
        size_t persistenceCount = 0;
        size_t internalCount = 0;
        reverse.ForEachPair([&](void*, void*) { ++reverseCount; });
        UECustom::FScriptMapHelper restoredPersistence(
            persistenceProperty, persistenceProperty->ContainerPtrToValuePtr<void>(subsystem));
        restoredPersistence.ForEachPair([&](void*, void*) { ++persistenceCount; });
        UECustom::FScriptMapHelper restoredInternal(
            internalProperty, internalProperty->ContainerPtrToValuePtr<void>(subsystem));
        restoredInternal.ForEachPair([&](void*, void*) { ++internalCount; });
        bool valid = array->Num() == static_cast<int32>(m_nativeRegistrySnapshot.NetIdToData.size())
            && reverseCount == m_nativeRegistrySnapshot.DataToNetIdMap.size()
            && persistenceCount == m_nativeRegistrySnapshot.PersistenceIDToDataMap.size()
            && internalCount == m_nativeRegistrySnapshot.InternalNameToDataMap.size();
        for (int32 index = 0; valid && index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object,
                static_cast<uint8*>(array->GetData())
                    + index * arrayProperty->GetInner()->GetElementSize(),
                sizeof(object));
            valid = object == m_nativeRegistrySnapshot.NetIdToData[index];
        }

        if (!valid)
        {
            PS::Log<LogLevel::Error>(
                STR("Native Building registry restoration audit failed; "
                    "registry snapshot retained for safety.\n"));
            return false;
        }
        ClearWorldRegistryState();
        return true;
    }

    void DragonWildsBuildingModLoader::ClearWorldRegistryState()
    {
        m_nativeRegistrySnapshot = {};
    }

    UObject* DragonWildsBuildingModLoader::LoadObject(
        const RC::StringType& path) const
    {
        if (auto* object = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, path.c_str(), false))
        {
            return object;
        }

        UECustom::TSoftObjectPtr<UObject> soft{
            UECustom::FSoftObjectPath(path)
        };
        return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
    }

    RC::StringType DragonWildsBuildingModLoader::Identity(
        const RC::StringType& owner, const RC::StringType& key)
    {
        return owner + TEXT("\n") + key;
    }
}
