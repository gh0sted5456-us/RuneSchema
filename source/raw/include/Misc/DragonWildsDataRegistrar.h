#pragma once

#include <string>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>
#include "Unreal/NameTypes.hpp"
#include "Unreal/Hooks.hpp"
#include "Core/PersistencePruner.h"

namespace RC::Unreal {
    class UObject;
    class UClass;
    class UFunction;
    class UWorld;
    class FString;
}

namespace DragonWilds {
    class DragonWildsDataRegistrar {
    public:
        void Initialize();
        bool IsInitialized() const { return m_initialized; }
        void Shutdown();
        ~DragonWildsDataRegistrar() { Shutdown(); }

    private:
        struct RegistrationStats {
            std::size_t ExistingCookedOrPak = 0;
            std::size_t ExistingRuneSchema = 0;
            std::size_t AddedCookedOrPak = 0;
            std::size_t AddedRuneSchema = 0;
            std::size_t Rejected = 0;
            std::size_t UnresolvedRuneSchema = 0;
        };

        struct RegistryBinding {
            RC::Unreal::UClass* DataClass{};
            RC::Unreal::UClass* SubsystemClass{};
            RC::Unreal::UClass* ExcludedClass{};
            bool CleanupAuthority = false;
            const RC::Unreal::TCHAR* StatusTag{};
        };
        std::vector<RegistryBinding> m_bindings;
        RC::Unreal::Hook::GlobalCallbackId m_characterJsonHook = RC::Unreal::Hook::ERROR_ID;
        RC::Unreal::Hook::GlobalCallbackId m_combatLifecycleHook = RC::Unreal::Hook::ERROR_ID;
        bool m_initialized = false;
        bool m_preflightingCharacterJson = false;
        bool m_registrySummaryReported = false;
        bool m_startupSaveCleanupAttempted = false;
        bool m_additionalWeaponsReadyReported = false;
        bool m_additionalWeaponsIncompleteReported = false;
        std::set<std::string> m_registryStatusReported;
        std::set<std::string> m_registryWaitingReported;
        PS::PersistencePruner m_pruner;
        std::string m_registryCandidateFingerprint;
        unsigned m_registryCandidatePasses = 0;
        std::vector<RC::Unreal::UClass*> m_additionalWeaponAttackClasses;
        std::vector<RC::Unreal::UClass*> m_ownedAdditionalWeaponAttackRoots;

        bool ResolveBindings();
        void PreloadMountedPersistenceAssets();
        void ScrubCharacterJsonBeforeLoad(RC::Unreal::UObject* context,
            RC::Unreal::UFunction* function, void* parameters);
        void InstallHooks();
        void RegisterAll();
        void BootstrapCombatRegistries(RC::Unreal::UWorld* world = nullptr);
        void CleanLocalCharacterSavesOnce();
        bool RegisterMissing(RC::Unreal::UClass* dataClass,
            RC::Unreal::UObject* subsystem,
            RC::Unreal::UClass* excludedClass = nullptr,
            const RC::Unreal::TCHAR* statusTag = nullptr,
            std::size_t* addedCount = nullptr,
            RegistrationStats* stats = nullptr);
        int32_t EnsureNetworkIdentity(
            RC::Unreal::UObject* dataAsset, RC::Unreal::UObject* subsystem);
        RC::Unreal::UObject* FindSubsystemInstance(RC::Unreal::UClass* subsystemClass);
        bool InsertIntoMap(RC::Unreal::UObject* subsystem, const RC::StringType& mapName,
            const RC::Unreal::FString& key, RC::Unreal::UObject* value);

    };
}
