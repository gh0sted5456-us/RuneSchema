#include <algorithm>
#include <limits>
#include "Unreal/AActor.hpp"
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsCourseLoader.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace {
    constexpr const TCHAR* OrbClassPath = TEXT("/Agility/Gameplay/World/BP_AgilityOrb_Base.BP_AgilityOrb_Base_C");
    constexpr const TCHAR* StarterClassPath = TEXT("/Agility/Gameplay/World/BP_AgilityCourseStarter_Base.BP_AgilityCourseStarter_Base_C");
    constexpr const TCHAR* ZoneClassPath = TEXT("/Agility/Gameplay/World/BP_AgilityCourseZone.BP_AgilityCourseZone_C");
    constexpr const TCHAR* StaminaOrbClassPath = TEXT("/Agility/Gameplay/World/AgilityObjects/BP_StaminaOrb.BP_StaminaOrb_C");
    constexpr const TCHAR* DataAssetClassPath = TEXT("/Script/Dominion.AgilityCourseDataAsset");
    constexpr const TCHAR* TargetPointClassPath = TEXT("/Script/Engine.TargetPoint");

    constexpr uint8 SplineSpaceWorld = 1;

    bool IsFrontEndWorld(UWorld* world)
    {
        if (!world)
        {
            return true;
        }

        const auto name = world->GetName();
        return name.find(STR("FrontEnd")) != RC::StringType::npos
            || name.find(STR("MainMenu")) != RC::StringType::npos;
    }

    nlohmann::json MakeDuration(int totalSeconds)
    {
        totalSeconds = std::max(0, totalSeconds);
        return {
            { "Days", 0 },
            { "Hours", totalSeconds / 3600 },
            { "Minutes", (totalSeconds % 3600) / 60 },
            { "Seconds", totalSeconds % 60 },
        };
    }

}

namespace DragonWilds {
    namespace {
        constexpr float BuildDelaySeconds = 1.0f;
        constexpr float CheckIntervalSeconds = 0.25f;
    }

    DragonWildsCourseLoader::DragonWildsCourseLoader() : DragonWildsModLoaderBase("courses")
    {
        SetDisplayName(TEXT("Course Loader"));
    }

