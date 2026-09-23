#include "Utility/NativeFunctionHook.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
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
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/Hooks.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsBuildingModLoader.h"
#include "Loader/OwnedContentLedger.h"
#include "Runtime/HostServices.h"

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
        constexpr const TCHAR* RetiredStabilityProfileRow = TEXT("FarmPlot");
        constexpr int BuildingManifestVersion = 1;

        std::string OrderedFingerprint(const std::vector<UObject*>& objects)
        {
            uint64_t hash = 14695981039346656037ull;
            const auto append = [&](const std::string& value, uint8_t separator) {
                for (const auto byte : value)
                {
                    hash ^= static_cast<uint8_t>(byte);
                    hash *= 1099511628211ull;
                }
                hash ^= separator;
                hash *= 1099511628211ull;
            };
            for (auto* object : objects)
            {
                auto* property = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), TEXT("PersistenceID")));
                if (!property) return {};
                const auto id = property->GetPropertyValue(
                    property->ContainerPtrToValuePtr<void>(object));
                append(RC::to_string(*id), 0x1f);
                append(RC::to_string(object->GetPathName()), 0x1e);
            }
            std::ostringstream output;
            output << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
            return output.str();
        }

        RC::StringType GuidString(const void* guidData)
        {
            uint32 words[4]{};
            std::memcpy(words, guidData, sizeof(words));
            return std::format(STR("{:08X}-{:08X}-{:08X}-{:08X}"),
                words[0], words[1], words[2], words[3]);
        }

        constexpr const TCHAR* UnlockHookPaths[] = {
            TEXT("/Script/Dominion.ProgressComponent:"
                 "Client_HandleNewBuildingPiecesLoadedFromPersistence"),
            TEXT("/Script/Dominion.ProgressComponent:Client_OnBuildingsUnlocked"),
        };
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
            PS::RoutineLog("buildings",
                STR("No active building definitions; native registry hooks were not registered.\n"));
            return;
        }

        RegisterHooks();
    }

    DragonWildsBuildingModLoader::~DragonWildsBuildingModLoader()
    {
        if (m_initGameStateCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_initGameStateCallbackId);
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

        try {
            for(const auto& record:OwnedContent::Declarations(data,RC::to_string(modName),"Building")) {
                BuildingDefinition definition{};
                definition.Owner=modName;
                definition.Key=RC::to_generic_string(record.InternalName);
                definition.AssetPath=RC::to_generic_string(record.Source);
                definition.Declared=true;
                definition.Unlock=false;
                definition.DeclaredPersistenceID=record.PersistenceID;
                definition.DeclaredInternalName=record.InternalName;
                definition.DeclaredInternalNameAsserted=record.InternalNameAsserted;
                auto existing=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& value){
                    return value.Owner==definition.Owner && value.Key==definition.Key;});
                if(existing==m_definitions.end())m_definitions.push_back(std::move(definition));
                else *existing=std::move(definition);
            }
        } catch(const std::exception& error) {
            PS::Log<LogLevel::Error>(STR("[SAVE-CLEANER][DECLARATION][LOADER:buildings][MOD:{}] Declaration rejected; other building records continue: {}.\n"),
                modName,PS::ToWideSafe(error.what()));
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
                    m_buildings.erase(identity);
                    DiscardUncommittedClone(building);
                }
                result.Errors++;
            };
            if (!ValidateBuildableActor(source, definition)
                || !ApplyProperties(building, definition, result)
                || !ApplyRequirements(building, definition))
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

            m_buildings[identity] = building;

            if (definition.Unlock)
            {
                m_unlocks.insert(identity);
            }
            else
            {
                m_unlocks.erase(identity);
            }

            m_applied.insert(identity);
            if(definition.Declared) {
                auto* name=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(building->GetClassPrivate(),TEXT("InternalName")));
                const auto actualName=name?RC::to_string(*name->GetPropertyValue(name->ContainerPtrToValuePtr<void>(building))):std::string{};
                OwnedContent::Merge(OwnedContent::LedgerPath(PS::HostServices::SettingsDirectory()),
                    {{"Building",RC::to_string(definition.Owner),definition.DeclaredPersistenceID,
                        actualName,RC::to_string(definition.AssetPath)}});
            }
            result.Loaded++;
        }

        // Do not install world-entry/world-teardown registry hooks for an
        // empty building configuration.  The loader is present in every
        // RuneSchema install, but touching the native registry is only needed
        // when at least one valid building definition was applied.
        if (m_applied.empty())
        {
            PS::RoutineLog("buildings",
                STR("No active building definitions; native registry hooks were not registered.\n"));
            return;
        }

        RegisterHooks();
        if (auto* progress = FindProgressComponent())
        {
            ApplyUnlocks(progress);
        }

        if (result.Loaded || result.Errors)
        {
            PS::Log<LogLevel::Normal>(
                STR("Buildings: {} loaded, {} error{}.\n"),
                result.Loaded, result.Errors, result.Errors == 1 ? STR("") : STR("s"));
        }
    }

    UObject* DragonWildsBuildingModLoader::LoadBuilding(
        const BuildingDefinition& definition, LoadResult& result, UObject** sourceOut)
    {
        const auto identity = Identity(definition.Owner, definition.Key);
        if (auto found = m_buildings.find(identity);
            found != m_buildings.end())
        {
            if (sourceOut) *sourceOut = LoadObject(definition.AssetPath);
            return found->second;
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
        return building;
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
        m_createdBuildings.push_back(created);
        PS::Log<LogLevel::Verbose>(
            STR("{}: cloned building '{}' as '{}' using {} reflected properties.\n"),
            owner, source->GetPathName(), created->GetPathName(), copied);
        return created;
    }

    bool DragonWildsBuildingModLoader::ValidateBuildableActor(
        UObject* source, const BuildingDefinition& definition)
    {
        if (!definition.Properties.contains("BuildableActor")) return true;
        if (!definition.Clone)
        {
            PS::Log<LogLevel::Error>(
                STR("Building '{}': BuildableActor replacement requires '$Clone'; mutating a shared native data asset is refused.\n"),
                definition.Key);
            return false;
        }

        const auto path = RC::to_generic_string(
            definition.Properties.at("BuildableActor").get<std::string>());
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

    void DragonWildsBuildingModLoader::DiscardUncommittedClone(UObject* building)
    {
        if (!building) return;
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
        if (!handleProperty || !tableProperty)
        {
            return false;
        }

        auto* handle = handleProperty->ContainerPtrToValuePtr<void>(building);
        auto* tableAddress = tableProperty->ContainerPtrToValuePtr<void>(handle);
        UObject* currentTable = nullptr;
        std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        if (currentTable)
        {
            return true;
        }

        auto* table = LoadObject(StabilityProfilePath);
        if (!table)
        {
            return false;
        }

        std::memcpy(tableAddress, &table, sizeof(table));
        currentTable = nullptr;
        std::memcpy(&currentTable, tableAddress, sizeof(currentTable));
        return currentTable == table;
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

    bool DragonWildsBuildingModLoader::ResolveWorldRegistryPath(AGameModeBase* gameMode)
    {
        auto* persistenceClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.PersistenceSubsystem"));
        if (!gameMode || !persistenceClass)
        {
            return false;
        }

        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(persistenceClass, candidates, true);
        UObject* persistence = nullptr;
        for (auto* candidate : candidates)
        {
            if (!candidate || candidate->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }
            if (candidate->GetWorld() == gameMode->GetWorld())
            {
                if (persistence)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry protection found multiple persistence subsystems for one world.\n"));
                    return false;
                }
                persistence = candidate;
            }
        }
        if (!persistence)
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry protection could not resolve this world's persistence subsystem.\n"));
            return false;
        }

        auto* settingsProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            persistence->GetClassPrivate(), TEXT("WorldSaveSettings")));
        auto* guidProperty = settingsProperty ? CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                settingsProperty->GetStruct().Get(), TEXT("WorldSaveGuid"))) : nullptr;
        if (!settingsProperty || !guidProperty || guidProperty->GetElementSize() != 16)
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry protection cannot read WorldSaveGuid.\n"));
            return false;
        }

        auto* settings = settingsProperty->ContainerPtrToValuePtr<void>(persistence);
        auto* guid = guidProperty->ContainerPtrToValuePtr<void>(settings);
        uint8 guidBytes[16]{};
        std::memcpy(guidBytes, guid, sizeof(guidBytes));
        if (std::all_of(std::begin(guidBytes), std::end(guidBytes),
                [](uint8 value) { return value == 0; }))
        {
            m_worldManifestPath.clear();
            return false;
        }

        const auto guidString = GuidString(guid);
        auto* systemLibrary = ActorHelper::ResolveObject(
            TEXT("/Script/Engine.Default__KismetSystemLibrary"));
        if (!systemLibrary)
        {
            return false;
        }
        auto savedDirectoryCall = ActorHelper::FunctionCall(systemLibrary,
            TEXT("/Script/Engine.KismetSystemLibrary:GetProjectSavedDirectory"));
        savedDirectoryCall.Invoke();
        const auto savedDirectory = savedDirectoryCall.Result<FString>();
        if (savedDirectory.GetCharArray().Num() <= 1)
        {
            return false;
        }

        m_worldManifestPath = std::filesystem::path(*savedDirectory)
            / "RuneSchema" / RC::to_string(guidString) / "CustomBuildingData.json";
        return true;
    }

    bool DragonWildsBuildingModLoader::ProtectWorldRegistry(UObject* subsystem)
    {
        auto* subsystemClass = subsystem->GetClassPrivate();
        auto* arrayProperty = CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("NetIdToData")));
        auto* reverseProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("DataToNetIdMap")));
        auto* persistenceMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("PersistenceIDToDataMap")));
        auto* internalMapProperty = CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
            subsystemClass, TEXT("InternalNameToDataMap")));
        if (!arrayProperty || !reverseProperty || !persistenceMapProperty || !internalMapProperty)
        {
            return false;
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        const auto elementSize = arrayProperty->GetInner()->GetElementSize();
        struct ActiveDefinition {
            const BuildingDefinition* Definition = nullptr;
            UObject* Object = nullptr;
            std::string PersistenceId;
            std::string InternalName;
            std::string AssetPath;
        };
        std::unordered_map<std::string, ActiveDefinition> activeById;
        for (const auto& definition : m_definitions)
        {
            const auto identity = Identity(definition.Owner, definition.Key);
            if (!m_applied.contains(identity)) continue;

            const auto loaded = m_buildings.find(identity);
            if (loaded == m_buildings.end() || !loaded->second) continue;
            auto* object = loaded->second;
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("InternalName")));
            if (!idProperty || !nameProperty)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building '{} / {}' resolved as '{}' but is missing required identity field(s): PersistenceID={}, InternalName={}.\n"),
                    definition.Owner, definition.Key,
                    object->GetClassPrivate()->GetPathName(),
                    idProperty ? STR("yes") : STR("no"),
                    nameProperty ? STR("yes") : STR("no"));
                return false;
            }
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto nameValue = nameProperty->GetPropertyValue(
                nameProperty->ContainerPtrToValuePtr<void>(object));
            ActiveDefinition active{ &definition, object, RC::to_string(*idValue),
                RC::to_string(*nameValue), RC::to_string(object->GetPathName()) };
            if (active.PersistenceId.empty()
                || !activeById.emplace(active.PersistenceId, active).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("Building registry protection found a duplicate/empty custom PersistenceID.\n"));
                return false;
            }
        }
        std::unordered_map<UObject*, std::string> activeIdsByObject;
        for (const auto& [id, active] : activeById)
        {
            activeIdsByObject.emplace(active.Object, id);
        }

        std::vector<UObject*> current;
        std::vector<std::string> currentIds;
        std::unordered_map<std::string, int32> currentIndexById;
        for (int32 index = 0; index < array->Num(); ++index)
        {
            UObject* object = nullptr;
            std::memcpy(&object,
                static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(object));
            if (!object) return false;
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            if (!idProperty) return false;
            const auto idValue = idProperty->GetPropertyValue(
                idProperty->ContainerPtrToValuePtr<void>(object));
            const auto id = RC::to_string(*idValue);
            if (id.empty() || !currentIndexById.emplace(id, index).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("The live Building registry contains a duplicate/empty PersistenceID; protection aborted.\n"));
                return false;
            }
            current.push_back(object);
            currentIds.push_back(id);
        }
        nlohmann::json manifest;
        bool created = false;
        if (std::filesystem::exists(m_worldManifestPath))
        {
            try
            {
                std::ifstream input(m_worldManifestPath);
                manifest = nlohmann::json::parse(input, nullptr, true, true);
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json is corrupt; registry protection was not applied: {}\n"),
                    PS::ToWideSafe(error.what()));
                return false;
            }
        }
        else
        {
            if (activeById.empty()) return true;
            created = true;
            manifest = {
                { "FormatVersion", BuildingManifestVersion },
                { "VanillaDefinitionCount", 0 },
                { "OrderedVanillaFingerprint", "" },
                { "Records", nlohmann::json::array() },
            };
        }

        if (!manifest.is_object()
            || manifest.value("FormatVersion", 0) != BuildingManifestVersion
            || !manifest.contains("VanillaDefinitionCount")
            || (!manifest["VanillaDefinitionCount"].is_number_unsigned()
                && (!manifest["VanillaDefinitionCount"].is_number_integer()
                    || manifest["VanillaDefinitionCount"].get<int64_t>() < 0))
            || !manifest.contains("OrderedVanillaFingerprint")
            || !manifest["OrderedVanillaFingerprint"].is_string()
            || !manifest.contains("Records") || !manifest["Records"].is_array())
        {
            PS::Log<LogLevel::Error>(
                STR("CustomBuildingData.json has an unsupported or invalid schema; registry untouched.\n"));
            return false;
        }

        std::vector<UObject*> vanilla;
        for (int32 index = 0; index < static_cast<int32>(current.size()); ++index)
        {
            if (!activeById.contains(currentIds[index])) vanilla.push_back(current[index]);
        }
        const auto vanillaFingerprint = OrderedFingerprint(vanilla);
        if (vanillaFingerprint.empty())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry fingerprint could not be generated; registry untouched.\n"));
            return false;
        }

        if (created)
        {
            manifest["VanillaDefinitionCount"] = vanilla.size();
            manifest["OrderedVanillaFingerprint"] = vanillaFingerprint;
            std::unordered_set<int32> usedIndices;
            int32 nextIndex = static_cast<int32>(current.size());
            for (const auto& definition : m_definitions)
            {
                const auto identity = Identity(definition.Owner, definition.Key);
                if (!m_applied.contains(identity)) continue;

                const auto loaded = m_buildings.find(identity);
                if (loaded == m_buildings.end() || !loaded->second) continue;
                auto* object = loaded->second;
                const auto& active = activeById.at(activeIdsByObject.at(object));
                auto foundIndex = currentIndexById.find(active.PersistenceId);
                int32 index = foundIndex == currentIndexById.end() ? nextIndex++ : foundIndex->second;
                if (!usedIndices.insert(index).second)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry history produced a duplicate custom index; registry untouched.\n"));
                    return false;
                }
                manifest["Records"].push_back({
                    { "Owner", RC::to_string(definition.Owner) },
                    { "Key", RC::to_string(definition.Key) },
                    { "PersistenceID", active.PersistenceId },
                    { "InternalName", active.InternalName },
                    { "AssetPath", active.AssetPath },
                    { "HistoricalIndex", index },
                    { "State", "active" },
                });
            }
        }
        else if (manifest["VanillaDefinitionCount"].get<size_t>() != vanilla.size()
            || manifest["OrderedVanillaFingerprint"].get<std::string>() != vanillaFingerprint)
        {
            PS::Log<LogLevel::Error>(
                STR("Incompatible vanilla Building registry detected; world registry reconstruction aborted.\n"));
            return false;
        }

        std::unordered_set<std::string> recordIds;
        std::unordered_set<std::string> recordOwners;
        std::unordered_set<int32> recordIndices;
        auto& records = manifest["Records"];
        for (auto& record : records)
        {
            if (!record.is_object() || !record.contains("Owner") || !record["Owner"].is_string()
                || !record.contains("Key") || !record["Key"].is_string()
                || !record.contains("PersistenceID") || !record["PersistenceID"].is_string()
                || !record.contains("InternalName") || !record["InternalName"].is_string()
                || !record.contains("AssetPath") || !record["AssetPath"].is_string()
                || !record.contains("HistoricalIndex") || !record["HistoricalIndex"].is_number_integer()
                || !record.contains("State") || !record["State"].is_string())
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains an invalid record; registry untouched.\n"));
                return false;
            }
            const auto id = record["PersistenceID"].get<std::string>();
            const auto ownerKey = record["Owner"].get<std::string>() + "\n"
                + record["Key"].get<std::string>();
            const auto index = record["HistoricalIndex"].get<int32>();
            const auto state = record["State"].get<std::string>();
            if (id.empty() || index < 0 || (state != "active" && state != "retired")
                || !recordIds.insert(id).second || !recordOwners.insert(ownerKey).second
                || !recordIndices.insert(index).second)
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains duplicate or invalid records; registry untouched.\n"));
                return false;
            }
        }
        const auto historicalSize = static_cast<int32>(vanilla.size() + records.size());
        if (std::any_of(recordIndices.begin(), recordIndices.end(),
                [&](int32 index) { return index >= historicalSize; }))
        {
            PS::Log<LogLevel::Error>(
                STR("CustomBuildingData.json contains an out-of-range historical index; registry untouched.\n"));
            return false;
        }

        for (const auto& [id, active] : activeById)
        {
            if (recordIds.contains(id)) continue;

            const auto ownerKey = RC::to_string(active.Definition->Owner) + "\n"
                + RC::to_string(active.Definition->Key);
            if (recordOwners.contains(ownerKey))
            {
                PS::Log<LogLevel::Error>(
                    STR("Custom Building '{} / {}' conflicts with an existing historical identity; registry untouched.\n"),
                    active.Definition->Owner, active.Definition->Key);
                return false;
            }

            const auto nextIndex = static_cast<int32>(vanilla.size() + records.size());
            records.push_back({
                { "Owner", RC::to_string(active.Definition->Owner) },
                { "Key", RC::to_string(active.Definition->Key) },
                { "PersistenceID", id }, { "InternalName", active.InternalName },
                { "AssetPath", active.AssetPath }, { "HistoricalIndex", nextIndex },
                { "State", "active" },
            });
            recordIds.insert(id);
            recordOwners.insert(ownerKey);
            recordIndices.insert(nextIndex);
        }

        const auto totalSize = static_cast<int32>(vanilla.size() + records.size());
        if (totalSize > std::numeric_limits<uint16>::max())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry exceeds the native index limit; registry untouched.\n"));
            return false;
        }
        for (const auto& record : records)
        {
            const auto id = record["PersistenceID"].get<std::string>();
            if (auto active = activeById.find(id); active != activeById.end())
            {
                const auto& value = active->second;
                if (record["Owner"].get<std::string>() != RC::to_string(value.Definition->Owner)
                    || record["Key"].get<std::string>() != RC::to_string(value.Definition->Key)
                    || record["AssetPath"].get<std::string>() != value.AssetPath
                    || record["InternalName"].get<std::string>() != value.InternalName)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Returning custom Building '{}' conflicts with its historical record; replacement aborted.\n"),
                        RC::to_generic_string(id));
                    return false;
                }
            }
        }
        std::vector<UObject*> desired(totalSize, nullptr);
        m_retiredBuildings.clear();
        int retired = 0;
        int reclaimed = 0;
        for (auto& record : records)
        {
            const auto id = record["PersistenceID"].get<std::string>();
            const auto index = record["HistoricalIndex"].get<int32>();
            if (index < 0 || index >= totalSize || desired[index])
            {
                PS::Log<LogLevel::Error>(
                    STR("CustomBuildingData.json contains a conflicting historical index; registry untouched.\n"));
                return false;
            }
            UObject* object = nullptr;
            if (auto active = activeById.find(id); active != activeById.end())
            {
                const auto& value = active->second;
                object = value.Object;
                if (record["State"].get<std::string>() == "retired") ++reclaimed;
                record["State"] = "active";
            }
            else
            {
                record["State"] = "retired";
                ++retired;
                object = CreateRetiredBuilding(record, index);
                if (!object)
                {
                    PS::Log<LogLevel::Error>(
                        STR("Retired Building creation failed; registry untouched.\n"));
                    return false;
                }
                object->SetRootSet();
                m_retiredBuildings.push_back(object);
                PS::Log<LogLevel::Verbose>(STR("Created retired Building '{}' at historical index {}.\n"),
                    RC::to_generic_string(id), index);
            }
            desired[index] = object;
        }
        auto vanillaIterator = vanilla.begin();
        for (auto& slot : desired)
        {
            if (!slot)
            {
                if (vanillaIterator == vanilla.end())
                {
                    PS::Log<LogLevel::Error>(
                        STR("Building registry history does not match the vanilla registry; reconstruction aborted.\n"));
                    return false;
                }
                slot = *vanillaIterator++;
            }
        }
        if (vanillaIterator != vanilla.end())
        {
            PS::Log<LogLevel::Error>(
                STR("Building registry history does not match the vanilla registry; reconstruction aborted.\n"));
            return false;
        }

        if (!CaptureNativeRegistry(subsystem))
        {
            return false;
        }

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

        const auto addStringMap = [&](FMapProperty* property, const FString& key, UObject* object) {
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

            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("PersistenceID")));
            auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("InternalName")));
            if (!indexProperty || !idProperty || !nameProperty) return false;
            indexProperty->SetIntPropertyValue(
                indexProperty->ContainerPtrToValuePtr<void>(object),
                static_cast<int64>(index));
            if (recordIndices.contains(index))
            {
                const auto id = idProperty->GetPropertyValue(
                    idProperty->ContainerPtrToValuePtr<void>(object));
                const auto name = nameProperty->GetPropertyValue(
                    nameProperty->ContainerPtrToValuePtr<void>(object));
                addStringMap(persistenceMapProperty, id, object);
                addStringMap(internalMapProperty, id, object);
                if (name.GetCharArray().Num() > 1) addStringMap(internalMapProperty, name, object);
            }
        }
        reverse.Rehash();

        bool valid = array->Num() == static_cast<int32>(desired.size());
        for (int32 index = 0; index < array->Num(); ++index)
        {
            auto* object = desired[index];
            int32 reverseIndex = -1;
            reverse.ForEachPair([&](void* key, void* value) {
                UObject* candidate = nullptr;
                std::memcpy(&candidate, key, sizeof(candidate));
                if (candidate == object)
                {
                    uint16 found = 0;
                    std::memcpy(&found, value, sizeof(found));
                    reverseIndex = static_cast<int32>(found);
                }
            });
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            const auto reported = static_cast<int32>(indexProperty->GetSignedIntPropertyValue(
                indexProperty->ContainerPtrToValuePtr<void>(object)));
            UObject* forward = nullptr;
            std::memcpy(&forward,
                static_cast<uint8*>(array->GetData()) + index * elementSize,
                sizeof(forward));
            valid = valid && forward == object && reverseIndex == index && reported == index;
        }
        const auto stringMapMatches = [&](FMapProperty* property,
                const FString& key, UObject* expected) {
            UECustom::FScriptMapHelper map(
                property, property->ContainerPtrToValuePtr<void>(subsystem));
            int matches = 0;
            map.ForEachPair([&](void* mapKey, void* mapValue) {
                if (static_cast<FString*>(mapKey)->Equals(key))
                {
                    UObject* value = nullptr;
                    std::memcpy(&value, mapValue, sizeof(value));
                    if (value == expected) ++matches;
                }
            });
            return matches == 1;
        };
        for (const auto& record : records)
        {
            const auto index = record["HistoricalIndex"].get<int32>();
            auto* object = desired[index];
            const FString id(RC::to_generic_string(
                record["PersistenceID"].get<std::string>()).c_str());
            const FString name(RC::to_generic_string(
                record["InternalName"].get<std::string>()).c_str());
            valid = valid && stringMapMatches(persistenceMapProperty, id, object)
                && stringMapMatches(internalMapProperty, id, object)
                && (name.GetCharArray().Num() <= 1
                    || stringMapMatches(internalMapProperty, name, object));
        }
        for (const auto& [id, active] : activeById)
        {
            int32 matches = 0;
            int32 installedIndex = -1;
            for (int32 index = 0; index < array->Num(); ++index)
            {
                UObject* candidate = nullptr;
                std::memcpy(&candidate,
                    static_cast<uint8*>(array->GetData()) + index * elementSize,
                    sizeof(candidate));
                if (candidate == active.Object)
                {
                    ++matches;
                    installedIndex = index;
                }
            }
            auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
                active.Object->GetClassPrivate(), TEXT("BuildingPieceDataIndex")));
            const auto reported = indexProperty ? static_cast<int32>(
                indexProperty->GetSignedIntPropertyValue(
                    indexProperty->ContainerPtrToValuePtr<void>(active.Object))) : -1;
            int32 reverseIndex = -1;
            reverse.ForEachPair([&](void* key, void* value) {
                UObject* candidate = nullptr;
                std::memcpy(&candidate, key, sizeof(candidate));
                if (candidate == active.Object)
                {
                    uint16 found = 0; std::memcpy(&found, value, sizeof(found));
                    reverseIndex = found;
                }
            });
            const bool returningValid = matches == 1 && installedIndex == reported
                && installedIndex == reverseIndex;
            valid = valid && returningValid;
        }
        if (!valid)
        {
            PS::Log<LogLevel::Error>(
                STR("Custom Building registry reconstruction validation failed.\n"));
            return false;
        }

        std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
            return left["HistoricalIndex"].template get<int32>()
                < right["HistoricalIndex"].template get<int32>();
        });
        try
        {
            std::filesystem::create_directories(m_worldManifestPath.parent_path());
            const auto temporary = m_worldManifestPath.string() + ".tmp";
            std::ofstream output(temporary, std::ios::trunc);
            output << manifest.dump(2);
            output.close();
            if (!output.good()) throw std::runtime_error("write failed");
            std::filesystem::copy_file(temporary, m_worldManifestPath,
                std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove(temporary);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("Could not persist CustomBuildingData.json: {}\n"), PS::ToWideSafe(error.what()));
            return false;
        }

        if (reclaimed)
        {
            PS::Log<LogLevel::Normal>(STR("Buildings: {} active, {} retired, {} reclaimed, 0 errors.\n"),
                activeById.size(), retired, reclaimed);
        }
        else
        {
            PS::Log<LogLevel::Normal>(STR("Buildings: {} active, {} retired, 0 errors.\n"),
                activeById.size(), retired);
        }
        return valid;
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
                        ->ApplyUnlocks(context.Context);
                },
                this);
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("BuildingLoaderInitGameState");
        m_initGameStateCallbackId = Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* gameMode) {
                PrepareWorldState(gameMode);
            },
            options);

        if (m_initGameStateCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Warning>(
                STR("Building world initialization callback could not be registered.\n"));
            return;
        }

        m_hooksRegistered = true;
    }

    void DragonWildsBuildingModLoader::PrepareWorldState(AGameModeBase* gameMode)
    {
        if (!m_catalogue)
        {
            m_catalogue = LoadObject(CataloguePath);
        }

        auto* subsystem = FindBuildingSubsystem(gameMode);
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
            return;
        }

        // A previous world's subsystem is no longer safe to dereference here.
        // Drop its snapshot before capturing the newly initialized registry.
        if (m_nativeRegistrySnapshot.Subsystem
            && m_nativeRegistrySnapshot.Subsystem != subsystem)
        {
            ClearWorldRegistryState();
        }

        auto* array = arrayProperty->ContainerPtrToValuePtr<FScriptArray>(subsystem);
        auto* catalogueIds = setProperty->ContainerPtrToValuePtr<FScriptSet>(m_catalogue);
        if (array->Num() < 700 || catalogueIds->Num() < 700)
        {
            PS::Log<LogLevel::Error>(
                STR("Buildings were not registered because the native registry is incomplete.\n"));
            return;
        }

        if (!ResolveWorldRegistryPath(gameMode)) return;

        bool protectedRegistry = false;
        try
        {
            protectedRegistry = ProtectWorldRegistry(subsystem);
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Error>(
                STR("Custom Building registry protection raised an error: {}\n"),
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
                STR("Custom Building registry protection failed; Building registration was aborted.\n"));
            return;
        }

        if (auto* progress = FindProgressComponent())
        {
            ApplyUnlocks(progress);
        }
    }

    void DragonWildsBuildingModLoader::ApplyUnlocks(UObject* progressComponent)
    {
        if (!progressComponent || m_unlocks.empty())
        {
            return;
        }

        std::vector<UObject*> buildings;
        for (const auto& key : m_unlocks)
        {
            auto found = m_buildings.find(key);
            if (found != m_buildings.end() && found->second)
            {
                buildings.push_back(found->second);
            }
        }

        auto* unlockedProperty = CastField<FArrayProperty>(
            PropertyHelper::GetPropertyByName(
                progressComponent->GetClassPrivate(), TEXT("BuildingsUnlocked")));
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
    }

    UObject* DragonWildsBuildingModLoader::FindProgressComponent() const
    {
        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(
            m_progressComponentClass, candidates, true);

        for (auto* candidate : candidates)
        {
            if (candidate
                && !candidate->HasAnyFlags(
                    static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                return candidate;
            }
        }

        return nullptr;
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

    UObject* DragonWildsBuildingModLoader::CreateRetiredBuilding(
        const nlohmann::json& record, int32 historicalIndex)
    {
        auto* stabilityTable = static_cast<UDataTable*>(LoadObject(StabilityProfilePath));
        if (!stabilityTable || !stabilityTable->GetRowStruct())
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building could not load the native stability table.\n"));
            return nullptr;
        }

        const FName stabilityRow(RetiredStabilityProfileRow);
        if (!stabilityTable->FindRowUnchecked(stabilityRow))
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building stability profile row '{}' is unavailable.\n"),
                RetiredStabilityProfileRow);
            return nullptr;
        }

        auto* object = ActorHelper::ConstructTransientObject(m_buildingPieceClass,
            std::format(STR("RuneSchema_RetiredBuilding_{}"), historicalIndex));
        if (!object)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building construction failed.\n"));
            return nullptr;
        }
        auto* objectClass = object->GetClassPrivate();
        auto* idProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("PersistenceID")));
        auto* nameProperty = CastField<FStrProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("InternalName")));
        auto* indexProperty = CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingPieceDataIndex")));
        auto* stabilityProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingStabilityProfileRowHandle")));
        auto* pieceTagProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("PieceTag")));
        auto* representationProperty = PropertyHelper::GetPropertyByName(
            objectClass, TEXT("RepresentationCategory"));
        auto* actorProperty = CastField<FSoftObjectProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildableActor")));
        auto* proxyProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("BuildingPieceProxyData")));
        auto* farAwayProperty = CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(
            objectClass, TEXT("bShouldBeVisibleFromFarAway")));
        if (!idProperty || !nameProperty || !indexProperty || !stabilityProperty
            || !pieceTagProperty || !representationProperty || !actorProperty
            || !proxyProperty || !farAwayProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building property layout is incompatible with RuneSchema.\n"));
            return nullptr;
        }

        auto* stabilityStruct = stabilityProperty->GetStruct().Get();
        auto* tableProperty = stabilityStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(stabilityStruct, TEXT("DataTable"))) : nullptr;
        auto* rowProperty = stabilityStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(stabilityStruct, TEXT("RowName"))) : nullptr;
        auto* tagStruct = pieceTagProperty->GetStruct().Get();
        auto* tagNameProperty = tagStruct ? CastField<FNameProperty>(
            PropertyHelper::GetPropertyByName(tagStruct, TEXT("TagName"))) : nullptr;
        auto* proxyStruct = proxyProperty->GetStruct().Get();
        auto* proxyMeshProperty = proxyStruct ? CastField<FSoftObjectProperty>(
            PropertyHelper::GetPropertyByName(proxyStruct, TEXT("ProxyMesh"))) : nullptr;
        if (!tableProperty || !rowProperty || !tagNameProperty || !proxyMeshProperty)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building nested property layout is incompatible with RuneSchema.\n"));
            return nullptr;
        }

        const FString historicalId(RC::to_generic_string(
            record["PersistenceID"].get<std::string>()).c_str());
        const FString historicalName(RC::to_generic_string(
            record["InternalName"].get<std::string>()).c_str());
        idProperty->SetPropertyValue(idProperty->ContainerPtrToValuePtr<void>(object), historicalId);
        nameProperty->SetPropertyValue(nameProperty->ContainerPtrToValuePtr<void>(object), historicalName);
        indexProperty->SetIntPropertyValue(
            indexProperty->ContainerPtrToValuePtr<void>(object),
            static_cast<int64>(historicalIndex));

        auto* stability = stabilityProperty->ContainerPtrToValuePtr<void>(object);
        std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(stability),
            &stabilityTable, sizeof(stabilityTable));
        rowProperty->SetPropertyValue(rowProperty->ContainerPtrToValuePtr<void>(stability),
            stabilityRow);
        PropertyHelper::CopyJsonValueToContainer(
            object, representationProperty, "ManagedActor");
        PropertyHelper::CopyJsonValueToContainer(object, farAwayProperty, false);

        const auto& actor = *actorProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(object);
        auto* proxy = proxyProperty->ContainerPtrToValuePtr<void>(object);
        const auto& proxyMesh = *proxyMeshProperty->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(proxy);
        auto* tag = pieceTagProperty->ContainerPtrToValuePtr<void>(object);
        const auto tagName = tagNameProperty->GetPropertyValue(
            tagNameProperty->ContainerPtrToValuePtr<void>(tag));
        UObject* verifiedTable = nullptr;
        std::memcpy(&verifiedTable, tableProperty->ContainerPtrToValuePtr<void>(stability),
            sizeof(verifiedTable));
        const auto verifiedRow = rowProperty->GetPropertyValue(
            rowProperty->ContainerPtrToValuePtr<void>(stability));
        const auto actorPathEmpty = actor.ObjectID.AssetPath.GetPackageName() == NAME_None
            && actor.ObjectID.AssetPath.GetAssetName() == NAME_None;
        const auto proxyPathEmpty = proxyMesh.ObjectID.AssetPath.GetPackageName() == NAME_None
            && proxyMesh.ObjectID.AssetPath.GetAssetName() == NAME_None;
        const auto farAway = farAwayProperty->GetPropertyValue(
            farAwayProperty->ContainerPtrToValuePtr<void>(object));
        auto* representationEnum = CastField<FEnumProperty>(representationProperty);
        auto* representationNumeric = representationEnum
            ? representationEnum->GetUnderlyingProperty()
            : CastField<FNumericProperty>(representationProperty);
        const auto representation = representationNumeric
            ? representationNumeric->GetSignedIntPropertyValue(
                representationProperty->ContainerPtrToValuePtr<void>(object))
            : int64{-1};
        if (verifiedTable != stabilityTable || verifiedRow != stabilityRow
            || !stabilityTable->FindRowUnchecked(verifiedRow)
            || !actorPathEmpty || !proxyPathEmpty || !tagName.IsNone() || farAway
            || representation != 1)
        {
            PS::Log<LogLevel::Error>(
                STR("Retired Building critical-field validation failed.\n"));
            return nullptr;
        }

        return object;
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
                    "retired Buildings and snapshot retained for safety.\n"));
            return false;
        }
        ClearWorldRegistryState();
        return true;
    }

    void DragonWildsBuildingModLoader::ClearWorldRegistryState()
    {
        const auto retired = m_retiredBuildings.size();
        for (auto* building : m_retiredBuildings)
        {
            if (building && building->IsRootSet()) building->ClearRootSet();
        }
        m_retiredBuildings.clear();
        m_nativeRegistrySnapshot = {};
        m_worldManifestPath.clear();
        if (retired)
        {
            PS::Log<LogLevel::Verbose>(
                STR("Released {} retired Building{} and cleared world registry state.\n"),
                retired, retired == 1 ? STR("") : STR("s"));
        }
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
