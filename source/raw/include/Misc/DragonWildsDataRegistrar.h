#pragma once

#include <string>
#include <filesystem>
#include <set>
#include <utility>
#include <vector>
#include "Unreal/NameTypes.hpp"
#include "Unreal/Hooks.hpp"

namespace RC::Unreal {
    class UObject;
    class UClass;
    class UFunction;
    class FString;
}

namespace DragonWilds {
    class DragonWildsDataRegistrar {
    public:
        // Runs the file-backed, owned-content cleanup as soon as RuneSchema has
        // finished discovering the active mod set.  This deliberately does not
        // depend on a GameInstance or a player controller: character JSON must
        // be clean before the game attempts to deserialize it.
        void PrepareRetiredContent();
        void Initialize();
        void Shutdown();
        ~DragonWildsDataRegistrar() { Shutdown(); }

    private:
        std::vector<std::pair<RC::Unreal::UClass*, RC::Unreal::UClass*>> m_bindings;
        std::vector<std::pair<RC::Unreal::UFunction*, int32_t>> m_functionHooks;
        RC::Unreal::Hook::GlobalCallbackId m_gameStateHook = RC::Unreal::Hook::ERROR_ID;
        bool m_initialized = false;
        bool m_retiredContentPrepared = false;
        // Steam JSON cleanup commits immediately after its atomic file pass.
        // WinGDK must retain the previous ledger until the provider-backed
        // live state has been scrubbed and read-back verified.
        std::filesystem::path m_pendingProviderSnapshot;
        std::set<std::string> m_pendingProviderUnsupportedKinds;
        bool m_providerBlockReported = false;
        bool m_providerPartialReported = false;
        struct RetiredContent {
            RC::Unreal::UObject* Data = nullptr;
            std::string Kind;
            std::string Owner;
            std::string PersistenceID;
        };
        std::vector<RetiredContent> m_retiredContent;

        bool ResolveBindings();
        void ScrubRetiredContent(RC::Unreal::UObject* controller);
        void InstallHooks();
        void RegisterAll();
        void RegisterMissing(RC::Unreal::UClass* dataClass, RC::Unreal::UObject* subsystem);
        int32_t EnsureNetworkIdentity(
            RC::Unreal::UObject* dataAsset, RC::Unreal::UObject* subsystem);
        RC::Unreal::UObject* FindSubsystemInstance(RC::Unreal::UClass* subsystemClass);
        bool InsertIntoMap(RC::Unreal::UObject* subsystem, const RC::StringType& mapName,
            const RC::Unreal::FString& key, RC::Unreal::UObject* value);

    };
}