    DragonWildsCourseLoader::~DragonWildsCourseLoader()
    {
        if (m_initGameStateCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_initGameStateCallbackId);
        }
        if (m_buildTickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_buildTickCallbackId);
        }
        if (m_propTickCallbackId != Hook::ERROR_ID)
        {
            Hook::UnregisterCallback(m_propTickCallbackId);
        }
    }

    void DragonWildsCourseLoader::OnLoad(const fs::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::GameInstanceInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            LoadCourses(data, modName);
        });
    }

    void DragonWildsCourseLoader::OnAutoReload(const RC::StringType& modName, const fs::path& modFilePath)
    {
        PS::Log<LogLevel::Warning>(
            STR("Ignored course changes for {}. Courses are built into the world on load; "
                "restart the game to apply them.\n"), modName);
    }

    bool DragonWildsCourseLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        return engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit;
    }

    bool DragonWildsCourseLoader::OnInitialize()
    {
        return SetupWorldReadyHook();
    }

    void DragonWildsCourseLoader::LoadCourses(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_array())
        {
            for (const auto& value : data)
            {
                RegisterCourse(value, modName);
            }
            return;
        }

        RegisterCourse(data, modName);
    }

    void DragonWildsCourseLoader::RegisterCourse(const nlohmann::json& value, const RC::StringType& modName)
    {
        auto readString = [&](const nlohmann::json& source, const std::string& field, const RC::StringType& fallback) {
            if (!PS::JsonHelpers::FieldExists(source, field))
            {
                return fallback;
            }
            std::string parsed;
            PS::JsonHelpers::ParseString(source, field, parsed);
            return RC::to_generic_string(parsed);
        };

        auto readInt = [&](const nlohmann::json& source, const std::string& field, int fallback) {
            if (!PS::JsonHelpers::FieldExists(source, field))
            {
                return fallback;
            }
            auto parsed = fallback;
            PS::JsonHelpers::ParseInteger(source, field, parsed);
            return parsed;
        };

        auto readDouble = [&](const nlohmann::json& source, const std::string& field, double fallback) {
            if (!PS::JsonHelpers::FieldExists(source, field))
            {
                return fallback;
            }
            auto parsed = fallback;
            PS::JsonHelpers::ParseDouble(source, field, parsed);
            return parsed;
        };

        auto readPaths = [&](const nlohmann::json& source, const std::string& field) {
            std::vector<RC::StringType> paths;
            if (!PS::JsonHelpers::FieldExists(source, field))
            {
                return paths;
            }

            const auto& list = source.at(field);
            if (!list.is_array())
            {
                throw std::runtime_error(std::format("'{}' must be an array of asset paths", field));
            }

            for (const auto& entry : list)
            {
                if (!entry.is_string())
                {
                    throw std::runtime_error(std::format("'{}' entries must be asset path strings", field));
                }
                paths.push_back(RC::to_generic_string(entry.get<std::string>()));
            }
            return paths;
        };

        auto readReward = [&](const nlohmann::json& parent, const std::string& field) {
            Reward reward{};
            if (!PS::JsonHelpers::FieldExists(parent, field))
            {
                return reward;
            }

            const auto& source = parent.at(field);
            if (source.is_number_integer())
            {
                reward.XP = source.get<int>();
                return reward;
            }

            if (!source.is_object())
            {
                throw std::runtime_error(std::format("'{}' must be a number or an object", field));
            }

            reward.XP = readInt(source, "XP", 0);
            reward.Recipes = readPaths(source, "Recipes");
            reward.Buildings = readPaths(source, "Buildings");

            if (PS::JsonHelpers::FieldExists(source, "Items"))
            {
                const auto& items = source.at("Items");
                if (!items.is_array())
                {
                    throw std::runtime_error("'Items' must be an array");
                }

                for (const auto& item : items)
                {
                    ItemReward entry{};
                    if (item.is_string())
                    {
                        entry.Path = RC::to_generic_string(item.get<std::string>());
                    }
                    else
                    {
                        PS::JsonHelpers::ValidateFieldExists(item, "Path");
                        entry.Path = readString(item, "Path", {});
                        entry.Count = readInt(item, "Count", 1);
                    }
                    reward.Items.push_back(std::move(entry));
                }
            }

            return reward;
        };

        auto readMedal = [&](const std::string& field) {
            Medal medal{};
            if (!PS::JsonHelpers::FieldExists(value, field))
            {
                return medal;
            }

            const auto& source = value.at(field);
            if (!source.is_object())
            {
                throw std::runtime_error(std::format("'{}' must be an object", field));
            }

            medal.TimeSeconds = readInt(source, "TimeSeconds", 0);
            medal.FirstTime = readReward(source, "FirstTime");
            medal.Repeat = readReward(source, "Repeat");

            if (PS::JsonHelpers::FieldExists(source, "FirstTimeXP"))
            {
                medal.FirstTime.XP = readInt(source, "FirstTimeXP", 0);
            }
            if (PS::JsonHelpers::FieldExists(source, "RepeatXP"))
            {
                medal.Repeat.XP = readInt(source, "RepeatXP", 0);
            }
            return medal;
        };

        auto readLocation = [&](const nlohmann::json& source, FVector& out) {
            if (source.is_array())
            {
                if (source.size() != 3)
                {
                    throw std::runtime_error("a position given as an array must have exactly three numbers");
                }
                out = FVector(source.at(0).get<double>(), source.at(1).get<double>(), source.at(2).get<double>());
                return false;
            }

            PS::JsonHelpers::ValidateFieldExists(source, "Location");
            PS::JsonHelpers::ParseVector(source, "Location", out);
            return true;
        };

        try
        {
            PS::JsonHelpers::ValidateFieldExists(value, "Id");
            PS::JsonHelpers::ValidateFieldExists(value, "StarterLocation");
            PS::JsonHelpers::ValidateFieldExists(value, "Orbs");

            CourseInfo course{};
            course.ModName = modName;
            course.Id = readString(value, "Id", {});
            if (course.Id.empty())
            {
                throw std::runtime_error("'Id' must not be empty");
            }

            course.DisplayName = readString(value, "DisplayName", course.Id);
            course.MinSkillLevel = readInt(value, "MinSkillLevel", 0);
            course.OrbXP = readInt(value, "OrbXP", 10);
            course.RequiredSkills = readPaths(value, "RequiredSkills");

            PS::JsonHelpers::ParseVector(value, "StarterLocation", course.StarterLocation);
            if (PS::JsonHelpers::FieldExists(value, "StarterRotation"))
            {
                PS::JsonHelpers::ParseRotator(value, "StarterRotation", course.StarterRotation);
            }

            course.StartLocation = course.StarterLocation;
            course.StartRotation = course.StarterRotation;
            if (PS::JsonHelpers::FieldExists(value, "StartLocation"))
            {
                PS::JsonHelpers::ParseVector(value, "StartLocation", course.StartLocation);
            }
            if (PS::JsonHelpers::FieldExists(value, "StartRotation"))
            {
                PS::JsonHelpers::ParseRotator(value, "StartRotation", course.StartRotation);
            }

            const auto& orbs = value.at("Orbs");
            if (!orbs.is_array() || orbs.empty())
            {
                throw std::runtime_error("'Orbs' must be a non-empty array");
            }

            for (const auto& orbValue : orbs)
            {
                Orb orb{};
                if (readLocation(orbValue, orb.Location))
                {
                    orb.XP = readInt(orbValue, "XP", -1);
                }
                course.Orbs.push_back(orb);
            }

            if (PS::JsonHelpers::FieldExists(value, "StaminaOrbs"))
            {
                const auto& staminaOrbs = value.at("StaminaOrbs");
                if (!staminaOrbs.is_array())
                {
                    throw std::runtime_error("'StaminaOrbs' must be an array");
                }

                for (const auto& staminaValue : staminaOrbs)
                {
                    StaminaOrb staminaOrb{};
                    if (readLocation(staminaValue, staminaOrb.Location))
                    {
                        staminaOrb.Stamina = readDouble(staminaValue, "Stamina", staminaOrb.Stamina);
                    }
                    course.StaminaOrbs.push_back(staminaOrb);
                }
            }

            if (PS::JsonHelpers::FieldExists(value, "Props"))
            {
                const auto& props = value.at("Props");
                if (!props.is_array())
                {
                    throw std::runtime_error("'Props' must be an array");
                }

                for (const auto& propValue : props)
                {
                    PS::JsonHelpers::ValidateFieldExists(propValue, "ClassPath");
                    PS::JsonHelpers::ValidateFieldExists(propValue, "Location");

                    Prop prop{};
                    prop.ClassPath = readString(propValue, "ClassPath", {});
                    PS::JsonHelpers::ParseVector(propValue, "Location", prop.Location);
                    if (PS::JsonHelpers::FieldExists(propValue, "Rotation"))
                    {
                        PS::JsonHelpers::ParseRotator(propValue, "Rotation", prop.Rotation);
                    }
                    course.Props.push_back(std::move(prop));
                }
            }

            if (PS::JsonHelpers::FieldExists(value, "Zone"))
            {
                const auto& zone = value.at("Zone");
                course.ZonePadding = readDouble(zone, "Padding", course.ZonePadding);

                if (PS::JsonHelpers::FieldExists(zone, "Points"))
                {
                    for (const auto& point : zone.at("Points"))
                    {
                        if (!point.is_array() || point.size() < 2)
                        {
                            throw std::runtime_error("each zone point must be [x, y] or [x, y, z]");
                        }
                        course.ZonePoints.push_back(FVector(point.at(0).get<double>(), point.at(1).get<double>(),
                            point.size() > 2 ? point.at(2).get<double>() : course.StarterLocation.Z()));
                    }
                }
            }

            course.Gold = readMedal("Gold");
            course.Silver = readMedal("Silver");
            course.Bronze = readMedal("Bronze");
            course.NoMedal = readReward(value, "NoMedal");
            if (PS::JsonHelpers::FieldExists(value, "NoMedalXP"))
            {
                course.NoMedal.XP = readInt(value, "NoMedalXP", 0);
            }

            const auto courseReward = readReward(value, "Reward");
            const auto firstTimeReward = readReward(value, "FirstTimeReward");
            auto merge = [](Reward& target, const Reward& source) {
                target.XP += source.XP;
                target.Items.insert(target.Items.end(), source.Items.begin(), source.Items.end());
                target.Recipes.insert(target.Recipes.end(), source.Recipes.begin(), source.Recipes.end());
                target.Buildings.insert(target.Buildings.end(), source.Buildings.begin(), source.Buildings.end());
            };

            for (auto* medal : { &course.Gold, &course.Silver, &course.Bronze })
            {
                merge(medal->Repeat, courseReward);
                merge(medal->FirstTime, courseReward);
                merge(medal->FirstTime, firstTimeReward);
            }
            merge(course.NoMedal, courseReward);

            for (const auto& existing : m_courses)
            {
                if (existing.Id == course.Id)
                {
                    throw std::runtime_error(std::format("duplicate course id '{}'", RC::to_string(course.Id)));
                }
            }

            PS::Log<LogLevel::Verbose>(STR("Added course '{}' for {} with {} orb(s)\n"),
                course.Id, modName, course.Orbs.size());
            m_courses.push_back(std::move(course));
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Invalid course in {}: {}\n"), modName, PS::ToWideSafe(e.what()));
        }
    }

    bool DragonWildsCourseLoader::SetupWorldReadyHook()
    {
        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("CourseLoaderInitGameState");

        m_initGameStateCallbackId = Hook::RegisterInitGameStatePostCallback(
            [this](Hook::TCallbackIterationData<void>&, AGameModeBase* gameMode) {
                if (!gameMode || m_courses.empty())
                {
                    return;
                }

                auto* world = gameMode->GetWorld();
                if (!world || IsFrontEndWorld(world))
                {
                    return;
                }

                m_pendingWorld = world;
                m_runtimeCourses.clear();
                m_secondsSinceWorldReady = 0.0f;
                m_secondsAtLastCheck = 0.0f;
                StartBuildTick();
            },
            options);

        if (m_initGameStateCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(STR("Unable to hook InitGameState; courses will not be built.\n"));
            return false;
        }

        return true;
    }

    void DragonWildsCourseLoader::StartPropTick()
    {
        if (m_propTickCallbackId != Hook::ERROR_ID || m_runtimeCourses.empty())
        {
            return;
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("CourseLoaderProps");

        m_propTickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>& iteration, UEngine*, float deltaSeconds, bool) {
                m_propCheckAccumulator += deltaSeconds;
                if (m_propCheckAccumulator < CheckIntervalSeconds)
                {
                    return;
                }
                m_propCheckAccumulator = 0.0f;

                UpdateRuntimeProps();
                if (m_runtimeCourses.empty())
                {
                    m_propTickCallbackId = Hook::ERROR_ID;
                    iteration.RemoveSelf();
                }
            },
            options);

        if (m_propTickCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(STR("Unable to register the course prop lifecycle tick.\n"));
        }
    }

    void DragonWildsCourseLoader::UpdateRuntimeProps()
    {
        for (auto courseIt = m_runtimeCourses.begin(); courseIt != m_runtimeCourses.end();)
        {
            auto& runtime = *courseIt;
            if (!IsWorldStillLoaded(runtime.World))
            {
                courseIt = m_runtimeCourses.erase(courseIt);
                continue;
            }

            auto visibility = ActorHelper::FunctionCall(runtime.VisibilityComponent,
                STR("/Script/Dominion.AgilityCourseComponent:IsVisible"));
            visibility.Invoke();
            const auto shouldShow = visibility.Result<bool>();
            if (shouldShow == runtime.PropsVisible)
            {
                ++courseIt;
                continue;
            }

            if (shouldShow)
            {
                for (auto& prop : runtime.Props)
                {
                    auto* propClass = ActorHelper::ResolveClass(prop.Definition.ClassPath);
                    if (!propClass)
                    {
                        continue;
                    }
                    prop.Actor = ActorHelper::SpawnActor(runtime.World, propClass,
                        prop.Definition.Location, prop.Definition.Rotation);
                }
            }
            else
            {
                for (auto& prop : runtime.Props)
                {
                    ActorHelper::DestroyActor(prop.Actor);
                    prop.Actor = nullptr;
                }
            }

            runtime.PropsVisible = shouldShow;
            ++courseIt;
        }
    }

    void DragonWildsCourseLoader::StartBuildTick()
    {
        if (m_buildTickCallbackId != Hook::ERROR_ID)
        {
            return;
        }

        Hook::FCallbackOptions options{};
        options.OwnerModName = TEXT("RuneSchema");
        options.HookName = TEXT("CourseLoaderBuild");

        m_buildTickCallbackId = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>& iteration, UEngine*, float deltaSeconds, bool) {
                auto stop = [&] {
                    m_pendingWorld = nullptr;
                    m_buildTickCallbackId = Hook::ERROR_ID;
                    iteration.RemoveSelf();
                };

                if (!m_pendingWorld)
                {
                    stop();
                    return;
                }

                m_secondsSinceWorldReady += deltaSeconds;
                if (m_secondsSinceWorldReady < BuildDelaySeconds
                    || m_secondsSinceWorldReady - m_secondsAtLastCheck < CheckIntervalSeconds)
                {
                    return;
                }
                m_secondsAtLastCheck = m_secondsSinceWorldReady;

                auto* world = m_pendingWorld;
                if (IsWorldStillLoaded(world))
                {
                    m_pendingWorld = nullptr;
                    BuildCourses(world);
                }

                stop();
            },
            options);

        if (m_buildTickCallbackId == Hook::ERROR_ID)
        {
            PS::Log<LogLevel::Error>(STR("Unable to register the course build tick.\n"));
        }
    }

    bool DragonWildsCourseLoader::IsWorldStillLoaded(UWorld* world) const
    {
        if (!world)
        {
            return false;
        }

        auto* worldClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.World"));
        if (!worldClass)
        {
            return false;
        }

        TArray<UObject*> worlds;
        UECustom::UObjectGlobals::GetObjectsOfClass(worldClass, worlds, true, static_cast<EObjectFlags>(0));
        for (auto* candidate : worlds)
        {
            if (candidate == world)
            {
                return true;
            }
        }

        return false;
    }

    bool DragonWildsCourseLoader::PreloadContent()
    {
        for (const auto* path : { OrbClassPath, StarterClassPath, ZoneClassPath, DataAssetClassPath, TargetPointClassPath })
        {
            if (!ActorHelper::ResolveObject(path))
            {
                PS::Log<LogLevel::Error>(STR("Agility content is unavailable ('{}'); no courses will be built.\n"), path);
                return false;
            }
        }

        auto warm = [](const RC::StringType& path, const TCHAR* kind) {
            if (!ActorHelper::ResolveObject(path))
            {
                PS::Log<LogLevel::Warning>(STR("Could not resolve the {} '{}'; it will be granted as nothing.\n"), kind, path);
            }
        };

        auto warmReward = [&](const Reward& reward) {
            for (const auto& item : reward.Items) { warm(item.Path, TEXT("item reward")); }
            for (const auto& recipe : reward.Recipes) { warm(recipe, TEXT("recipe unlock")); }
            for (const auto& building : reward.Buildings) { warm(building, TEXT("building unlock")); }
        };

        for (const auto& course : m_courses)
        {
            if (!course.StaminaOrbs.empty())
            {
                warm(StaminaOrbClassPath, TEXT("stamina orb class"));
            }
            for (const auto& prop : course.Props)
            {
                warm(prop.ClassPath, TEXT("prop class"));
            }
            for (const auto* medal : { &course.Gold, &course.Silver, &course.Bronze })
            {
                warmReward(medal->FirstTime);
                warmReward(medal->Repeat);
            }
            warmReward(course.NoMedal);
        }

        return true;
    }

    void DragonWildsCourseLoader::BuildCourses(UWorld* world)
    {
        if (!PreloadContent())
        {
            return;
        }

        auto succeeded = 0;
        for (const auto& course : m_courses)
        {
            if (BuildCourse(world, course))
            {
                ++succeeded;
            }
        }

        PS::Log<LogLevel::Normal>(STR("Courses: {} built, {} errors.\n"), succeeded, m_courses.size() - succeeded);
        StartPropTick();
    }

    bool DragonWildsCourseLoader::BuildCourse(UWorld* world, const CourseInfo& course)
    {
        try
        {
            auto* orbClass = ActorHelper::ResolveClass(OrbClassPath);
            auto* starterClass = ActorHelper::ResolveClass(StarterClassPath);
            auto* zoneClass = ActorHelper::ResolveClass(ZoneClassPath);
            auto* targetPointClass = ActorHelper::ResolveClass(TargetPointClassPath);
            auto* dataAssetClass = ActorHelper::ResolveClass(DataAssetClassPath);

            if (!orbClass || !starterClass || !zoneClass || !targetPointClass || !dataAssetClass)
            {
                throw std::runtime_error("the agility classes could not be resolved");
            }

            auto* zone = ActorHelper::SpawnActor(world, zoneClass, course.StarterLocation, FRotator{},
                [&](AActor* spawned) {
                    auto* spline = ActorHelper::GetObjectRef(spawned, STR("ZoneSpline"));
                    if (!spline)
                    {
                        throw std::runtime_error("the course zone had no ZoneSpline component");
                    }

                    auto points = course.ZonePoints;
                    if (points.empty())
                    {
                        auto minX = std::numeric_limits<double>::max();
                        auto minY = std::numeric_limits<double>::max();
                        auto maxX = std::numeric_limits<double>::lowest();
                        auto maxY = std::numeric_limits<double>::lowest();
                        auto sumZ = 0.0;

                        auto include = [&](const FVector& point) {
                            minX = std::min(minX, point.X());
                            minY = std::min(minY, point.Y());
                            maxX = std::max(maxX, point.X());
                            maxY = std::max(maxY, point.Y());
                            sumZ += point.Z();
                        };

                        for (const auto& orb : course.Orbs) { include(orb.Location); }
                        include(course.StarterLocation);

                        const auto pad = course.ZonePadding;
                        const auto z = sumZ / static_cast<double>(course.Orbs.size() + 1);
                        points = {
                            FVector(minX - pad, minY - pad, z),
                            FVector(maxX + pad, minY - pad, z),
                            FVector(maxX + pad, maxY + pad, z),
                            FVector(minX - pad, maxY + pad, z),
                        };
                    }

                    if (points.size() < 3)
                    {
                        throw std::runtime_error("a course zone needs at least three boundary points");
                    }

                    ActorHelper::FunctionCall(spline, STR("/Script/Engine.SplineComponent:ClearSplinePoints"))
                        .Arg(STR("bUpdateSpline"), false).Invoke();

                    for (const auto& point : points)
                    {
                        ActorHelper::FunctionCall(spline, STR("/Script/Engine.SplineComponent:AddSplinePoint"))
                            .Arg(STR("position"), point)
                            .Arg(STR("CoordinateSpace"), SplineSpaceWorld)
                            .Arg(STR("bUpdateSpline"), false).Invoke();
                    }

                    ActorHelper::FunctionCall(spline, STR("/Script/Engine.SplineComponent:SetClosedLoop"))
                        .Arg(STR("bInClosedLoop"), true)
                        .Arg(STR("bUpdateSpline"), true).Invoke();
                });

            auto findCourseComponent = [](AActor* actor) -> UObject* {
                // Native orb classes use AgilityCourseComponent. The exported course
                // wall/arrow blueprints name the same component AgilityCourse.
                for (const auto* name : { STR("AgilityCourseComponent"), STR("AgilityCourse") })
                {
                    if (PropertyHelper::GetPropertyByName(actor->GetClassPrivate(), name))
                    {
                        if (auto* component = ActorHelper::GetObjectRef(actor, name))
                        {
                            return component;
                        }
                    }
                }
                return nullptr;
            };

            auto linkToZone = [&](AActor* actor) {
                if (auto* component = findCourseComponent(actor))
                {
                    ActorHelper::SetSoftObjectRef(component, STR("CourseZoneOwner"), zone);
                }
            };

            std::vector<AActor*> orbs;
            orbs.reserve(course.Orbs.size());
            for (const auto& orb : course.Orbs)
            {
                const auto xp = orb.XP >= 0 ? orb.XP : course.OrbXP;
                orbs.push_back(ActorHelper::SpawnActor(world, orbClass, orb.Location, FRotator{},
                    [&](AActor* spawned) {
                        PropertyHelper::CopyJsonValueToContainer(spawned,
                            PropertyHelper::GetPropertyByName(spawned->GetClassPrivate(), STR("XPGainedByPickup")), xp);
                        linkToZone(spawned);
                    }));
            }

            for (size_t i = 0; i + 1 < orbs.size(); ++i)
            {
                ActorHelper::FunctionCall(orbs[i], STR("/Script/Dominion.AgilityOrb:SetNextOrbInSequence"))
                    .SoftObjectArg(STR("AgilityOrb"), orbs[i + 1]).Invoke();
            }

            for (auto* orb : orbs)
            {
                ActorHelper::AddToSoftObjectSet(zone, STR("AgilityCourseActors"), orb);
            }

            auto* startPoint = ActorHelper::SpawnActor(world, targetPointClass, course.StartLocation, course.StartRotation);
            auto* interactionPoint = ActorHelper::SpawnActor(world, targetPointClass, course.StarterLocation, course.StarterRotation);

            auto makeReward = [](const Reward& reward) {
                nlohmann::json out = { { "XPAmount", std::max(0, reward.XP) } };
                if (!reward.Items.empty())
                {
                    auto items = nlohmann::json::array();
                    for (const auto& item : reward.Items)
                    {
                        items.push_back({ { "ItemData", RC::to_string(item.Path) }, { "Count", std::max(1, item.Count) } });
                    }
                    out["ItemRewards"] = std::move(items);
                }
                if (!reward.Recipes.empty())
                {
                    auto recipes = nlohmann::json::array();
                    for (const auto& recipe : reward.Recipes) { recipes.push_back(RC::to_string(recipe)); }
                    out["RecipeUnlocks"] = std::move(recipes);
                }
                if (!reward.Buildings.empty())
                {
                    auto buildings = nlohmann::json::array();
                    for (const auto& building : reward.Buildings) { buildings.push_back(RC::to_string(building)); }
                    out["BuildingPieceUnlocks"] = std::move(buildings);
                }
                return out;
            };

            auto makeMedal = [&](const Medal& medal) {
                return nlohmann::json{
                    { "MedalTime", MakeDuration(medal.TimeSeconds) },
                    { "FirstTimeCourseReward", makeReward(medal.FirstTime) },
                    { "RepeatableCourseReward", makeReward(medal.Repeat) },
                };
            };

            static uint32 sequence = 0;
            auto* dataAsset = ActorHelper::ConstructTransientObject(dataAssetClass,
                std::format(STR("AgilityCourse_{}_{}"), course.Id, ++sequence));

            auto writeProperty = [&](UObject* container, const RC::StringType& name, const nlohmann::json& json) {
                PropertyHelper::CopyJsonValueToContainer(container,
                    PropertyHelper::GetPropertyByName(container->GetClassPrivate(), name), json);
            };

            writeProperty(dataAsset, STR("PersistenceID"), RC::to_string(course.Id));
            writeProperty(dataAsset, STR("InternalName"), RC::to_string(course.Id));

            nlohmann::json timeTrial = {
                { "DisplayName", RC::to_string(course.DisplayName) },
                { "MinSkillLevel", std::clamp(course.MinSkillLevel, 0, 255) },
                { "GoldMedal", makeMedal(course.Gold) },
                { "SilverMedal", makeMedal(course.Silver) },
                { "BronzeMedal", makeMedal(course.Bronze) },
                { "NoMedal", makeReward(course.NoMedal) },
            };

            if (!course.RequiredSkills.empty())
            {
                auto skills = nlohmann::json::array();
                for (const auto& skill : course.RequiredSkills) { skills.push_back(RC::to_string(skill)); }
                timeTrial["RequiredSkills"] = std::move(skills);
            }

            writeProperty(dataAsset, STR("TimeTrialData"), timeTrial);

            ActorHelper::SpawnActor(world, starterClass, course.StarterLocation, course.StarterRotation,
                [&](AActor* starter) {
                    ActorHelper::FunctionCall(starter, STR("/Script/Dominion.AgilityCourseStarter:SetFirstAgilityOrbInSequence"))
                        .SoftObjectArg(STR("AgilityOrb"), orbs.front()).Invoke();

                    ActorHelper::FunctionCall(starter, STR("/Script/Dominion.AgilityCourseStarter:SetAgilityCourseZone"))
                        .Arg(STR("InAgilityCourseZone"), zone).Invoke();

                    ActorHelper::FunctionCall(starter, STR("/Script/Dominion.AgilityCourseStarter:SetCourseStartPoint"))
                        .Arg(STR("TargetPoint"), startPoint).Invoke();

                    ActorHelper::SetSoftObjectRef(starter, STR("CourseInteractionPoint"), interactionPoint);
                    ActorHelper::SetObjectRef(starter, STR("TimeTrialDataAsset"), dataAsset);
                });

            auto staminaPlaced = 0;
            if (!course.StaminaOrbs.empty())
            {
                auto* staminaClass = ActorHelper::ResolveClass(StaminaOrbClassPath);
                if (staminaClass)
                {
                    for (const auto& staminaOrb : course.StaminaOrbs)
                    {
                        auto* placed = ActorHelper::SpawnActor(world, staminaClass, staminaOrb.Location, FRotator{},
                            [&](AActor* spawned) {
                                writeProperty(spawned, STR("StaminaRegainOnCollect"), std::max(0.0, staminaOrb.Stamina));
                                linkToZone(spawned);
                            });

                        ActorHelper::AddToSoftObjectSet(zone, STR("AgilityCourseActors"), placed);
                        ++staminaPlaced;
                    }
                }
            }

            auto propsPlaced = 0;
            RuntimeCourse runtimeProps{};
            runtimeProps.World = world;
            runtimeProps.VisibilityComponent = findCourseComponent(orbs.front());
            for (const auto& prop : course.Props)
            {
                auto* propClass = ActorHelper::ResolveClass(prop.ClassPath);
                if (!propClass || !ActorHelper::IsActorClass(propClass) || ActorHelper::IsAbstract(propClass))
                {
                    PS::Log<LogLevel::Warning>(STR("Course '{}': skipping unusable prop '{}'\n"), course.Id, prop.ClassPath);
                    continue;
                }

                const auto isCourseAware =
                    PropertyHelper::GetPropertyByName(propClass, STR("AgilityCourseComponent"))
                    || PropertyHelper::GetPropertyByName(propClass, STR("AgilityCourse"));

                if (!isCourseAware)
                {
                    runtimeProps.Props.push_back({ prop, nullptr });
                    ++propsPlaced;
                    continue;
                }

                auto* placed = ActorHelper::SpawnActor(world, propClass, prop.Location, prop.Rotation,
                    [&](AActor* spawned) { linkToZone(spawned); });

                ActorHelper::AddToSoftObjectSet(zone, STR("AgilityCourseActors"), placed);
                ++propsPlaced;
            }

            if (!runtimeProps.Props.empty())
            {
                if (!runtimeProps.VisibilityComponent)
                {
                    throw std::runtime_error("the first agility orb had no course visibility component");
                }
                m_runtimeCourses.push_back(std::move(runtimeProps));
            }

            PS::Log<LogLevel::Verbose>(STR("Built course '{}': {} orb(s), {} stamina orb(s), {} prop(s)\n"),
                course.Id, orbs.size(), staminaPlaced, propsPlaced);
            return true;
        }
        catch (const std::exception& e)
        {
            PS::Log<LogLevel::Error>(STR("Course '{}' from {} failed to build: {}\n"),
                course.Id, course.ModName, PS::ToWideSafe(e.what()));
            return false;
        }
    }
}
