#pragma once

#include <cstdint>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Unreal/Hooks.hpp"

namespace RC::Unreal {
    class AGameModeBase;
    class AActor;
    class UObject;
    class UClass;
    class UFunction;
    class UEngine;
    class FIntProperty;
    class FStrProperty;
}

namespace PS::Network {
class RegistryBridge {
public:
    using AuthorityChannelHandler = std::function<std::string(
        RC::Unreal::UObject*, const std::string&, const std::string&, const std::string&)>;
    using GenericAuthorityHandler = std::function<std::string(
        RC::Unreal::UObject*, const std::string&, const std::string&, const std::string&,const std::string&)>;
    using ClientNotificationHandler = std::function<void(
        RC::Unreal::UObject*, const std::string&, const std::string&, const std::string&)>;

    void Start();
    void Stop();
    // Receives the validated, merged mod-owned registry contract directly
    // from the registry loader. No runtime manifest file is involved.
    void SetRegistrySnapshot(std::string snapshot);
    ~RegistryBridge() { Stop(); }

    // Authority-only transport entry points for captured presentation events.
    // Payloads are compact JSON objects whose keys resolve through the local
    // manifest. They never carry or execute gameplay authority.
    bool PublishActivation(const std::string& envelope);
    bool PublishPersistentState(const std::string& envelope);
    // Durable, authority-owned world state. Each loader contributes a bounded
    // record keyed by stable instance id; the bridge republishes one snapshot
    // so late joiners receive the same state as connected clients.
    bool UpsertWorldInstance(const std::string& instanceId,const std::string& record);
    bool RemoveWorldInstance(const std::string& instanceId,const std::string& reason = "removed");
    // Sends a bounded request through the bridge component owned by the local
    // player. The server remains the only executor and applies its own policy.
    bool RequestAuthority(const std::string& action,const std::string& payload);

    // Optional authority services are supplied by their owning loaders. The
    // transport validates/rate-limits the envelope; the service revalidates
    // gameplay state and returns a bounded receipt detail.
    AuthorityChannelHandler QuestControl;
    GenericAuthorityHandler AuthorityChannel;
    ClientNotificationHandler ClientNotification;
    ClientNotificationHandler PersistentState;

private:
    struct Contract {
        RC::Unreal::UClass* Type = nullptr;
        // 0.7.0 can use the compact, proven IdentityPayload RepNotify contract
        // shipped by RuneSchemaEssentials.  The expanded fields remain
        // supported for forward-compatible cooked bridge assets.
        bool Compact = false;
        RC::Unreal::FStrProperty* IdentityPayload = nullptr;
        RC::Unreal::FIntProperty* ProtocolVersion = nullptr;
        RC::Unreal::FStrProperty* RegistryFingerprint = nullptr;
        RC::Unreal::FIntProperty* RegistryRevision = nullptr;
        RC::Unreal::FStrProperty* ActivationEnvelope = nullptr;
        RC::Unreal::FIntProperty* ActivationRevision = nullptr;
        RC::Unreal::FStrProperty* PersistentStateEnvelope = nullptr;
        RC::Unreal::FIntProperty* PersistentStateRevision = nullptr;
    };
    struct WorldContract {
        RC::Unreal::UClass* Type = nullptr;
        RC::Unreal::FIntProperty* ProtocolVersion = nullptr;
        RC::Unreal::FStrProperty* WorldStateEnvelope = nullptr;
        RC::Unreal::FIntProperty* WorldStateRevision = nullptr;
        RC::Unreal::FStrProperty* ActivationEnvelope = nullptr;
        RC::Unreal::FIntProperty* ActivationRevision = nullptr;
    };

    RC::Unreal::Hook::GlobalCallbackId m_worldStarting = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_worldReady = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_authorityPre = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_processEvent = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_retryTick = RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::UObject* m_authorityComponent = nullptr;
    RC::Unreal::UObject* m_worldAuthorityComponent = nullptr;
    RC::Unreal::AGameModeBase* m_pendingMode = nullptr;
    float m_retryElapsed = 0.0f;
    float m_retryInterval = 0.0f;
    float m_playerBridgeInterval = 0.0f;
    uint32_t m_seenRegistryRevision = 0;
    uint32_t m_seenActivationRevision = 0;
    uint32_t m_seenPersistentRevision = 0;
    bool m_started = false;
    struct AuthorityAction {
        std::string Action;
        std::string DataAsset;
        std::string Key;
        std::string GraphClass;
        std::string EntryFunction = "Trigger";
        std::unordered_map<std::string,std::string> Bindings;
    };
    std::unordered_map<std::string,AuthorityAction> m_authorityActions;
    // Exact cooked asset identities which select an authoritative registry
    // action.  The client resolves these from its live player components at
    // the attack notify boundary; paths are never accepted from the wire.
    std::unordered_map<std::string,std::string> m_authoritySelectionPaths;
    RC::Unreal::UObject* m_activeAuthorityGraph = nullptr;
    bool m_nativeAuthorityActionObserved = false;
    std::string m_manifestFingerprint;
    std::string m_registrySnapshot;
    std::string m_activationEnvelope;
    std::string m_persistentStateEnvelope;
    uint32_t m_registryRevision = 0;
    uint32_t m_activationRevision = 0;
    uint32_t m_persistentRevision = 0;
    int64_t m_outboundRevision = 0;
    struct RequestWindow {std::chrono::steady_clock::time_point Started{};uint32_t Count=0;int64_t Revision=0;};
    std::unordered_map<std::string,RequestWindow> m_requestWindows;
    std::unordered_map<std::string,std::string> m_worldInstances;
    uint64_t m_worldLedgerRevision = 0;

    Contract ResolveContract() const;
    WorldContract ResolveWorldContract() const;
    RC::Unreal::UObject* EnsureWorldComponent(RC::Unreal::AActor* actor);
    void LoadAuthorityActions();
    void EnsurePlayerBridges();
    RC::Unreal::UObject* EnsureBridgeComponent(RC::Unreal::AActor* actor,bool publishRegistry);
    bool Attach(RC::Unreal::AGameModeBase* mode);
    void RetryAttach(float deltaSeconds);
    void Observe(RC::Unreal::UObject* source, RC::Unreal::UFunction* function);
    void ObserveAuthorityPre(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
    void ObserveSelectionNotify(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
    void ObserveAuthorityPost(RC::Unreal::UObject* source,RC::Unreal::UFunction* function);
    void HandleGenericRequest(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
    void ObserveClientTransport(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters);
    void SendReceipt(RC::Unreal::UObject* component,const std::string& channel,const std::string& entity,int64_t revision,bool success,const std::string& detail);
    void SendNotification(RC::Unreal::UObject* component,const std::string& channel,const std::string& entity,int64_t revision,const std::string& payload);
    void ForwardRegistryRequest(RC::Unreal::UObject* component,const std::string& key);
    void SendCompatibilityAck(const std::string& remoteFingerprint);
    void InvokeAuthorityAction(RC::Unreal::UObject* graph,const AuthorityAction& action);
    void InvokeAuthorityActionForCaster(RC::Unreal::UObject* caster,const AuthorityAction& action);
    std::string CompactPayload() const;
    void PublishCompactPayload();
    bool PublishWorldSnapshot();
    void RecordDiagnostic(const std::string& channel,const std::string& entity,int64_t revision,
        const std::string& result,const std::string& detail) const;
    void ResetWorld();
};
}
