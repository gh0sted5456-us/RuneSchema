#pragma once

#include <vector>
#include "Unreal/Hooks.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class AActor;
    class UObject;
    class UWorld;
}

namespace DragonWilds {
    class DragonWildsCourseLoader : public DragonWildsModLoaderBase {
        struct ItemReward {
            RC::StringType Path;
            int Count = 1;
        };

        struct Reward {
            int XP = 0;
            std::vector<ItemReward> Items;
            std::vector<RC::StringType> Recipes;
            std::vector<RC::StringType> Buildings;
        };

        struct Medal {
            int TimeSeconds = 0;
            Reward FirstTime;
            Reward Repeat;
        };

        struct Orb {
            RC::Unreal::FVector Location{};
            int XP = -1;
        };

        struct StaminaOrb {
            RC::Unreal::FVector Location{};
            double Stamina = 25.0;
        };

        struct Prop {
            RC::StringType ClassPath;
            RC::Unreal::FVector Location{};
            RC::Unreal::FRotator Rotation{};
        };

        struct CourseInfo {
            RC::StringType ModName;
            RC::StringType Id;
            RC::StringType DisplayName;
            int MinSkillLevel = 0;
            int OrbXP = 10;
            std::vector<RC::StringType> RequiredSkills;

            RC::Unreal::FVector StarterLocation{};
            RC::Unreal::FRotator StarterRotation{};
            RC::Unreal::FVector StartLocation{};
            RC::Unreal::FRotator StartRotation{};

            std::vector<Orb> Orbs;
            std::vector<StaminaOrb> StaminaOrbs;
            std::vector<Prop> Props;

            std::vector<RC::Unreal::FVector> ZonePoints;
            double ZonePadding = 2500.0;

            Medal Gold;
            Medal Silver;
            Medal Bronze;
            Reward NoMedal;
        };

        struct RuntimeProp {
            Prop Definition;
            RC::Unreal::AActor* Actor = nullptr;
        };

        struct RuntimeCourse {
            RC::Unreal::UWorld* World = nullptr;
            RC::Unreal::UObject* VisibilityComponent = nullptr;
            std::vector<RuntimeProp> Props;
            bool PropsVisible = false;
        };

    public:
        DragonWildsCourseLoader();

        ~DragonWildsCourseLoader() override;

    protected:
        virtual void OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual void OnAutoReload(const RC::StringType& modName, const std::filesystem::path& modFilePath) override final;

        virtual bool CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase) override final;
        virtual bool OnInitialize() override final;

    private:
        std::vector<CourseInfo> m_courses;
        std::vector<RuntimeCourse> m_runtimeCourses;
        RC::Unreal::UWorld* m_pendingWorld = nullptr;
        float m_secondsSinceWorldReady = 0.0f;
        float m_secondsAtLastCheck = 0.0f;
        RC::Unreal::Hook::GlobalCallbackId m_initGameStateCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_buildTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_propTickCallbackId = RC::Unreal::Hook::ERROR_ID;
        float m_propCheckAccumulator = 0.0f;

        void LoadCourses(const nlohmann::json& data, const RC::StringType& modName);
        void RegisterCourse(const nlohmann::json& value, const RC::StringType& modName);

        bool SetupWorldReadyHook();
        void StartBuildTick();
        void StartPropTick();
        void UpdateRuntimeProps();
        bool IsWorldStillLoaded(RC::Unreal::UWorld* world) const;
        void BuildCourses(RC::Unreal::UWorld* world);
        bool BuildCourse(RC::Unreal::UWorld* world, const CourseInfo& course);
        bool PreloadContent();
    };
}
