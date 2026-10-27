#include "Runtime/RegistryBridge.h"
#include "Runtime/SlowWork.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "nlohmann/json.hpp"
#include "Runtime/HostServices.h"
#include "Runtime/MappingBackbone.h"
#include "Runtime/NetworkRoleNotice.h"
#include "Runtime/RegistryManifestSummary.h"
#include "Runtime/Storefront.h"
#include "Core/NetworkIdentityState.h"
#include "Core/PlayerStatus.h"
#include "Core/PartyStatWire.h"
#include "Generator/ToolRequest.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FManagedStruct.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3D.h"
#include "Utility/Config.h"
#include "Utility/BuildInfo.h"
#include "Unreal/AActor.hpp"
#include "Unreal/AGameModeBase.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/World.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace {
constexpr auto BridgeClass = TEXT("/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C");
constexpr auto WorldBridgeClass = TEXT("/RuneSchema/Networking/BPC_RuneSchemaWorldBridge.BPC_RuneSchemaWorldBridge_C");
constexpr auto PluginPresentationClass = TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginPresentation.BPC_RuneSchemaPluginPresentation_C");
constexpr auto DamageNotifyClass = TEXT("/Game/Gameplay/Character/Player/AnimNotifies/AnimNotify_AttackDamagePoint.AnimNotify_AttackDamagePoint_C");
constexpr int32_t Protocol = 1;
constexpr size_t MaxEnvelopeBytes = 16 * 1024;
constexpr size_t MaxActionPayloadBytes = 4 * 1024;
constexpr uint32_t MaxRequestsPerSecond = 8;
constexpr size_t MaxWorldInstances = 512;
constexpr size_t MaxPresentationRoutes = 128;
// The main and experimental packages at one numeric release share the same
// registry protocol; the channel suffix is diagnostic, not a parity mismatch.
const std::string BuildIdentity{PS::BuildInfo::Version};

struct BridgeFunctionNames {
    const FName Acknowledge{TEXT("ServerAcknowledgeRuneSchemaPresentation"),FNAME_Add}, Resync{TEXT("ServerRequestRuneSchemaResync"),FNAME_Add},
        Presentation{TEXT("MulticastRuneSchemaPluginPresentation"),FNAME_Add}, ClientPresentation{TEXT("ClientRuneSchemaPluginPresentation"),FNAME_Add},
        Request{TEXT("ServerRequestRuneSchemaAction"),FNAME_Add}, LegacyRequest{TEXT("ServerRequestRegistryAction"),FNAME_Add},
        Receipt{TEXT("ClientRuneSchemaReceipt"),FNAME_Add}, Notification{TEXT("ClientRuneSchemaNotification"),FNAME_Add},
        Identity{TEXT("OnRep_IdentityPayload"),FNAME_Add}, Registry{TEXT("OnRep_RegistryRevision"),FNAME_Add},
        Activation{TEXT("OnRep_ActivationRevision"),FNAME_Add}, Persistent{TEXT("OnRep_PersistentStateRevision"),FNAME_Add}, World{TEXT("OnRep_WorldStateRevision"),FNAME_Add},
        BeginPlay{TEXT("ReceiveBeginPlay"),FNAME_Add}, Damage{TEXT("Received_Notify"),FNAME_Add}, Removal{TEXT("RemoveItemByData"),FNAME_Add};
    bool Pre(FName name) const {return name==Acknowledge||name==Resync||name==Presentation||name==Request||name==LegacyRequest||name==Damage;}
    bool Post(FName name) const {return name==Presentation||name==ClientPresentation||name==Receipt||name==Notification||name==Identity||name==Registry
        ||name==Activation||name==Persistent||name==World||name==BeginPlay||name==Removal;}
};
const BridgeFunctionNames& Names(){static const BridgeFunctionNames names;return names;}

bool ExactClass(UObject* source,const TCHAR* path,PS::WeakObjectHandle& cached) {
    auto* actual=source->GetClassPrivate();
    if(actual&&actual==cached.Get())return true;
    if(!actual||actual->GetPathName()!=path)return false;
    cached.Assign(actual);return true;
}
bool ExactClass(UObject* source,const std::string& path,PS::WeakObjectHandle& cached) {
    auto* actual=source->GetClassPrivate();
    return actual&&(actual==cached.Get()||ExactClass(source,PS::ToWideSafe(path.c_str()).c_str(),cached));
}
enum class BridgeKind {Player,World,Presentation,Damage};
template<BridgeKind Kind> bool BridgeClassMatches(UObject* source) {
    static thread_local PS::WeakObjectHandle cached;
    constexpr std::array paths{BridgeClass,WorldBridgeClass,PluginPresentationClass,DamageNotifyClass};
    return ExactClass(source,paths[static_cast<size_t>(Kind)],cached);
}

PS::MappingBackbone::Mapping LocalMapping() {
    return PS::MappingBackbone::Current(PS::HostServices::WorkingDirectory());
}

std::string MappingState(const std::string& local,const std::string& remote) {
    if(local.empty()&&remote.empty())return "unavailable";
    if(local.empty())return "authority-only";
    if(remote.empty())return "client-only";
    return local==remote?"match":"different";
}

std::string Fingerprint(const std::string& bytes) {
    // A deterministic content identity is sufficient here: clients receive no
    // trust from this value; they only compare it with their installed file.
    uint64_t value=14695981039346656037ull;
    for(const auto byte:bytes){value^=static_cast<unsigned char>(byte);value*=1099511628211ull;}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<value;
    return out.str();
}

std::string EffectiveRegistryFingerprint(const std::string& snapshot) {
    return Fingerprint(snapshot + "\nnetwork-identities="
        + PS::NetworkIdentityState::Fingerprint());
}

PS::RegistryManifestSummary::Fingerprints OwnerFingerprints(
    const nlohmann::json& state,const char* key)
{
    PS::RegistryManifestSummary::Fingerprints result;
    if(!state.contains(key))return result;
    const auto& owners=state.at(key);
    if(!owners.is_object()||owners.size()>256)
        throw std::runtime_error(std::string(key)+" must be a bounded owner fingerprint object");
    for(const auto& [owner,fingerprint]:owners.items()) {
        if(owner.empty()||owner.size()>(std::string_view(key)=="networkIdentityLanes"?512:96)||!fingerprint.is_string()
            ||fingerprint.get_ref<const std::string&>().empty()||fingerprint.get_ref<const std::string&>().size()>96)
            throw std::runtime_error(std::string(key)+" contains an invalid owner fingerprint");
        result.emplace(owner,fingerprint.get<std::string>());
    }
    return result;
}

std::string AssetPackage(std::string path) {
    const auto slash=path.find_last_of('/');
    const auto dot=path.find('.',slash==std::string::npos?0:slash);
    if(dot!=std::string::npos)path.resize(dot);
    return path;
}

UObject* ControllerPawn(UObject* controller) {
    if(!controller)return nullptr;
    DragonWilds::ActorHelper::FunctionCall pawn(controller,TEXT("/Script/Engine.Controller:K2_GetPawn"));
    pawn.Invoke();return pawn.Result<UObject*>();
}

bool ActorAuthority(UObject* actor) {
    if(!actor||!actor->IsA<AActor>())return false;
    DragonWilds::ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));
    authority.Invoke();return authority.Result<bool>();
}

bool LocalController(UObject* controller) {
    if(!controller)return false;
    DragonWilds::ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));
    local.Invoke();return local.Result<bool>();
}

template<class T> T* Field(UClass* type,const TCHAR* name,EPropertyFlags required) {
    auto* field=type?CastField<T>(DragonWilds::PropertyHelper::GetPropertyByName(type,name)):nullptr;
    if(!field || field->GetArrayDim()!=1 || !field->HasAllPropertyFlags(required))
        throw std::runtime_error(std::format("Registry bridge field '{}' does not match the cooked contract",RC::to_string(name)));
    return field;
}

template<class T> T* OptionalField(UClass* type,const TCHAR* name,EPropertyFlags required) {
    auto* field=type?CastField<T>(DragonWilds::PropertyHelper::GetPropertyByName(type,name)):nullptr;
    return field && field->GetArrayDim()==1 && field->HasAllPropertyFlags(required)?field:nullptr;
}

int32_t ReadInt(UObject* object,FIntProperty* field) {
    return field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(object));
}

std::string ReadString(UObject* object,FStrProperty* field) {
    const auto& value=field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(object));
    const auto& chars=value.GetCharArray();
    if(chars.Num()==0)return {};
    if(chars.Num()<0 || !chars.GetData() || chars.GetData()[chars.Num()-1]!=0 || chars.Num()>static_cast<int32_t>(MaxEnvelopeBytes+1))
        throw std::runtime_error("Registry bridge string layout or size is invalid");
    return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
}

void WriteString(UObject* object,FStrProperty* field,const std::string& value) {
    if(value.size()>MaxEnvelopeBytes)throw std::runtime_error("Registry bridge envelope exceeds 16 KiB");
    DragonWilds::PropertyHelper::CopyJsonValueToContainer(object,field,value);
}

void WriteInt(UObject* object,FIntProperty* field,int32_t value) {
    field->SetPropertyValue(field->ContainerPtrToValuePtr<void>(object),value);
}

void WriteObject(UObject* owner,FObjectPropertyBase* base,UObject* value,const std::string& label) {
    auto* field=CastField<FObjectProperty>(base);
    if(!owner || !field || field->GetArrayDim()!=1 || field->GetElementSize()!=sizeof(UObject*)
        || !value || !value->IsA(field->GetPropertyClass()))
        throw std::runtime_error("Registry authority object binding does not match: "+label);
    auto* address=field->ContainerPtrToValuePtr<void>(owner);
    std::memcpy(address,&value,sizeof(value));
    UObject* current=nullptr;std::memcpy(&current,address,sizeof(current));
    if(current!=value)throw std::runtime_error("Registry authority object binding did not round-trip: "+label);
}

template<class T> T* Parameter(UFunction* function,const TCHAR* name) {
    if(!function)return nullptr;const FName expected(name,FNAME_Find);
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
        if(field->GetFName()==expected&&field->HasAnyPropertyFlags(CPF_Parm)&&!field->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)&&field->GetArrayDim()==1)
            return CastField<T>(field);
    return nullptr;
}

std::string ParameterString(UFunction* function,void* parameters,const TCHAR* name,size_t limit) {
    auto* field=Parameter<FStrProperty>(function,name);if(!field||!parameters)throw std::runtime_error("Generic bridge string parameter contract changed");
    const auto& value=field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));const auto& chars=value.GetCharArray();
    if(chars.Num()<1||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0||chars.Num()>static_cast<int32_t>(limit+1))throw std::runtime_error("Generic bridge string parameter is invalid");
    return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
}

int64_t ParameterInt64(UFunction* function,void* parameters,const TCHAR* name) {
    auto* field=Parameter<FInt64Property>(function,name);if(!field||!parameters)throw std::runtime_error("Generic bridge revision contract changed");
    return field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
}

bool ParameterBool(UFunction* function,void* parameters,const TCHAR* name) {
    auto* field=Parameter<FBoolProperty>(function,name);if(!field||!parameters)throw std::runtime_error("Generic bridge boolean contract changed");
    return field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
}

bool Token(const std::string& value,size_t limit) {
    if(value.empty()||value.size()>limit)return false;
    return std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='.'||c==':'||c=='_'||c=='-';});
}

std::string PresentationRouteKey(const std::string& pluginId,const std::string& connection) {
    return pluginId+"|"+connection;
}

bool ValidatePresentationValue(const nlohmann::json& value,size_t depth,size_t& fields) {
    if(depth>8)return false;
    if(value.is_object()) {
        if((fields+=value.size())>32)return false;
        for(const auto& [_,child]:value.items())if(!ValidatePresentationValue(child,depth+1,fields))return false;
    } else if(value.is_array()) {
        if((fields+=value.size())>64)return false;
        for(const auto& child:value)if(!ValidatePresentationValue(child,depth+1,fields))return false;
    } else if(value.is_string()) {
        const auto text=value.get<std::string>();
        if(text.size()>512 || (!text.empty()&&text.front()=='/')
            || text.find("/Game/")!=std::string::npos || text.find("/Script/")!=std::string::npos)
            return false;
    }
    return true;
}
}

namespace PS::Network {
RegistryBridge::Contract RegistryBridge::ResolveContract() const {
    Contract result;
    result.Type=DragonWilds::ActorHelper::ResolveClass(BridgeClass);
    auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    if(!result.Type || !base || !result.Type->IsChildOf(base) || DragonWilds::ActorHelper::IsAbstract(result.Type))
        throw std::runtime_error("Cooked RuneSchema registry bridge component is unavailable");
    result.IdentityPayload=OptionalField<FStrProperty>(result.Type,TEXT("IdentityPayload"),CPF_Net|CPF_RepNotify);
    if(result.IdentityPayload) {
        result.Compact=true;
        return result;
    }
    result.ProtocolVersion=Field<FIntProperty>(result.Type,TEXT("ProtocolVersion"),CPF_Net);
    result.RegistryFingerprint=Field<FStrProperty>(result.Type,TEXT("RegistryFingerprint"),CPF_Net|CPF_RepNotify);
    result.RegistryRevision=Field<FIntProperty>(result.Type,TEXT("RegistryRevision"),CPF_Net|CPF_RepNotify);
    result.ActivationEnvelope=Field<FStrProperty>(result.Type,TEXT("ActivationEnvelope"),CPF_Net|CPF_RepNotify);
    result.ActivationRevision=Field<FIntProperty>(result.Type,TEXT("ActivationRevision"),CPF_Net|CPF_RepNotify);
    result.PersistentStateEnvelope=Field<FStrProperty>(result.Type,TEXT("PersistentStateEnvelope"),CPF_Net|CPF_RepNotify);
    result.PersistentStateRevision=Field<FIntProperty>(result.Type,TEXT("PersistentStateRevision"),CPF_Net|CPF_RepNotify);
    return result;
}

RegistryBridge::WorldContract RegistryBridge::ResolveWorldContract() const {
    WorldContract result;result.Type=DragonWilds::ActorHelper::ResolveClass(WorldBridgeClass);
    auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    if(!result.Type||!base||!result.Type->IsChildOf(base)||DragonWilds::ActorHelper::IsAbstract(result.Type))
        throw std::runtime_error("Cooked RuneSchema world bridge component is unavailable");
    result.ProtocolVersion=Field<FIntProperty>(result.Type,TEXT("ProtocolVersion"),CPF_Net);
    result.WorldStateEnvelope=Field<FStrProperty>(result.Type,TEXT("WorldStateEnvelope"),CPF_Net);
    result.WorldStateRevision=Field<FIntProperty>(result.Type,TEXT("WorldStateRevision"),CPF_Net|CPF_RepNotify);
    result.ActivationEnvelope=Field<FStrProperty>(result.Type,TEXT("ActivationEnvelope"),CPF_Net);
    result.ActivationRevision=Field<FIntProperty>(result.Type,TEXT("ActivationRevision"),CPF_Net|CPF_RepNotify);
    return result;
}

UClass* RegistryBridge::ResolvePresentationClass() const {
    auto* type=DragonWilds::ActorHelper::ResolveClass(PluginPresentationClass);
    auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    if(!type||!base||!type->IsChildOf(base)||DragonWilds::ActorHelper::IsAbstract(type))
        throw std::runtime_error("Cooked RuneSchema plugin presentation component is unavailable");
    return type;
}

void RegistryBridge::ResetWorld() {
    for(const auto& pending:m_pendingClientActions)Trace("consumable.authority",pending.Caster.Get(),pending.Key,"consume",pending.Revision,pending.Correlation,
        "world_reset","cancelled","world_changed",pending.Retries,0,true);
    for(const auto& pending:m_pendingAuthorityActions)Trace("consumable.authority",pending.Caster.Get(),pending.Key,"consume",pending.Revision,pending.Correlation,
        "world_reset","cancelled","world_changed",0,0,true);
    try {
        const auto summary=m_diagnostics.Snapshot();
        if(summary.value("worldEpoch",uint64_t{})!=0) {
            const auto path=HostServices::ExportsDirectory()/"NetworkBridgeDiagnostics.json";
            std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary|std::ios::trunc)<<summary.dump(2);
            auto health=summary;health.erase("events");
            PS::Log<LogLevel::Normal>(STR("Registry bridge health: {}\n"),PS::ToWideSafe(health.dump().c_str()));
        }
    }catch(...) {}
    m_authorityComponent=nullptr;m_worldAuthorityComponent=nullptr;m_presentationAuthorityComponent=nullptr;
    m_pendingMode.Reset();
    m_helpyRosterComponent.Reset();m_helpyRosterRevision=0;m_helpyRosterGeneration=0;
    m_retryElapsed=m_retryInterval=0.0f;
    m_seenRegistryRevision=m_seenActivationRevision=m_seenPersistentRevision=0;
    m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;
    m_manifestFingerprint.clear();m_activationEnvelope.clear();m_persistentStateEnvelope.clear();
    m_sessionRegistryOwners.clear();m_sessionNetworkOwners.clear();m_sessionAuthorityFingerprint.clear();m_sessionComparisonFingerprint.clear();
    m_registryRevision=m_activationRevision=m_persistentRevision=0;
    m_outboundRevision=0;m_activeRequestRevision=0;m_activeRequestCorrelation.clear();m_requestWindows.clear();m_worldInstances.clear();m_worldLedgerRevision=0;
    m_consumptionPermits.clear();m_pendingAuthorityActions.clear();m_pendingClientActions.clear();m_lastAuthorityDeferred=false;
    m_authorityGraphRoutes.clear();
    m_modDataRelay.Reset();m_relayPeers.clear();m_relayElapsed=0;
    m_presentationConsumers.clear();m_presentationAuthorityRevisions.clear();m_presentationClientRevisions.clear();
    m_diagnosticOwners.clear();m_nextDiagnosticOwner=0;
    const auto role=Storefront::IsDedicatedServer()?"dedicated-server":"listen-server";
    const auto& mapping=LocalMapping();
    m_diagnostics.BeginWorld(role,BuildIdentity,Protocol,EffectiveRegistryFingerprint(m_registrySnapshot),mapping.Available?"available":"unavailable",
        PS::PSConfig::Get()->GetSettings().bridgeDiagnostics.verbose);
}

std::map<std::string, std::string> RegistryBridge::RegistryOwnerFingerprints() const {
    return m_registrySnapshot.empty() ? std::map<std::string, std::string>{}
        : PS::RegistryManifestSummary::Owners(m_registrySnapshot);
}

void RegistryBridge::SetRegistrySnapshot(std::string snapshot) {
    if(snapshot.size()>4*1024*1024)throw std::runtime_error("Merged mod registry exceeds 4 MiB");
    const auto parsed=nlohmann::json::parse(snapshot);
    if(!parsed.is_object() || parsed.value("kind",std::string{})!="RuneSchemaRegistryBridgeManifest"
        || parsed.value("schemaVersion",0)!=1 || parsed.value("protocolVersion",0)!=Protocol
        || !parsed.contains("entries") || !parsed["entries"].is_array())
        throw std::runtime_error("Merged mod registry contract is invalid");
    std::vector<PS::ModDataRelay::Channel> relayChannels;
    for(const auto& entry:parsed["entries"])if(entry.value("kind",std::string{})=="DataRelay")relayChannels.push_back(PS::ModDataRelay::Declaration(entry.at("owner").get<std::string>(),entry.at("relay")));
    m_modDataRelay.Configure(relayChannels);
    m_registrySnapshot=std::move(snapshot);
    m_manifestFingerprint=EffectiveRegistryFingerprint(m_registrySnapshot);
    Trace("registry.discovery",nullptr,"manifest","validate",0,{},"cooked_registry_validated","accepted","accepted");
    LoadAuthorityActions();
    LoadPresentationRoutes();
    // Snapshot publication happens once per registry rebuild. Catch players
    // already present at that boundary; later pawns attach from BeginPlay.
    try{EnsurePlayerBridges();}catch(...){}
}

std::string RegistryBridge::CompactPayload() const {
    const auto& mapping=LocalMapping();
    nlohmann::json payload={{"kind","RuneSchemaRegistryBridgeState"},{"protocolVersion",Protocol},
        {"build",BuildIdentity},{"channels",{"system.compat","registry.action","quest.control","quest.state","dialogue.session","event.state","spawn.state","npc.state","vendor.state","player.profile","nameplate.profile","world.state","notify.player"}},
        {"registryFingerprint",m_manifestFingerprint},{"mappingFingerprint",mapping.Fingerprint},{"registryRevision",m_registryRevision},
        {"activationRevision",m_activationRevision},{"persistentStateRevision",m_persistentRevision}};
    payload["sessionPolicy"]={{"mode","server-baseline-permissive"},
        {"clientExtras","allowed"},{"missingContent","allowed"},
        {"actionValidation","independent"},{"sharedRegistries","authority-first-warn"}};
    payload["registryOwners"]=PS::RegistryManifestSummary::Owners(m_registrySnapshot);
    payload["networkIdentityLanes"]=PS::NetworkIdentityState::Snapshot();
    payload["networkIdentityOwners"]=PS::NetworkIdentityState::OwnerSnapshot();
    payload["nativeIdAuthority"]=PS::NetworkIdentityState::GetAuthorityOrderProof();
    if(!m_activationEnvelope.empty())payload["activation"]=nlohmann::json::parse(m_activationEnvelope);
    if(!m_persistentStateEnvelope.empty())payload["persistentState"]=nlohmann::json::parse(m_persistentStateEnvelope);
    return payload.dump();
}

void RegistryBridge::PublishCompactPayload() {
    if(!m_authorityComponent)return;
    const auto contract=ResolveContract();
    if(!contract.Compact)throw std::runtime_error("Compact registry bridge payload requested for expanded contract");
    WriteString(m_authorityComponent,contract.IdentityPayload,CompactPayload());
}

void RegistryBridge::LoadAuthorityActions() {
    if(m_registrySnapshot.empty())throw std::runtime_error("Merged mod registry snapshot is unavailable");
    const auto parsed=nlohmann::json::parse(m_registrySnapshot);
    if(!parsed.is_object() || !parsed.contains("entries") || !parsed["entries"].is_array())
        throw std::runtime_error("Registry authority manifest is unavailable");
    m_authorityActions.clear();m_authoritySelectionPaths.clear();m_authorityEntryNames.clear();m_authorityGraphRoutes.clear();
    for(const auto& entry:parsed["entries"]) {
        if(!entry.contains("authority") || !entry["authority"].is_object())continue;
        const auto& authority=entry["authority"];
        const auto graph=authority.value("graphClass",std::string{}),action=authority.value("action",std::string{}),asset=authority.value("dataAsset",std::string{});
        if(graph.empty() || (action!="SpawnFollower" && action!="ExecuteGraph" && action!="ConsumedItemAuthority")
            || ((action=="SpawnFollower"||action=="ConsumedItemAuthority") && asset.empty()))
            throw std::runtime_error("Registry authority action is malformed");
        AuthorityAction candidate{action,asset,entry.value("key",std::string{}),graph,authority.value("function",std::string{"Trigger"}),{}};
        candidate.EntryName=FName(PS::ToWideSafe(candidate.EntryFunction.c_str()).c_str(),FNAME_Add);
        if(std::find(m_authorityEntryNames.begin(),m_authorityEntryNames.end(),candidate.EntryName)==m_authorityEntryNames.end())m_authorityEntryNames.push_back(candidate.EntryName);
        if(authority.contains("bindings") && authority["bindings"].is_object())
            for(const auto& [property,path]:authority["bindings"].items())candidate.Bindings.emplace(property,path.get<std::string>());
        if(action=="SpawnFollower")candidate.Bindings["Follower Data Asset"]=asset;
        if(candidate.Key.empty())throw std::runtime_error("Registry authority action requires a namespaced entry key");
        const auto [stored,inserted]=m_authorityActions.emplace(candidate.Key,candidate);
        if(!inserted && (stored->second.Action!=candidate.Action || stored->second.DataAsset!=candidate.DataAsset
            || stored->second.GraphClass!=candidate.GraphClass || stored->second.EntryFunction!=candidate.EntryFunction
            || stored->second.Bindings!=candidate.Bindings))
            throw std::runtime_error("Registry authority entry key has conflicting definitions");
        const auto remember=[this,&candidate](const std::string& path){
            if(path.empty())return;
            const auto [at,added]=m_authoritySelectionPaths.emplace(AssetPackage(path),candidate.Key);
            if(!added && at->second!=candidate.Key) {
                const auto prior=m_authorityActions.find(at->second);
                if(prior==m_authorityActions.end() || prior->second.Action!=candidate.Action
                    || prior->second.DataAsset!=candidate.DataAsset || prior->second.GraphClass!=candidate.GraphClass
                    || prior->second.EntryFunction!=candidate.EntryFunction || prior->second.Bindings!=candidate.Bindings)
                    throw std::runtime_error("Registry authority selector is claimed by incompatible entry keys");
            }
        };
        remember(entry.value("spell",entry.value("package",std::string{})));
        remember(candidate.DataAsset);
        if(candidate.Action!="ConsumedItemAuthority")remember(candidate.GraphClass);
        if(entry.contains("metadata") && entry["metadata"].is_object()
            && entry["metadata"].contains("selectors") && entry["metadata"]["selectors"].is_array())
            for(const auto& selector:entry["metadata"]["selectors"])
                if(selector.is_string())remember(selector.get<std::string>());
    }
}

void RegistryBridge::LoadPresentationRoutes() {
    if(m_registrySnapshot.empty())throw std::runtime_error("Merged mod registry snapshot is unavailable");
    const auto parsed=nlohmann::json::parse(m_registrySnapshot);
    if(!parsed.is_object()||!parsed.contains("entries")||!parsed["entries"].is_array())
        throw std::runtime_error("Registry presentation manifest is unavailable");
    m_presentationRoutes.clear();
    m_reportedMissingPresentationRoutes.clear();
    for(const auto& entry:parsed["entries"]) {
        if(!entry.contains("consumer")||!entry["consumer"].is_object())continue;
        const auto& consumer=entry["consumer"];
        PresentationRoute candidate{entry.value("owner",std::string{}),consumer.value("connection",std::string{}),
            consumer.value("class",std::string{}),consumer.value("function",std::string("OnRuneSchemaPresentation")),{}};
        if(!Token(candidate.PluginId,96)||!Token(candidate.Connection,96)||candidate.ClassPath.empty()
            ||!Token(candidate.Function,96)||!consumer.contains("allowedKeys")||!consumer["allowedKeys"].is_array())
            throw std::runtime_error("Registry presentation consumer is malformed");
        for(const auto& key:consumer["allowedKeys"]) {
            if(!key.is_string()||!Token(key.get<std::string>(),96))
                throw std::runtime_error("Registry presentation allowlist is malformed");
            candidate.AllowedKeys.insert(key.get<std::string>());
        }
        if(candidate.AllowedKeys.empty())throw std::runtime_error("Registry presentation allowlist is empty");
        const auto routeKey=PresentationRouteKey(candidate.PluginId,candidate.Connection);
        auto [route,inserted]=m_presentationRoutes.emplace(routeKey,candidate);
        if(!inserted) {
            if(route->second.ClassPath!=candidate.ClassPath||route->second.Function!=candidate.Function)
                throw std::runtime_error("Registry presentation route has conflicting consumers");
            route->second.AllowedKeys.insert(candidate.AllowedKeys.begin(),candidate.AllowedKeys.end());
        }
        if(m_presentationRoutes.size()>MaxPresentationRoutes)
            throw std::runtime_error("Registry presentation route capacity exceeded");
    }
}

void RegistryBridge::ObserveSelectionNotify(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function || !parameters || m_authoritySelectionPaths.empty())return;
    if(function->GetFName()!=Names().Damage||!BridgeClassMatches<BridgeKind::Damage>(source))return;

    UObject* mesh=nullptr;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default)) {
        if(!field->HasAnyPropertyFlags(CPF_Parm) || field->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm))continue;
        auto* objectField=CastField<FObjectPropertyBase>(field);
        if(!objectField)continue;
        auto* value=objectField->GetObjectPropertyValue(objectField->ContainerPtrToValuePtr<void>(parameters));
        if(value && value->IsA(DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.SkeletalMeshComponent")))) {mesh=value;break;}
    }
    if(!mesh)return;
    DragonWilds::ActorHelper::FunctionCall getOwner(mesh,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();
    auto* caster=getOwner.Result<UObject*>();
    auto* actorClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    if(!caster || !actorClass || !caster->IsA(actorClass))return;
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(authority.Result<bool>())return;

    std::vector<UObject*> subjects{caster};
    auto* componentClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
    if(componentClass) {
        const auto components=static_cast<AActor*>(caster)->GetComponentsByClass(componentClass);
        for(auto* component:components)if(component)subjects.push_back(component);
    }
    std::string selectedKey;
    for(auto* subject:subjects) {
        for(auto* field:TFieldRange<FProperty>(subject->GetClassPrivate(),EFieldIterationFlags::IncludeSuper)) {
            auto* objectField=CastField<FObjectPropertyBase>(field);
            if(!objectField || objectField->GetArrayDim()!=1)continue;
            auto* value=objectField->GetObjectPropertyValue(objectField->ContainerPtrToValuePtr<void>(subject));
            if(!value)continue;
            auto found=m_authoritySelectionPaths.find(AssetPackage(RC::to_string(value->GetPathName())));
            if(found==m_authoritySelectionPaths.end())continue;
            if(!selectedKey.empty() && selectedKey!=found->second)
                throw std::runtime_error("multiple authoritative registry actions are selected on the local player");
            selectedKey=found->second;
        }
    }
    if(selectedKey.empty())return;
    const auto contract=ResolveContract();
    const auto components=static_cast<AActor*>(caster)->GetComponentsByClass(contract.Type);
    if(!components.Num())throw std::runtime_error("Owned player registry bridge has not replicated yet");
    ForwardRegistryRequest(components[0],selectedKey);
}

void RegistryBridge::ObserveConsumedItemAuthority(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!parameters||!MatchAuthorityGraph(source,function->GetFName()))return;
    const auto selected=m_authoritySelectionPaths.find(AssetPackage(RC::to_string(source->GetPathName())));
    if(selected==m_authoritySelectionPaths.end())return;
    const auto actionAt=m_authorityActions.find(selected->second);
    if(actionAt==m_authorityActions.end()||actionAt->second.Action!="ConsumedItemAuthority")return;
    auto& action=actionAt->second;
    if(function->GetFName()!=action.EntryName||!ExactClass(source,action.GraphClass,action.GraphType))return;
    auto* controllerField=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("PlayerController"),FNAME_Find)));
    if(!controllerField||controllerField->GetArrayDim()!=1||controllerField->GetOffset_Internal()<0
        ||static_cast<size_t>(controllerField->GetOffset_Internal())+sizeof(UObject*)>function->GetParmsSize())
        throw std::runtime_error("Consumed-item callback controller contract changed");
    auto* controller=controllerField->GetObjectPropertyValue(controllerField->ContainerPtrToValuePtr<void>(parameters));
    auto* caster=ControllerPawn(controller);
    if(!caster||!caster->IsA<AActor>())throw std::runtime_error("Consumed-item callback has no owned player pawn");
    if(ActorAuthority(caster)) {
        Trace("consumable.authority",caster,action.Key,"consume",0,{},"local_action_observed","executed","native_authority_bypass");
        return;
    }
    if(!LocalController(controller))throw std::runtime_error("Consumed-item callback did not originate from the owning client");
    const auto revision=++m_outboundRevision;const auto correlation=DiagnosticCorrelation(caster,revision);
    Trace("consumable.authority",caster,action.Key,"consume",revision,correlation,"local_action_observed","accepted","accepted");
    const auto duplicate=std::find_if(m_pendingClientActions.begin(),m_pendingClientActions.end(),
        [&](const PendingClientAction& pending){return pending.Caster.Get()==caster&&pending.Key==action.Key;});
    if(duplicate==m_pendingClientActions.end()) {
        m_pendingClientActions.push_back({PS::WeakObjectHandle(caster),action.Key,0.5f,5.0f,DiagnosticOwner(caster),correlation,revision,0,std::chrono::steady_clock::now()});
        Trace("consumable.authority",caster,action.Key,"consume",revision,correlation,"request_queued","pending","bridge_not_ready");
    } else Trace("consumable.authority",caster,action.Key,"consume",duplicate->Revision,duplicate->Correlation,"local_action_deduplicated","accepted","duplicate_observation");
}

void RegistryBridge::ObserveInventoryRemoval(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!parameters||function->GetFName()!=Names().Removal
        ||function->GetPathName()!=TEXT("/Script/Dominion.InventoryComponent:RemoveItemByData"))return;
    auto* itemField=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("ItemData"),FNAME_Find)));
    auto* countField=CastField<FIntProperty>(function->FindProperty(FName(TEXT("Count"),FNAME_Find)));
    auto* resultField=CastField<FBoolProperty>(function->GetReturnProperty());
    DragonWilds::ReflectedContract::Frame(function);
    if(!DragonWilds::ReflectedContract::Input(itemField,function)||itemField->GetElementSize()!=sizeof(UObject*)
        ||!DragonWilds::ReflectedContract::Input(countField,function)||countField->GetElementSize()!=sizeof(int32_t)
        ||!DragonWilds::ReflectedContract::Output(resultField,function)||resultField->GetElementSize()!=sizeof(bool)
        ||!resultField->IsNativeBool())throw std::runtime_error("Consumed-item inventory-removal contract changed");
    if(!resultField->GetPropertyValue(resultField->ContainerPtrToValuePtr<void>(parameters)))return;
    const auto count=countField->GetPropertyValue(countField->ContainerPtrToValuePtr<void>(parameters));
    auto* item=itemField->GetObjectPropertyValue(itemField->ContainerPtrToValuePtr<void>(parameters));
    if(count!=1||!item)return;
    const auto selected=m_authoritySelectionPaths.find(AssetPackage(RC::to_string(item->GetPathName())));
    if(selected==m_authoritySelectionPaths.end())return;
    const auto actionAt=m_authorityActions.find(selected->second);
    if(actionAt==m_authorityActions.end()||actionAt->second.Action!="ConsumedItemAuthority"
        ||AssetPackage(actionAt->second.DataAsset)!=AssetPackage(RC::to_string(item->GetPathName())))return;
    auto* controller=source->GetOuterPrivate();auto* caster=ControllerPawn(controller);
    if(!caster||!ActorAuthority(caster))throw std::runtime_error("Consumed-item removal was not owned by an authoritative player");
    const auto permitKey=RC::to_string(caster->GetPathName())+"|"+actionAt->second.Key;
    auto& permit=m_consumptionPermits[permitKey];permit.Count=std::min<uint32_t>(permit.Count+1,4);
    permit.Expires=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    permit.Owner=DiagnosticOwner(caster);
    Trace("consumable.authority",caster,actionAt->second.Key,"consume",permit.Revision,permit.Correlation,
        "authoritative_inventory_removal_observed","accepted","inventory_confirmed");
    Trace("consumable.authority",caster,actionAt->second.Key,"consume",permit.Revision,permit.Correlation,
        "consumption_permit_created","accepted","accepted");
}

bool RegistryBridge::ConsumeAuthorityPermit(UObject* caster,const AuthorityAction& action) {
    if(!caster||action.Action!="ConsumedItemAuthority")return false;
    const auto key=RC::to_string(caster->GetPathName())+"|"+action.Key;
    const auto found=m_consumptionPermits.find(key);const auto now=std::chrono::steady_clock::now();
    if(found==m_consumptionPermits.end())return false;
    if(found->second.Expires<now||found->second.Count==0){m_consumptionPermits.erase(found);return false;}
    Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
        "permit_matched","accepted","accepted");
    if(--found->second.Count==0)m_consumptionPermits.erase(found);
    return true;
}

void RegistryBridge::InvokeConsumedItemAuthority(UObject* caster,const AuthorityAction& action) {
    auto* graphType=DragonWilds::ActorHelper::ResolveClass(PS::ToWideSafe(action.GraphClass.c_str()));
    auto* item=DragonWilds::ActorHelper::ResolveObject(PS::ToWideSafe(action.DataAsset.c_str()));
    if(!graphType||!item||!item->IsA(graphType))throw std::runtime_error("Consumed ItemData does not match its registered cooked callback class");
    DragonWilds::ActorHelper::FunctionCall getController(caster,TEXT("/Script/Engine.Pawn:GetController"));
    getController.Invoke();auto* controller=getController.Result<UObject*>();
    if(!controller||controller->GetWorld()!=caster->GetWorld())throw std::runtime_error("Consumed-item authority controller is unavailable");
    const auto entry=PS::ToWideSafe(action.EntryFunction.c_str());
    auto* callback=item->GetFunctionByNameInChain(entry.c_str());
    auto* controllerField=callback?CastField<FObjectPropertyBase>(callback->FindProperty(FName(TEXT("PlayerController"),FNAME_Find))):nullptr;
    auto* tagsField=callback?CastField<FStructProperty>(callback->FindProperty(FName(TEXT("ItemGameplayTags"),FNAME_Find))):nullptr;
    auto* tagsType=tagsField?tagsField->GetStruct().Get():nullptr;
    DragonWilds::ReflectedContract::Frame(callback);
    if(DragonWilds::ReflectedContract::Count(callback)!=2 || !DragonWilds::ReflectedContract::Input(controllerField,callback)
        ||controllerField->GetElementSize()!=sizeof(UObject*)||!controllerField->GetPropertyClass().Get()||!controller->IsA(controllerField->GetPropertyClass().Get())
        ||!DragonWilds::ReflectedContract::Input(tagsField,callback)||!tagsType||tagsField->GetElementSize()!=tagsType->GetStructureSize()
        ||tagsType->GetPathName()!=TEXT("/Script/GameplayTags.GameplayTagContainer"))
        throw std::runtime_error("Consumed-item success function layout changed");
    DragonWilds::FManagedStruct tags(tagsType);struct EmptyTags{uint8_t Bytes[32];} empty{};
    std::memcpy(&empty,tags.GetData(),sizeof(empty));
    DragonWilds::ActorHelper::FunctionCall invoke(item,callback);
    Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
        "cooked_callback_invoked","accepted","accepted");
    try {
        invoke.Arg(TEXT("PlayerController"),controller).Arg(TEXT("ItemGameplayTags"),empty).Invoke();
        Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
            "callback_completed","executed","executed");
    } catch(...) {
        Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
            "callback_threw","rejected","callback_failed",0,0,true);throw;
    }
}

void RegistryBridge::TickAuthorityActions(float deltaSeconds) {
    const auto elapsed=std::max(0.0f,deltaSeconds);const auto now=std::chrono::steady_clock::now();
    for(auto at=m_consumptionPermits.begin();at!=m_consumptionPermits.end();) {
        if(at->second.Expires<now||at->second.Count==0) {
            Trace("consumable.authority",nullptr,at->first.substr(at->first.find_last_of('|')+1),"consume",at->second.Revision,
                at->second.Correlation,"consumption_permit_expired","expired","inventory_not_confirmed",0,5000,true);
            at=m_consumptionPermits.erase(at);
        } else ++at;
    }
    for(auto at=m_pendingClientActions.begin();at!=m_pendingClientActions.end();) {
        at->Delay-=elapsed;at->Remaining-=elapsed;auto* caster=at->Caster.Get();bool sent=false;
        const auto elapsedMs=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-at->Started).count());
        if(!caster||at->Remaining<=0.0f){Trace("consumable.authority",caster,at->Key,"consume",at->Revision,at->Correlation,
            "request_expired","expired",caster?"bridge_not_ready":"player_disconnected",at->Retries,elapsedMs,true);at=m_pendingClientActions.erase(at);continue;}
        if(at->Delay<=0.0f)try {
            at->Delay=0.5f; // Failed attachment retries must not run every frame.
            ++at->Retries;Trace("consumable.authority",caster,at->Key,"consume",at->Revision,at->Correlation,
                "retry_attempted","pending","bridge_not_ready",at->Retries,elapsedMs);
            const auto contract=ResolveContract();const auto components=static_cast<AActor*>(caster)->GetComponentsByClass(contract.Type);
            if(components.Num()){ForwardRegistryRequest(components[0],at->Key,at->Revision);sent=true;}
        }catch(const std::exception& error){RecordDiagnostic("consumable.authority",at->Key,at->Revision,"deferred",error.what());}
        catch(...){Trace("consumable.authority",caster,at->Key,"consume",at->Revision,at->Correlation,
            "retry_attempted","deferred","internal_error",at->Retries,elapsedMs,true);}
        if(sent)at=m_pendingClientActions.erase(at);else ++at;
    }
    for(auto at=m_pendingAuthorityActions.begin();at!=m_pendingAuthorityActions.end();) {
        at->Remaining-=elapsed;auto* caster=at->Caster.Get();
        if(!caster||at->Remaining<=0.0f) {
            const auto elapsedMs=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-at->Started).count());
            Trace("consumable.authority",caster,at->Key,"consume",at->Revision,at->Correlation,
                "authority_request_expired","expired",caster?"inventory_not_confirmed":"player_disconnected",0,elapsedMs,true);
            at=m_pendingAuthorityActions.erase(at);continue;
        }
        const auto action=m_authorityActions.find(at->Key);
        if(action==m_authorityActions.end()){at=m_pendingAuthorityActions.erase(at);continue;}
        const auto permitKey=RC::to_string(caster->GetPathName())+"|"+at->Key;
        const auto permit=m_consumptionPermits.find(permitKey);
        if(permit==m_consumptionPermits.end()||permit->second.Expires<now||permit->second.Count==0){++at;continue;}
        const auto selected=action->second;const auto revision=at->Revision;const auto correlation=at->Correlation;
        at=m_pendingAuthorityActions.erase(at);
        m_activeRequestRevision=revision;m_activeRequestCorrelation=correlation;
        try{m_lastAuthorityDeferred=false;InvokeAuthorityActionForCaster(caster,selected);}
        catch(const std::exception& error){RecordDiagnostic("consumable.authority",selected.Key,m_activeRequestRevision,"rejected",error.what());}
        m_activeRequestRevision=0;m_activeRequestCorrelation.clear();
    }
}

UObject* RegistryBridge::EnsureBridgeComponent(AActor* actor,bool publishRegistry) {
    if(!actor)return nullptr;
    const auto contract=ResolveContract();
    auto components=actor->GetComponentsByClass(contract.Type);
    UObject* component=components.Num()?components[0]:nullptr;bool created=false;
    if(!component) {
        const FTransform transform{};bool manual=true,deferred=true;
        DragonWilds::ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
        add.Arg(TEXT("Class"),contract.Type).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();
        component=add.Result<UObject*>();created=true;
    }
    if(!component)return nullptr;
    if(!contract.Compact)WriteInt(component,contract.ProtocolVersion,Protocol);
    if(created) {
        const FTransform transform{};bool manual=true;
        DragonWilds::ActorHelper::FunctionCall finish(actor,TEXT("/Script/Engine.Actor:FinishAddComponent"));
        finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform).Invoke();
    }
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled")).Arg(TEXT("bEnabled"),false).Invoke();
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated")).Arg(TEXT("ShouldReplicate"),true).Invoke();
    return component;
}

UObject* RegistryBridge::EnsureWorldComponent(AActor* actor) {
    if(!actor)return nullptr;const auto contract=ResolveWorldContract();
    auto components=actor->GetComponentsByClass(contract.Type);UObject* component=components.Num()?components[0]:nullptr;bool created=false;
    if(!component) {
        const FTransform transform{};bool manual=true,deferred=true;DragonWilds::ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
        add.Arg(TEXT("Class"),contract.Type).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();component=add.Result<UObject*>();created=true;
    }
    if(!component)return nullptr;WriteInt(component,contract.ProtocolVersion,Protocol);
    if(created){const FTransform transform{};bool manual=true;DragonWilds::ActorHelper::FunctionCall finish(actor,TEXT("/Script/Engine.Actor:FinishAddComponent"));finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual).Arg(TEXT("RelativeTransform"),transform).Invoke();}
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled")).Arg(TEXT("bEnabled"),false).Invoke();
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated")).Arg(TEXT("ShouldReplicate"),true).Invoke();return component;
}

UObject* RegistryBridge::EnsurePresentationComponent(AActor* actor,bool replicated) {
    if(!actor)return nullptr;
    auto* type=ResolvePresentationClass();
    auto components=actor->GetComponentsByClass(type);UObject* component=components.Num()?components[0]:nullptr;bool created=false;
    if(!component) {
        const FTransform transform{};bool manual=true,deferred=true;
        DragonWilds::ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
        add.Arg(TEXT("Class"),type).Arg(TEXT("bManualAttachment"),manual)
            .Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();
        component=add.Result<UObject*>();created=true;
    }
    if(!component)return nullptr;
    if(created) {
        const FTransform transform{};bool manual=true;
        DragonWilds::ActorHelper::FunctionCall finish(actor,TEXT("/Script/Engine.Actor:FinishAddComponent"));
        finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual)
            .Arg(TEXT("RelativeTransform"),transform).Invoke();
    }
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled"))
        .Arg(TEXT("bEnabled"),false).Invoke();
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
        .Arg(TEXT("ShouldReplicate"),replicated).Invoke();
    return component;
}

void RegistryBridge::EnsurePlayerBridges() {
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType)return;
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);
    for(auto* player:players)if(player && player->IsA<AActor>() && !player->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
        DragonWilds::ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(authority.Result<bool>())EnsureBridgeComponent(static_cast<AActor*>(player),false);
    }
}

void RegistryBridge::ObservePlayerLifecycle(UObject* source,UFunction* function) {
    if(!source||!function||function->GetFName()!=Names().BeginPlay||!source->IsA<AActor>())return;
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType||!source->IsA(playerType)
        || source->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))return;
    DragonWilds::ActorHelper::FunctionCall authority(source,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(authority.Result<bool>())EnsureBridgeComponent(static_cast<AActor*>(source),false);
}

int64_t RegistryBridge::ForwardRegistryRequest(UObject* component,const std::string& key,int64_t revision) {
    if(!component||!Token(key,512))throw std::runtime_error("Registry bridge request key is invalid");
    if(revision<=0)revision=++m_outboundRevision;
    DragonWilds::ActorHelper::FunctionCall owner(component,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
    auto* caster=owner.Result<UObject*>();const auto instance=DiagnosticInstance(caster);const auto correlation=DiagnosticCorrelation(caster,revision);
    if(auto* rpc=component->GetFunctionByNameInChain(TEXT("ServerRequestRuneSchemaAction"));rpc&&rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer)) {
        DragonWilds::ActorHelper::FunctionCall request(component,rpc);
        request.Arg(TEXT("Channel"),std::string("registry.action")).Arg(TEXT("EntityId"),key)
            .Arg(TEXT("ActionKey"),std::string("execute")).Arg(TEXT("Payload"),nlohmann::json{{"diagnosticEpoch",m_diagnostics.WorldEpoch()},{"diagnosticBridgeInstance",instance}}.dump())
            .Arg(TEXT("Revision"),revision).Invoke();
        Trace("consumable.authority",caster,key,"consume",revision,correlation,"request_transmitted","accepted","accepted");
        return revision;
    }
    auto* legacy=component->GetFunctionByNameInChain(TEXT("ServerRequestRegistryAction"));
    if(!legacy||!legacy->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer))throw std::runtime_error("Cooked registry server RPC is unavailable");
    DragonWilds::ActorHelper::FunctionCall request(component,legacy);request.Arg(TEXT("ActionKey"),key).Invoke();
    Trace("consumable.authority",caster,key,"consume",revision,correlation,"request_transmitted","accepted","legacy_transport");
    return revision;
}

void RegistryBridge::SendReceipt(UObject* component,const std::string& channel,const std::string& entity,int64_t revision,bool success,const std::string& detail) {
    if(!component)return;auto* rpc=component->GetFunctionByNameInChain(TEXT("ClientRuneSchemaReceipt"));
    if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetClient))return;
    DragonWilds::ActorHelper::FunctionCall call(component,rpc);call.Arg(TEXT("Channel"),channel).Arg(TEXT("EntityId"),entity)
        .Arg(TEXT("Revision"),revision).Arg(TEXT("Success"),success).Arg(TEXT("Detail"),detail.substr(0,512)).Invoke();
}

void RegistryBridge::SendNotification(UObject* component,const std::string& channel,const std::string& entity,int64_t revision,const std::string& payload) {
    if(!component||payload.size()>MaxActionPayloadBytes)return;auto* rpc=component->GetFunctionByNameInChain(TEXT("ClientRuneSchemaNotification"));
    if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetClient))return;
    DragonWilds::ActorHelper::FunctionCall call(component,rpc);call.Arg(TEXT("Channel"),channel).Arg(TEXT("EntityId"),entity)
        .Arg(TEXT("Payload"),payload).Arg(TEXT("Revision"),revision).Invoke();
}

bool RegistryBridge::PublishPresentation(const std::string& pluginId,const std::string& connection,
    const std::string& entityId,const std::string& payload,int64_t revision) {
    if(!m_presentationAuthorityComponent||!Token(pluginId,96)||!Token(connection,96)
        ||!Token(entityId,512)||payload.empty()||payload.size()>MaxActionPayloadBytes||revision<=0)return false;
    const auto route=m_presentationRoutes.find(PresentationRouteKey(pluginId,connection));
    if(route==m_presentationRoutes.end())return false;
    const auto body=nlohmann::json::parse(payload);size_t fields=0;
    if(!body.is_object()||!body.contains("key")||!body["key"].is_string()
        ||!route->second.AllowedKeys.contains(body["key"].get<std::string>())
        ||!ValidatePresentationValue(body,0,fields))return false;
    auto* rpc=m_presentationAuthorityComponent->GetFunctionByNameInChain(TEXT("MulticastRuneSchemaPluginPresentation"));
    if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetMulticast))return false;
    DragonWilds::ActorHelper::FunctionCall call(m_presentationAuthorityComponent,rpc);
    call.Arg(TEXT("PluginId"),pluginId).Arg(TEXT("Connection"),connection).Arg(TEXT("EntityId"),entityId)
        .Arg(TEXT("Payload"),payload).Arg(TEXT("Revision"),revision).Invoke();
    return true;
}

void RegistryBridge::ObservePresentationPre(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!parameters)return;
    const auto name=function->GetFName();
    if(name!=Names().Acknowledge&&name!=Names().Resync&&name!=Names().Presentation)return;
    if(name==Names().Acknowledge&&BridgeClassMatches<BridgeKind::Player>(source)) {
        const auto channel=ParameterString(function,parameters,TEXT("Channel"),193);
        const auto entity=ParameterString(function,parameters,TEXT("EntityId"),512);
        const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
        const auto success=ParameterBool(function,parameters,TEXT("Success"));
        const auto detail=ParameterString(function,parameters,TEXT("Detail"),512);
        if(!Token(channel,193)||!Token(entity,512)||revision<=0)throw std::runtime_error("Presentation acknowledgement is invalid");
        DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        auto* actor=owner.Result<UObject*>();if(!actor||!actor->IsA<AActor>())throw std::runtime_error("Presentation acknowledgement has no player owner");
        DragonWilds::ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(!authority.Result<bool>())return;
        RecordDiagnostic(channel,entity,revision,success?"acknowledged":"client-failed",detail);return;
    }
    if(name==Names().Resync&&BridgeClassMatches<BridgeKind::Player>(source)) {
        const auto channel=ParameterString(function,parameters,TEXT("Channel"),193);
        const auto entity=ParameterString(function,parameters,TEXT("EntityId"),512);
        const auto known=ParameterInt64(function,parameters,TEXT("KnownRevision"));
        if(!Token(channel,193)||!Token(entity,512)||known<0)throw std::runtime_error("Presentation resync request is invalid");
        for(const auto& [_,encoded]:m_worldInstances) {
            const auto record=nlohmann::json::parse(encoded);
            if(record.value("kind",std::string{})!="RuneSchemaPresentationState")continue;
            const auto routeChannel=record.value("pluginId",std::string{})+"."+record.value("connection",std::string{});
            if(routeChannel!=channel||(entity!="all"&&record.value("entityId",std::string{})!=entity)
                ||record.value("revision",int64_t{})<=known)continue;
            SendNotification(source,"presentation.resync",record.value("entityId",std::string{}),
                record.value("revision",int64_t{}),record.dump());
        }
        return;
    }
    if(name!=Names().Presentation||!BridgeClassMatches<BridgeKind::Presentation>(source))return;
    DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
    auto* actor=owner.Result<UObject*>();if(!actor||!actor->IsA<AActor>())throw std::runtime_error("Presentation bridge has no GameState owner");
    DragonWilds::ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return;
    const auto pluginId=ParameterString(function,parameters,TEXT("PluginId"),96);
    const auto connection=ParameterString(function,parameters,TEXT("Connection"),96);
    const auto entity=ParameterString(function,parameters,TEXT("EntityId"),512);
    const auto payloadText=ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes);
    const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
    if(!Token(pluginId,96)||!Token(connection,96)||!Token(entity,512)||revision<=0)
        throw std::runtime_error("Presentation envelope is invalid");
    const auto routeKey=PresentationRouteKey(pluginId,connection);const auto route=m_presentationRoutes.find(routeKey);
    if(route==m_presentationRoutes.end())throw std::runtime_error("Presentation route is not registered");
    const auto payload=nlohmann::json::parse(payloadText);size_t fields=0;
    if(!payload.is_object()||!payload.contains("key")||!payload["key"].is_string()
        ||!route->second.AllowedKeys.contains(payload["key"].get<std::string>())
        ||!ValidatePresentationValue(payload,0,fields))
        throw std::runtime_error("Presentation payload is not allow-listed or contains an asset reference");
    auto& seen=m_presentationAuthorityRevisions[routeKey+"|"+entity];
    if(revision<=seen)throw std::runtime_error("Presentation revision is stale");seen=revision;
    const auto state=payload.value("state",std::string{});const bool loop=payload.value("loop",false);
    const auto instance="presentation:"+pluginId+":"+connection+":"+entity;
    if(state=="stop")RemoveWorldInstance(instance,"presentation stopped");
    else if(state=="start"&&loop) {
        const auto started=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        UpsertWorldInstance(instance,nlohmann::json{{"kind","RuneSchemaPresentationState"},{"lifecycle","active"},
            {"pluginId",pluginId},{"connection",connection},{"entityId",entity},{"payload",payload},
            {"revision",revision},{"startedAtUnixMs",started}}.dump());
    }
    RecordDiagnostic(pluginId+"."+connection,entity,revision,"published",payload["key"].get<std::string>());
}

void RegistryBridge::HandleGenericRequest(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!BridgeClassMatches<BridgeKind::Player>(source)||!parameters)return;
    const auto channel=ParameterString(function,parameters,TEXT("Channel"),64);
    const auto entity=ParameterString(function,parameters,TEXT("EntityId"),512);
    const auto action=ParameterString(function,parameters,TEXT("ActionKey"),64);
    const auto payloadText=ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes);
    const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
    if(!Token(channel,64)||!Token(entity,512)||!Token(action,64)||revision<=0)throw std::runtime_error("Generic bridge envelope is invalid");
    const auto payload=nlohmann::json::parse(payloadText,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>64)throw std::runtime_error("Bridge payload nesting exceeds its limit");return true;});if(!payload.is_object()||payload.size()>32)throw std::runtime_error("Generic bridge payload must be a bounded object");
    DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();auto* caster=owner.Result<UObject*>();
    auto* actorClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));if(!caster||!actorClass||!caster->IsA(actorClass))throw std::runtime_error("Generic bridge has no owning actor");
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();if(!authority.Result<bool>())return;
    uint64_t diagnosticEpoch=0,diagnosticInstance=0;
    if(payload.contains("diagnosticEpoch")&&payload["diagnosticEpoch"].is_number_unsigned())diagnosticEpoch=payload["diagnosticEpoch"].get<uint64_t>();
    if(payload.contains("diagnosticBridgeInstance")&&payload["diagnosticBridgeInstance"].is_number_unsigned())diagnosticInstance=payload["diagnosticBridgeInstance"].get<uint64_t>();
    const auto correlation=diagnosticEpoch>0&&diagnosticInstance>0&&diagnosticInstance<=1000000
        ?BridgeDiagnostics::CorrelationFrom(diagnosticEpoch,diagnosticInstance,revision):DiagnosticCorrelation(caster,revision);
    Trace(channel,caster,entity,action,revision,correlation,"server_envelope_received","accepted","accepted");
    const auto ownerKey=DiagnosticOwner(caster)+"|"+channel;auto& window=m_requestWindows[ownerKey];const auto now=std::chrono::steady_clock::now();
    if(window.Started.time_since_epoch().count()==0||now-window.Started>=std::chrono::seconds(1)){window.Started=now;window.Count=0;}
    if(++window.Count>MaxRequestsPerSecond) {Trace(channel,caster,entity,action,revision,correlation,"rate_limit_rejected","rejected","rate_limited",0,0,true);SendReceipt(source,channel,entity,revision,false,"rate_limited");if(channel=="mod.data")return;throw std::runtime_error("Generic bridge request rate limit exceeded");}
    Trace(channel,caster,entity,action,revision,correlation,"rate_limit_accepted","accepted","accepted");
    if(channel!="mod.data"&&revision<=window.Revision) {Trace(channel,caster,entity,action,revision,correlation,"revision_stale","rejected","stale_revision",0,0,true);SendReceipt(source,channel,entity,revision,false,"stale_revision");throw std::runtime_error("Generic bridge request revision is stale");}
    window.Revision=revision;Trace(channel,caster,entity,action,revision,correlation,"revision_accepted","accepted","accepted");
    try {
        if(channel=="mod.data"){
            auto* notify=source->GetFunctionByNameInChain(TEXT("ClientRuneSchemaNotification"));
            if(!function->HasAnyFunctionFlags(FUNC_Net)||!function->HasAnyFunctionFlags(FUNC_NetServer)
                ||!notify||!notify->HasAnyFunctionFlags(FUNC_Net)||!notify->HasAnyFunctionFlags(FUNC_NetClient))throw std::runtime_error("Native mod-data RPC contract is unavailable");
            const PS::WeakObjectHandle identity(source);if(identity.Get()!=source)throw std::runtime_error("Relay player component identity is unavailable");
            const auto peer="peer-"+std::to_string(identity.ObjectIndex)+"-"+std::to_string(identity.ObjectSerialNumber);
            if(action=="subscribe"||action=="resync")m_modDataRelay.Subscribe(peer,entity,payload.at("version").get<uint32_t>(),action=="resync");
            else if(action=="chunk"){
                if(payload.at("channel")!=entity)throw std::runtime_error("Relay entity/channel mismatch");
                DragonWilds::ActorHelper::FunctionCall local(caster,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();
                const auto complete=m_modDataRelay.Receive(peer,payload,local.Result<bool>(),PS::ModDataRelay::Clock::now());
                if(complete&&ModDataReceived)try{ModDataReceived(*complete);}catch(...){PS::Log<LogLevel::Verbose>(STR("[MOD-DATA][CONSUMER-FAILED] Server consumer rejected completed data; transport receipt remains independent.\n"));}
            }else throw std::runtime_error("Unsupported mod-data relay action");
            m_relayPeers[peer].Assign(source);
            SendReceipt(source,channel,entity,revision,true,"accepted");return;
        }
        if(channel=="system.compat") {
            if(action!="hello")throw std::runtime_error("Unsupported compatibility action verb");
            const bool protocolMatch=payload.value("protocolVersion",0)==Protocol;
            const bool buildMatch=payload.value("build",std::string{})==BuildIdentity;
            const bool registryMatch=payload.value("registryFingerprint",std::string{})
                ==EffectiveRegistryFingerprint(m_registrySnapshot);
            const auto mappingState=MappingState(LocalMapping().Fingerprint,payload.value("mappingFingerprint",std::string{}));
            const auto clientRegistry=OwnerFingerprints(payload,"registryOwners");
            const auto clientNetwork=OwnerFingerprints(payload,"networkIdentityOwners");
            const auto registrySession=PS::RegistryManifestSummary::Negotiate(clientRegistry,
                PS::RegistryManifestSummary::Owners(m_registrySnapshot));
            const auto networkSession=PS::RegistryManifestSummary::Negotiate(clientNetwork,
                PS::NetworkIdentityState::OwnerSnapshot());
            const auto parity=PS::RegistryManifestSummary::Compatibility(registrySession,networkSession,
                payload.contains("registryOwners")&&payload.contains("networkIdentityOwners"),payload.contains("networkIdentityLanes"),
                OwnerFingerprints(payload,"networkIdentityLanes"),PS::NetworkIdentityState::Snapshot(),
                PS::NetworkIdentityState::MatchesAuthorityOrderProof(payload.value("nativeIdAuthority",std::string{})));
            const bool compatible=protocolMatch;
            window.RegistryRejected=!compatible;
            const auto detail=std::string("RuneSchema session ")+(compatible?"accepted":"rejected")+
                ": protocol="+(protocolMatch?"match":"different")+", shared-registries="+parity+
                "; registry("+registrySession.Summary()+"); network("+networkSession.Summary()+
                "); build="+(buildMatch?"match":"different")+", registry="+(registryMatch?"match":"different")+
                ", mapping-telemetry="+mappingState+". Content differences are warnings; individual operations remain authority-validated.";
            SendReceipt(source,channel,entity,revision,compatible,detail);return;
        }
        if(channel=="registry.action") {
            if(action!="execute")throw std::runtime_error("Unsupported registry action verb");
            const auto found=m_authorityActions.find(entity);if(found==m_authorityActions.end())throw std::runtime_error("Registry action key is not registered");
            m_activeRequestRevision=revision;m_activeRequestCorrelation=correlation;
            m_lastAuthorityDeferred=false;InvokeAuthorityActionForCaster(caster,found->second);
            m_activeRequestRevision=0;m_activeRequestCorrelation.clear();
            SendReceipt(source,channel,entity,revision,true,m_lastAuthorityDeferred
                ?"deferred":"executed");
            Trace(channel,caster,entity,action,revision,correlation,"receipt_sent","accepted",m_lastAuthorityDeferred?"deferred":"executed");return;
        }
        if(channel=="quest.control") {
            if(!QuestControl)throw std::runtime_error("Quest authority service is unavailable");
            if(action!="start"&&action!="repeat"&&action!="abandon"&&action!="reset")
                throw std::runtime_error("Unsupported quest control action verb");
            const auto detail=QuestControl(caster,entity,action,payloadText);
            SendNotification(source,"quest.state",entity,revision,nlohmann::json({{"kind","changed"},{"action",action}}).dump());
            SendReceipt(source,channel,entity,revision,true,detail.empty()?"Quest action completed by authority.":detail);return;
        }
        if(channel=="quest.state") {
            if(action!="resync"||entity!="all")throw std::runtime_error("Unsupported quest-state request");
            SendNotification(source,channel,entity,revision,nlohmann::json({{"kind","resync"}}).dump());
            SendReceipt(source,channel,entity,revision,true,"Quest presentation resync requested.");return;
        }
        if(channel=="world.state") {
            if(action!="resync"||entity!="all")throw std::runtime_error("Unsupported world-state request");
            SendNotification(source,channel,entity,revision,m_persistentStateEnvelope.empty()?nlohmann::json({{"kind","RuneSchemaWorldSnapshot"},{"revision",0},{"instances",nlohmann::json::array()}}).dump():m_persistentStateEnvelope);
            SendReceipt(source,channel,entity,revision,true,"World-state snapshot sent.");return;
        }
        if(AuthorityChannel) {
            const auto detail=AuthorityChannel(caster,channel,entity,action,payloadText);
            if(channel=="helpy.authority"&&action=="Players")SendNotification(source,"helpy.players","all",revision,detail);
            if(channel=="helpy.authority"&&action=="InventoryMarks")SendNotification(source,"helpy.marks",entity,revision,detail);
            SendReceipt(source,channel,entity,revision,true,detail.empty()?"Authority action completed.":detail);return;
        }
        throw std::runtime_error("Unsupported RuneSchema action channel");
    }catch(const std::exception& error){m_activeRequestRevision=0;m_activeRequestCorrelation.clear();RecordDiagnostic(channel,entity,revision,"rejected",error.what());SendReceipt(source,channel,entity,revision,false,channel=="mod.data"?std::string(error.what()).substr(0,512):"callback_failed");if(channel=="mod.data")return;throw;}
}

void RegistryBridge::ObserveClientTransport(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!parameters)return;const auto name=function->GetFName();
    if(name!=Names().Receipt&&name!=Names().Notification)return;
    if(!BridgeClassMatches<BridgeKind::Player>(source))return;
    const auto channel=ParameterString(function,parameters,TEXT("Channel"),64),entity=ParameterString(function,parameters,TEXT("EntityId"),512);
    if(name==Names().Receipt) {
        const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
        const auto success=ParameterBool(function,parameters,TEXT("Success"));
        const auto detail=ParameterString(function,parameters,TEXT("Detail"),512);
        DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        auto* caster=owner.Result<UObject*>();Trace(channel,caster,entity,"receipt",revision,DiagnosticCorrelation(caster,revision),
            channel=="system.compat"?(success?"compatibility_handshake_accepted":"compatibility_handshake_rejected"):"receipt_received",
            success?"executed":"rejected",channel=="system.compat"&&!success?"incompatible":detail);
        if(channel=="system.compat" && !success)
            PS::Log<LogLevel::Warning>(STR("{}\n"),PS::ToWideSafe(detail.c_str()));
    } else {
        const auto payload=ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes);(void)nlohmann::json::parse(payload);
        DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        if(channel=="presentation.resync") {
            const auto record=nlohmann::json::parse(payload);
            if(record.value("kind",std::string{})!="RuneSchemaPresentationState"||!record.contains("payload"))
                throw std::runtime_error("Presentation resync record is invalid");
            DeliverPresentation(source,record.value("pluginId",std::string{}),record.value("connection",std::string{}),
                record.value("entityId",std::string{}),record["payload"].dump(),record.value("revision",int64_t{}));
            return;
        }
        if(channel=="world.state"&&PersistentState)PersistentState(owner.Result<UObject*>(),channel,entity,payload);
        if(channel=="helpy.players")try {
            const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
            if(revision!=m_helpyRosterRevision||!revision||m_helpyRosterComponent.Get()!=source
                ||m_helpyRosterGeneration!=PS::SpawnToolRequests::Generation.load())return;
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-m_helpyRosterRequestedAt).count();
            m_helpyRosterRevision=0;m_helpyRosterComponent.Reset();
            if(elapsed<0||elapsed>4000)return;
            const auto roster=nlohmann::json::parse(payload);
            if(roster.value("Kind",std::string{})=="HelpyPlayers1"&&roster.contains("Players")&&roster["Players"].is_array()&&roster["Players"].size()<=256) {
                nlohmann::json rows=nlohmann::json::array();
                for(const auto& player:roster["Players"])if(player.is_object()) {
                    auto row=nlohmann::json{{"Name",player.value("Name",std::string("Player"))},{"Path",player.value("Guid",std::string{})},{"Guid",player.value("Guid",std::string{})}};
                    if(player.contains("PlayerId")&&player["PlayerId"].is_number_integer())row["PlayerId"]=player["PlayerId"];
                    if(player.contains("RoleStatus")&&PS::PlayerStatus::Valid(player["RoleStatus"]))row["RoleStatus"]=player["RoleStatus"];
                    if(player.value("NativePlayerColorVerified",false)&&player.contains("NativePlayerColor")&&player["NativePlayerColor"].is_array()&&player["NativePlayerColor"].size()==4){
                        bool valid=true;for(const auto& channel:player["NativePlayerColor"])if(!channel.is_number()||!std::isfinite(channel.get<double>())||channel.get<double>()<0||channel.get<double>()>1){valid=false;break;}
                        if(valid&&player["NativePlayerColor"][3].get<double>()>0){row["NativePlayerColor"]=player["NativePlayerColor"];row["NativePlayerColorVerified"]=true;}
                    }
                    if(player.contains("PlatformAccountName")&&player["PlatformAccountName"].is_string()){
                        const auto account=player["PlatformAccountName"].get<std::string>();
                        if(account.size()<=128&&std::none_of(account.begin(),account.end(),[](unsigned char c){return c<32||c==127;}))row["PlatformAccountName"]=account;
                    }
                    if(player.contains("Platform")&&player["Platform"].is_string()){
                        const auto platform=player["Platform"].get<std::string>();
                        if(platform=="Steam"||platform=="Epic"||platform=="Game Pass"||platform=="Xbox"||platform=="PS5"||platform=="Switch 2"||platform=="Jagex"||platform=="Unknown")row["Platform"]=platform;
                    }
                    if(player.contains("StatWire"))row["Stats"]=PS::PartyStatWire::Decode(player["StatWire"],std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(),elapsed);
                    rows.push_back(std::move(row));
                }
                auto roles=PS::PlayerStatus::Snapshot(rows,PS::SpawnToolRequests::Generation.load());
                PS::SpawnToolRequests::CatalogProgress({{"Players",std::move(rows)},{"PlayerRoles",std::move(roles)}});
            }
        }catch(...){}
        if(ClientNotification)ClientNotification(owner.Result<UObject*>(),channel,entity,payload);
    }
}

void RegistryBridge::AcknowledgePresentation(const PresentationRoute& route,const std::string& entityId,
    int64_t revision,bool success,const std::string& detail) {
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType)return;const auto contract=ResolveContract();TArray<UObject*> players;
    UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);
    for(auto* player:players)if(player&&player->IsA<AActor>())try {
        DragonWilds::ActorHelper::FunctionCall local(player,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();
        if(!local.Result<bool>())continue;
        const auto components=static_cast<AActor*>(player)->GetComponentsByClass(contract.Type);if(!components.Num())continue;
        auto* rpc=components[0]->GetFunctionByNameInChain(TEXT("ServerAcknowledgeRuneSchemaPresentation"));
        if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer))continue;
        DragonWilds::ActorHelper::FunctionCall call(components[0],rpc);
        call.Arg(TEXT("Channel"),route.PluginId+"."+route.Connection).Arg(TEXT("EntityId"),entityId)
            .Arg(TEXT("Revision"),revision).Arg(TEXT("Success"),success).Arg(TEXT("Detail"),detail.substr(0,512)).Invoke();
        return;
    }catch(...){}
}

void RegistryBridge::DeliverPresentation(UObject* context,const std::string& pluginId,const std::string& connection,
    const std::string& entityId,const std::string& payloadText,int64_t revision) {
    if(Storefront::IsDedicatedServer())return;
    if(!Token(pluginId,96)||!Token(connection,96)||!Token(entityId,512)||revision<=0
        ||payloadText.empty()||payloadText.size()>MaxActionPayloadBytes)
        throw std::runtime_error("Client presentation envelope is invalid");
    const auto routeAt=m_presentationRoutes.find(PresentationRouteKey(pluginId,connection));
    if(routeAt==m_presentationRoutes.end()) {
        std::string action="<unreadable>";
        try {
            const auto envelope=nlohmann::json::parse(payloadText);
            if(envelope.is_object()&&envelope.contains("key")&&envelope["key"].is_string()
                &&Token(envelope["key"].get<std::string>(),96))
                action=envelope["key"].get<std::string>();
        }catch(...) {}
        std::vector<std::string> connections,owners;
        for(const auto& [_,route]:m_presentationRoutes) {
            if(route.PluginId==pluginId)connections.push_back(route.Connection);
            if(route.Connection==connection)owners.push_back(route.PluginId);
        }
        const auto describe=[](std::vector<std::string> values) {
            std::sort(values.begin(),values.end());
            values.erase(std::unique(values.begin(),values.end()),values.end());
            if(values.empty())return std::string("<none>");
            std::string result;
            const auto count=std::min<size_t>(values.size(),5);
            for(size_t index=0;index<count;++index){if(!result.empty())result+=",";result+=values[index];}
            if(values.size()>count)result+=",...";
            return result;
        };
        const auto diagnosticKey=pluginId+"|"+connection+"|"+action;
        if(!m_reportedMissingPresentationRoutes.emplace(diagnosticKey).second)return;
        const auto detail=std::format(
            "Client presentation route is not installed: emitted mod='{}' connection='{}' entity='{}' action='{}'; local connections for mod={}; local owners for connection={}; only this optional RuneSchema presentation was dropped",
            pluginId,connection,entityId.substr(0,128),action,describe(std::move(connections)),describe(std::move(owners)));
        RecordDiagnostic(pluginId+"."+connection,entityId,revision,"rejected",detail);
        throw std::runtime_error(detail);
    }
    const auto& route=routeAt->second;const auto payload=nlohmann::json::parse(payloadText);size_t fields=0;
    if(!payload.is_object()||!payload.contains("key")||!payload["key"].is_string()
        ||!route.AllowedKeys.contains(payload["key"].get<std::string>())
        ||!ValidatePresentationValue(payload,0,fields))
        throw std::runtime_error("Client presentation payload is not allow-listed or contains an asset reference");
    const auto revisionKey=PresentationRouteKey(pluginId,connection)+"|"+entityId;
    auto& seen=m_presentationClientRevisions[revisionKey];if(revision<=seen)return;seen=revision;
    try {
        UObject* gameState=nullptr;auto* world=context?context->GetWorld():nullptr;
        auto* gameplay=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
            TEXT("/Script/Engine.Default__GameplayStatics"),false);
        if(world&&gameplay) {
            DragonWilds::ActorHelper::FunctionCall query(gameplay,TEXT("/Script/Engine.GameplayStatics:GetGameState"));
            query.Arg(TEXT("WorldContextObject"),static_cast<UObject*>(world)).Invoke();gameState=query.Result<UObject*>();
        }
        if(!gameState||!gameState->IsA<AActor>())throw std::runtime_error("Client GameState is unavailable");
        auto consumer=m_presentationConsumers.find(PresentationRouteKey(pluginId,connection));
        UObject* component=consumer==m_presentationConsumers.end()?nullptr:consumer->second;
        if(!component||component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))) {
            auto* type=DragonWilds::ActorHelper::ResolveClass(PS::ToWideSafe(route.ClassPath.c_str()));
            if(!type)type=UECustom::UKismetSystemLibrary::LoadClassAsset_Blocking(
                UECustom::TSoftClassPtr<UObject>(UECustom::FSoftObjectPath(PS::ToWideSafe(route.ClassPath.c_str()))));
            auto* base=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.ActorComponent"));
            if(!type||!base||!type->IsChildOf(base)||DragonWilds::ActorHelper::IsAbstract(type))
                throw std::runtime_error("Client presentation consumer class is unavailable or is not an ActorComponent");
            const FTransform transform{};bool manual=true,deferred=true;
            DragonWilds::ActorHelper::FunctionCall add(gameState,TEXT("/Script/Engine.Actor:AddComponentByClass"));
            add.Arg(TEXT("Class"),type).Arg(TEXT("bManualAttachment"),manual)
                .Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();
            component=add.Result<UObject*>();if(!component)throw std::runtime_error("Client presentation consumer creation returned null");
            DragonWilds::ActorHelper::FunctionCall finish(gameState,TEXT("/Script/Engine.Actor:FinishAddComponent"));
            finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual)
                .Arg(TEXT("RelativeTransform"),transform).Invoke();
            DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
                .Arg(TEXT("ShouldReplicate"),false).Invoke();
            m_presentationConsumers[PresentationRouteKey(pluginId,connection)]=component;
        }
        const auto functionName=PS::ToWideSafe(route.Function.c_str());
        auto* handler=component->GetFunctionByNameInChain(functionName.c_str());
        if(!handler)throw std::runtime_error("Client presentation consumer function is unavailable");
        DragonWilds::ActorHelper::FunctionCall invoke(component,handler);
        invoke.Arg(TEXT("PluginId"),pluginId).Arg(TEXT("Connection"),connection).Arg(TEXT("EntityId"),entityId)
            .Arg(TEXT("Payload"),payloadText).Arg(TEXT("Revision"),revision).Invoke();
        AcknowledgePresentation(route,entityId,revision,true,"Presentation delivered to local consumer.");
    }catch(const std::exception& error) {
        AcknowledgePresentation(route,entityId,revision,false,error.what());throw;
    }
}

void RegistryBridge::ObservePresentationTransport(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||!parameters)return;
    const auto name=function->GetFName();
    if(name!=Names().Presentation
        &&name!=Names().ClientPresentation)return;
    if(Storefront::IsDedicatedServer()||!BridgeClassMatches<BridgeKind::Presentation>(source))return;
    DeliverPresentation(source,ParameterString(function,parameters,TEXT("PluginId"),96),
        ParameterString(function,parameters,TEXT("Connection"),96),
        ParameterString(function,parameters,TEXT("EntityId"),512),
        ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes),
        ParameterInt64(function,parameters,TEXT("Revision")));
}

void RegistryBridge::ReplayPresentationSnapshot(UObject* context,const std::string& payloadText) {
    if(Storefront::IsDedicatedServer())return;
    const auto snapshot=nlohmann::json::parse(payloadText);
    if(!snapshot.is_object()||snapshot.value("kind",std::string{})!="RuneSchemaWorldSnapshot"
        ||!snapshot.contains("instances")||!snapshot["instances"].is_array())return;
    for(const auto& record:snapshot["instances"]) {
        if(!record.is_object()||record.value("kind",std::string{})!="RuneSchemaPresentationState"
            ||record.value("lifecycle",std::string("active"))!="active"||!record.contains("payload"))continue;
        DeliverPresentation(context,record.value("pluginId",std::string{}),record.value("connection",std::string{}),
            record.value("entityId",std::string{}),record["payload"].dump(),record.value("revision",int64_t{}));
    }
}

void RegistryBridge::SendCompatibilityAck(const std::string& remoteFingerprint) {
    if(remoteFingerprint.empty())return;auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));if(!playerType)return;
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);const auto contract=ResolveContract();
    for(auto* player:players)if(player&&player->IsA<AActor>())try {
        DragonWilds::ActorHelper::FunctionCall local(player,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();if(!local.Result<bool>())continue;
        const auto components=static_cast<AActor*>(player)->GetComponentsByClass(contract.Type);if(!components.Num())continue;auto* rpc=components[0]->GetFunctionByNameInChain(TEXT("ServerRequestRuneSchemaAction"));if(!rpc)continue;
        const nlohmann::json payload={{"protocolVersion",Protocol},{"build",BuildIdentity},{"registryFingerprint",EffectiveRegistryFingerprint(m_registrySnapshot)},
            {"authorityFingerprint",remoteFingerprint},{"mappingFingerprint",LocalMapping().Fingerprint},
            {"registryOwners",PS::RegistryManifestSummary::Owners(m_registrySnapshot)},
            {"networkIdentityOwners",PS::NetworkIdentityState::OwnerSnapshot()},
            {"nativeIdAuthority",PS::NetworkIdentityState::GetAuthorityOrderProof()},
            {"networkIdentityLanes",PS::NetworkIdentityState::Snapshot()},
            {"sessionPolicy",{{"mode","server-baseline-permissive"},{"clientExtras","allowed"},
                {"missingContent","allowed"},{"actionValidation","independent"},{"sharedRegistries","authority-first-warn"}}},
            {"channels",{"system.compat","registry.action","quest.control","quest.state","dialogue.session","event.state","spawn.state","npc.state","vendor.state","player.profile","nameplate.profile","world.state","notify.player"}}};
        const auto revision=++m_outboundRevision;const auto correlation=DiagnosticCorrelation(player,revision);
        Trace("system.compat",player,"client","hello",revision,correlation,"compatibility_handshake_started","pending","accepted");
        DragonWilds::ActorHelper::FunctionCall request(components[0],rpc);request.Arg(TEXT("Channel"),std::string("system.compat")).Arg(TEXT("EntityId"),std::string("client"))
            .Arg(TEXT("ActionKey"),std::string("hello")).Arg(TEXT("Payload"),payload.dump()).Arg(TEXT("Revision"),revision).Invoke();
        DragonWilds::ActorHelper::FunctionCall resync(components[0],rpc);resync.Arg(TEXT("Channel"),std::string("quest.state")).Arg(TEXT("EntityId"),std::string("all"))
            .Arg(TEXT("ActionKey"),std::string("resync")).Arg(TEXT("Payload"),std::string("{}")).Arg(TEXT("Revision"),++m_outboundRevision).Invoke();
        DragonWilds::ActorHelper::FunctionCall world(components[0],rpc);world.Arg(TEXT("Channel"),std::string("world.state")).Arg(TEXT("EntityId"),std::string("all"))
            .Arg(TEXT("ActionKey"),std::string("resync")).Arg(TEXT("Payload"),std::string("{}")).Arg(TEXT("Revision"),++m_outboundRevision).Invoke();return;
    }catch(...){}
}

bool RegistryBridge::RequestAuthority(const std::string& action,const std::string& payload) {
    if(action=="Players"&&m_helpyRosterRevision&&m_helpyRosterGeneration==PS::SpawnToolRequests::Generation.load()
        &&std::chrono::steady_clock::now()-m_helpyRosterRequestedAt<std::chrono::seconds(4))return true;
    if(!Token(action,64)||payload.empty()||payload.size()>MaxActionPayloadBytes)return false;
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType)return false;const auto contract=ResolveContract();
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);
    for(auto* player:players)if(player&&player->IsA<AActor>())try {
        DragonWilds::ActorHelper::FunctionCall local(player,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();
        if(!local.Result<bool>())continue;
        const auto components=static_cast<AActor*>(player)->GetComponentsByClass(contract.Type);if(!components.Num())continue;
        auto* rpc=components[0]->GetFunctionByNameInChain(TEXT("ServerRequestRuneSchemaAction"));
        if(!rpc||!rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer))continue;
        const auto revision=++m_outboundRevision;
        if(action=="Players") {m_helpyRosterComponent.Assign(components[0]);m_helpyRosterRevision=revision;m_helpyRosterGeneration=PS::SpawnToolRequests::Generation.load();m_helpyRosterRequestedAt=std::chrono::steady_clock::now();}
        DragonWilds::ActorHelper::FunctionCall request(components[0],rpc);
        request.Arg(TEXT("Channel"),std::string("helpy.authority"))
            .Arg(TEXT("EntityId"),std::string("request-")+std::to_string(revision))
            .Arg(TEXT("ActionKey"),action).Arg(TEXT("Payload"),payload)
            .Arg(TEXT("Revision"),revision).Invoke();return true;
    }catch(...){}
    return false;
}

bool RegistryBridge::Attach(AGameModeBase* mode) {
    if(!mode)return false;
    auto* modeObject=static_cast<UObject*>(mode);
    auto* gameState=DragonWilds::ActorHelper::GetObjectRef(modeObject,TEXT("GameState"));
    if(!gameState) {
        auto* world=modeObject->GetWorld();
        auto* gameplay=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
            TEXT("/Script/Engine.Default__GameplayStatics"),false);
        if(world && gameplay) {
            DragonWilds::ActorHelper::FunctionCall query(gameplay,TEXT("/Script/Engine.GameplayStatics:GetGameState"));
            query.Arg(TEXT("WorldContextObject"),static_cast<UObject*>(world)).Invoke();
            gameState=query.Result<UObject*>();
        }
    }
    auto* actorClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    if(!gameState || !actorClass || !gameState->IsA(actorClass))return false;
    auto* actor=static_cast<AActor*>(gameState);
    DragonWilds::ActorHelper::FunctionCall authority(actor,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return false;
    m_worldAuthorityComponent=EnsureWorldComponent(actor);
    if(!m_worldAuthorityComponent)throw std::runtime_error("GameState world bridge component creation returned null");
    m_presentationAuthorityComponent=EnsurePresentationComponent(actor,true);
    if(!m_presentationAuthorityComponent)throw std::runtime_error("GameState presentation bridge component creation returned null");

    const auto contract=ResolveContract();
    auto components=actor->GetComponentsByClass(contract.Type);
    UObject* component=components.Num()?components[0]:nullptr;
    bool created=false;
    if(!component) {
        const FTransform transform{};bool manual=true,deferred=true;
        DragonWilds::ActorHelper::FunctionCall add(actor,TEXT("/Script/Engine.Actor:AddComponentByClass"));
        add.Arg(TEXT("Class"),contract.Type).Arg(TEXT("bManualAttachment"),manual)
            .Arg(TEXT("RelativeTransform"),transform).Arg(TEXT("bDeferredFinish"),deferred).Invoke();
        component=add.Result<UObject*>();created=true;
    }
    if(!component)throw std::runtime_error("GameState registry bridge component creation returned null");

    if(m_registrySnapshot.empty())throw std::runtime_error("Merged mod registry snapshot is unavailable");
    const auto parsed=nlohmann::json::parse(m_registrySnapshot);
    if(!parsed.is_object() || parsed.value("kind",std::string{})!="RuneSchemaRegistryBridgeManifest"
        || parsed.value("schemaVersion",0)!=1 || parsed.value("protocolVersion",0)!=Protocol
        || !parsed.contains("entries") || !parsed["entries"].is_array())
        throw std::runtime_error("Registry bridge manifest requires kind, schemaVersion 1, protocolVersion 1 and an entries array");
    // Keep world attachment on the same manifest contract used at discovery.
    // A second, older validator here previously rejected ConsumedItemAuthority
    // even after SetRegistrySnapshot had accepted it.
    LoadAuthorityActions();

    m_manifestFingerprint=EffectiveRegistryFingerprint(m_registrySnapshot);
    if(contract.Compact) {
        m_registryRevision=std::max<uint32_t>(1,m_registryRevision+1);
        WriteString(component,contract.IdentityPayload,CompactPayload());
    } else {
        WriteInt(component,contract.ProtocolVersion,Protocol);
        WriteString(component,contract.RegistryFingerprint,m_manifestFingerprint);
        WriteInt(component,contract.RegistryRevision,std::max(1,ReadInt(component,contract.RegistryRevision)+1));
    }
    if(created) {
        const FTransform transform{};bool manual=true;
        DragonWilds::ActorHelper::FunctionCall finish(actor,TEXT("/Script/Engine.Actor:FinishAddComponent"));
        finish.Arg(TEXT("Component"),component).Arg(TEXT("bManualAttachment"),manual)
            .Arg(TEXT("RelativeTransform"),transform).Invoke();
    }
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetComponentTickEnabled"))
        .Arg(TEXT("bEnabled"),false).Invoke();
    DragonWilds::ActorHelper::FunctionCall(component,TEXT("/Script/Engine.ActorComponent:SetIsReplicated"))
        .Arg(TEXT("ShouldReplicate"),true).Invoke();
    m_authorityComponent=component;
    m_pendingMode.Reset();
    Trace("bridge.lifecycle",actor,"world","attach",0,{},"player_bridge_attachment_ready","accepted","accepted");
    // One bounded world-ready scan covers pawns whose BeginPlay preceded the
    // bridge. New pawns are handled by ObservePlayerLifecycle.
    EnsurePlayerBridges();
    return true;
}

bool RegistryBridge::AuthorityEntry(FName name) const {
    return std::find(m_authorityEntryNames.begin(),m_authorityEntryNames.end(),name)!=m_authorityEntryNames.end();
}

RegistryBridge::AuthorityAction* RegistryBridge::MatchAuthorityGraph(UObject* source,FName name) {
    if(!source||!AuthorityEntry(name))return nullptr;
    auto* type=source->GetClassPrivate();if(!type)return nullptr;
    auto found=m_authorityGraphRoutes.find(type);
    if(found==m_authorityGraphRoutes.end()||found->second.Type.Get()!=type){
        if(m_authorityGraphRoutes.size()>=1024)m_authorityGraphRoutes.clear();
        GraphRoutes routes;routes.Type.Assign(type);
        const auto path=RC::to_string(type->GetPathName());
        for(const auto& [key,action]:m_authorityActions)if(action.GraphClass==path)routes.Keys.push_back(key);
        found=m_authorityGraphRoutes.insert_or_assign(type,std::move(routes)).first;
    }
    for(const auto& key:found->second.Keys){auto action=m_authorityActions.find(key);
        if(action!=m_authorityActions.end()&&action->second.EntryName==name){
            return &action->second;}}
    return nullptr;
}

void RegistryBridge::ObserveAuthorityPre(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function)return;
    ObserveConsumedItemAuthority(source,function,parameters);
    ObserveSelectionNotify(source,function,parameters);
    const auto name=function->GetFName();
    if(name==Names().Request) {HandleGenericRequest(source,function,parameters);return;}
    if(name==Names().LegacyRequest
        && BridgeClassMatches<BridgeKind::Player>(source)) {
        FStrProperty* keyField=nullptr;size_t inputs=0;
        for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)
            && !field->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)){++inputs;keyField=CastField<FStrProperty>(field);}
        if(inputs!=1 || !keyField || !parameters)throw std::runtime_error("ServerRequestRegistryAction contract changed");
        const auto& value=keyField->GetPropertyValue(keyField->ContainerPtrToValuePtr<void>(parameters));
        const auto& chars=value.GetCharArray();
        if(chars.Num()<1 || !chars.GetData() || chars.GetData()[chars.Num()-1]!=0 || chars.Num()>513)
            throw std::runtime_error("Registry action key is invalid");
        const auto key=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
        const auto found=m_authorityActions.find(key);
        if(found==m_authorityActions.end())throw std::runtime_error("Registry action key is not registered");
        DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        auto* caster=owner.Result<UObject*>();
        InvokeAuthorityActionForCaster(caster,found->second);return;
    }
    // A graph started by the bridge will pass through this observer too. Mark
    // it as consumed and let the cooked graph execute once; never route it back
    // through InvokeAuthorityAction from the post-hook.
    if(m_activeAuthorityGraph && source==m_activeAuthorityGraph) {
        m_nativeAuthorityActionObserved=true;
        return;
    }
    const auto* action=MatchAuthorityGraph(source,name);if(!action)return;
    auto* casterField=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Caster")));
    auto* caster=casterField?casterField->GetObjectPropertyValue(casterField->ContainerPtrToValuePtr<void>(source)):nullptr;
    if(!caster || !caster->IsA<AActor>())return;
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    // On the host/server the original cooked graph is already authoritative.
    // The bridge only forwards client-originated casts to that same graph.
    if(authority.Result<bool>())return;
    const auto contract=ResolveContract();auto components=static_cast<AActor*>(caster)->GetComponentsByClass(contract.Type);
    if(!components.Num())throw std::runtime_error("Owned player registry bridge has not replicated yet");
    ForwardRegistryRequest(components[0],action->Key);
}

void RegistryBridge::InvokeAuthorityAction(UObject* graph,const AuthorityAction& action) {
    auto* casterField=CastField<FObjectPropertyBase>(
        DragonWilds::PropertyHelper::GetPropertyByName(graph->GetClassPrivate(),TEXT("Caster")));
    auto* caster=casterField?casterField->GetObjectPropertyValue(casterField->ContainerPtrToValuePtr<void>(graph)):nullptr;
    InvokeAuthorityActionForCaster(caster,action);
}

void RegistryBridge::InvokeAuthorityActionForCaster(UObject* caster,const AuthorityAction& action) {
    auto* actorClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    if(!caster || !actorClass || !caster->IsA(actorClass))throw std::runtime_error("registry authority request has no actor Caster");
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    if(!authority.Result<bool>())return;
    const auto compatibility=m_requestWindows.find(DiagnosticOwner(caster)+"|system.compat");
    if(compatibility!=m_requestWindows.end()&&compatibility->second.RegistryRejected)
        throw std::runtime_error("Registry action refused: client/server registry compatibility failed; align enabled mods and restart before retrying");
    if(action.Action=="ConsumedItemAuthority") {
        if(!ConsumeAuthorityPermit(caster,action)) {
            const auto duplicate=std::find_if(m_pendingAuthorityActions.begin(),m_pendingAuthorityActions.end(),
                [&](const PendingAuthorityAction& pending){return pending.Caster.Get()==caster&&pending.Key==action.Key;});
            if(duplicate==m_pendingAuthorityActions.end()) {
                m_pendingAuthorityActions.push_back({PS::WeakObjectHandle(caster),action.Key,5.0f,DiagnosticOwner(caster),
                    m_activeRequestCorrelation,m_activeRequestRevision,std::chrono::steady_clock::now()});
                Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
                    "authority_request_deferred","deferred","inventory_not_confirmed");
            }
            m_lastAuthorityDeferred=true;return;
        }
        InvokeConsumedItemAuthority(caster,action);PublishActionActivation(caster,action);
        Trace("consumable.authority",caster,action.Key,"consume",m_activeRequestRevision,m_activeRequestCorrelation,
            "activation_published","executed","executed");return;
    }
    if(action.Action!="SpawnFollower" && action.Action!="ExecuteGraph")throw std::runtime_error("authority action is unsupported");
    auto* graphType=DragonWilds::ActorHelper::ResolveClass(PS::ToWideSafe(action.GraphClass.c_str()));
    if(!graphType || DragonWilds::ActorHelper::IsAbstract(graphType))
        throw std::runtime_error("Authority Blueprint graph is unavailable");
    UObject* data=nullptr;UClass* expected=nullptr;
    if(action.Action=="SpawnFollower") {
        data=DragonWilds::ActorHelper::ResolveObject(PS::ToWideSafe(action.DataAsset.c_str()));
        expected=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.FollowerDataAsset"));
        if(!data || !expected || !data->IsA(expected))throw std::runtime_error("Follower authority data asset is unavailable");
    }

    // The reflected FollowerComponent::SpawnFollower signature is not verified
    // for this executable. Calling it synchronously froze two standalone runs.
    // Fail closed until a captured native contract proves parameter and lifecycle
    // semantics; the cooked authority graph remains the only permitted route.

    FStaticConstructObjectParameters graphParams(graphType,caster);
    graphParams.Name=FName(NAME_None);
    auto* graph=UObjectGlobals::StaticConstructObject<UObject*>(graphParams);
    auto* casterField=graph?CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(graphType,TEXT("Caster"))):nullptr;
    const auto entryFunction=PS::ToWideSafe(action.EntryFunction.c_str());
    auto* trigger=graph?graph->GetFunctionByNameInChain(entryFunction.c_str()):nullptr;
    if(!graph || !trigger || trigger->GetReturnProperty()
        || DragonWilds::ReflectedContract::Count(trigger)!=0)
        throw std::runtime_error("Authority Blueprint entry function is unavailable or requires parameters");
    if(casterField)WriteObject(graph,casterField,caster,"Caster");
    else if(action.Action=="SpawnFollower")throw std::runtime_error("Follower authority Blueprint has no Caster field");
    for(const auto& [property,path]:action.Bindings) {
        auto* field=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(graphType,PS::ToWideSafe(property.c_str())));
        auto* value=DragonWilds::ActorHelper::ResolveObject(PS::ToWideSafe(path.c_str()));
        if(!field || !value || !value->IsA(field->GetPropertyClass()))
            throw std::runtime_error(std::format("Authority binding '{}' does not match its Blueprint property",property));
        WriteObject(graph,field,value,property);
    }
    m_activeAuthorityGraph=graph;m_nativeAuthorityActionObserved=true;
    try {DragonWilds::ActorHelper::FunctionCall(graph,trigger).Invoke();}
    catch(...){m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;throw;}
    m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;
    PublishActionActivation(caster,action);
}

void RegistryBridge::PublishActionActivation(UObject* caster,const AuthorityAction& action) {
    if(!caster || action.Key.empty())return;
    const nlohmann::json envelope={{"key",action.Key},{"caster",RC::to_string(caster->GetPathName())},
        {"action",action.Action},{"revision",++m_outboundRevision}};
    if(!PublishActivation(envelope.dump()))
        PS::Log<LogLevel::Warning>(STR("Registry action '{}' executed, but no replicated activation bridge was ready.\n"),
            PS::ToWideSafe(action.Key.c_str()));
}

void RegistryBridge::ObserveAuthorityPost(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function)return;
    ObserveInventoryRemoval(source,function,parameters);
    const auto* found=MatchAuthorityGraph(source,function->GetFName());if(!found)return;
    if(source!=m_activeAuthorityGraph) {
        auto* casterField=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Caster")));
        auto* caster=casterField?casterField->GetObjectPropertyValue(casterField->ContainerPtrToValuePtr<void>(source)):nullptr;
        if(!caster || !caster->IsA<AActor>())return;
        DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(authority.Result<bool>())PublishActionActivation(caster,*found);
        return;
    }
    auto* graph=m_activeAuthorityGraph;
    m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;
    (void)graph;
}

void RegistryBridge::RetryAttach(float deltaSeconds) {
    auto* pending=static_cast<AGameModeBase*>(m_pendingMode.Get());
    if(!pending || m_authorityComponent){if(!pending)m_pendingMode.Reset();return;}
    m_retryElapsed+=std::max(0.0f,deltaSeconds);
    m_retryInterval+=std::max(0.0f,deltaSeconds);
    if(m_retryInterval<0.25f)return;
    m_retryInterval=0.0f;
    try {
        if(Attach(pending))return;
        if(m_retryElapsed>=30.0f) {
            Trace("bridge.lifecycle",pending,"player","attach",0,{},"player_bridge_attachment_expired","expired","bridge_not_ready",0,30000,true);
            PS::LogStructuredFallback<LogLevel::Warning>(
                PS::StructuredDiagnosticB3D::Report("RS-CORE-050","Warning","Blocked",
                "Registry bridge attachment expired","The 30-second attach window expired without an authoritative GameState.","RegistryBridge","Finalization"),
                STR("Registry bridge deferred attachment timed out after 30 seconds; this world has no authoritative GameState.\n"));
            m_pendingMode.Reset();
        }
    } catch(const std::exception& error) {
        Trace("bridge.lifecycle",pending,"player","attach",0,{},"player_bridge_attachment_expired","rejected","bridge_not_ready",0,0,true);
        PS::LogStructuredFallback<LogLevel::Warning>(
            PS::StructuredDiagnosticB3D::Report("RS-CORE-051","Warning","Partial",
            "Registry bridge attachment failed",error.what(),"RegistryBridge","Application"),
            STR("Registry bridge unavailable: {}.\n"),PS::ToWideSafe(error.what()));
        m_pendingMode.Reset();
    }
}

void RegistryBridge::CatchUpAuthorityWorld() {
    if(m_authorityComponent||m_pendingMode.Get())return;
    // Dedicated initialization can finish inside/after InitGameState, before
    // these callbacks existed. Catch the existing mode once, without waiting
    // for a second world load or scanning every frame.
    auto* type=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.GameModeBase"));
    if(!type)return;
    TArray<UObject*> modes;UECustom::UObjectGlobals::GetObjectsOfClass(type,modes,true,
        static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));
    if(modes.Num()<0||modes.Num()>64)throw std::runtime_error("Existing authority world roster exceeds its safe bound");
    AGameModeBase* selected=nullptr;
    for(auto* object:modes){
        auto* world=object?object->GetWorld():nullptr;if(!world)continue;
        const auto path=RC::to_string(world->GetPathName());
        if(!path.starts_with("/Game/Maps/")||!IsGameplayRoleWorld(path))continue;
        if(selected)throw std::runtime_error("Existing authority worlds are ambiguous; awaiting the next native world-ready event");
        selected=static_cast<AGameModeBase*>(object);
    }
    if(!selected)return;
    ResetWorld();m_pendingMode.Assign(selected);m_retryElapsed=m_retryInterval=0.0f;
    PS::Log<LogLevel::Normal>(STR("[NETWORK][STARTUP-CATCHUP] Existing authoritative world queued for bounded bridge attachment.\n"));
}

void RegistryBridge::StartRetryTick() {
    if(m_retryTick!=Hook::ERROR_ID)return;
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");
    options.HookName=TEXT("RegistryBridgeDeferredAttach");
    m_retryTick=Hook::RegisterEngineTickPostCallback(
        [this](Hook::TCallbackIterationData<void>&,UEngine*,float deltaSeconds,bool){
            TickModDataRelay(deltaSeconds);
            PS::SlowWork::Measure("network.authority",[&]{TickAuthorityActions(deltaSeconds);});
            if(m_pendingMode.Get() && !m_authorityComponent)
                PS::SlowWork::Measure("network.attach",[&]{RetryAttach(deltaSeconds);});
        },options);
    if(m_retryTick==Hook::ERROR_ID){Trace("bridge.lifecycle",m_pendingMode.Get(),"player","attach",0,{},"player_bridge_attachment_expired","rejected","bridge_not_ready",0,0,true);m_pendingMode.Reset();PS::Log<LogLevel::Warning>(STR("Registry bridge deferred attachment could not be scheduled.\n"));}
}

void RegistryBridge::TickModDataRelay(float deltaSeconds){
    if(m_relayPeers.empty())return;m_relayElapsed+=std::max(0.0f,deltaSeconds);if(m_relayElapsed<0.25f)return;m_relayElapsed=0;
    for(auto at=m_relayPeers.begin();at!=m_relayPeers.end();){auto* component=at->second.Get();
        if(!component||component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))){m_modDataRelay.Remove(at->first);at=m_relayPeers.erase(at);}else ++at;}
    for(const auto& [peer,packet]:m_modDataRelay.Drain(4,PS::ModDataRelay::Clock::now()))try{
        const auto at=m_relayPeers.find(peer);if(at==m_relayPeers.end())continue;auto* component=at->second.Get();if(!component)continue;
        SendNotification(component,"mod.data",packet.at("channel").get<std::string>(),packet.at("sequence").get<int64_t>(),packet.dump());
    }catch(const std::exception&){PS::Log<LogLevel::Verbose>(STR("[MOD-DATA][DELIVERY-UNAVAILABLE] Mod-data packet not delivered; subscriber may request a fresh snapshot.\n"));}
}

void RegistryBridge::Observe(UObject* source,UFunction* function) {
    if(!source||!function)return;
    const auto name=function->GetFName();const auto& names=Names();
    if(name!=names.World&&name!=names.Activation&&name!=names.Identity&&name!=names.Registry&&name!=names.Persistent)return;
    if(BridgeClassMatches<BridgeKind::World>(source)) {
        const auto contract=ResolveWorldContract();
        if(ReadInt(source,contract.ProtocolVersion)!=Protocol)throw std::runtime_error("World bridge protocol is unsupported");
        if(name==Names().World) {
            const auto revision=static_cast<uint32_t>(ReadInt(source,contract.WorldStateRevision));if(!revision||revision<=m_seenPersistentRevision)return;
            const auto envelope=nlohmann::json::parse(ReadString(source,contract.WorldStateEnvelope));
            if(!envelope.is_object()||envelope.value("kind",std::string{})!="RuneSchemaWorldSnapshot")throw std::runtime_error("World bridge snapshot is invalid");
            m_seenPersistentRevision=revision;ReplayPresentationSnapshot(source,envelope.dump());if(PersistentState)PersistentState(source,"world.state","all",envelope.dump());
        } else if(name==Names().Activation) {
            const auto revision=static_cast<uint32_t>(ReadInt(source,contract.ActivationRevision));if(!revision||revision<=m_seenActivationRevision)return;
            const auto envelope=nlohmann::json::parse(ReadString(source,contract.ActivationEnvelope));if(!envelope.is_object()||!envelope.contains("key")||!envelope["key"].is_string())throw std::runtime_error("World activation envelope is invalid");m_seenActivationRevision=revision;
            if(ClientNotification)ClientNotification(source,"registry.action",envelope["key"].get<std::string>(),envelope.dump());
        }
        return;
    }
    if(!BridgeClassMatches<BridgeKind::Player>(source))return;
    const auto contract=ResolveContract();
    if(contract.Compact) {
        if(name!=Names().Identity)return;
        const auto state=nlohmann::json::parse(ReadString(source,contract.IdentityPayload));
        if(!state.is_object() || state.value("kind",std::string{})!="RuneSchemaRegistryBridgeState"
            || state.value("protocolVersion",0)!=Protocol)throw std::runtime_error("Compact registry bridge state is invalid");
        const auto& local=m_registrySnapshot;
        const auto remote=state.value("registryFingerprint",std::string{});
        const auto localMapping=LocalMapping().Fingerprint;
        const auto remoteMapping=state.value("mappingFingerprint",std::string{});
        const auto comparison=remote+"|"+EffectiveRegistryFingerprint(local)+"|"+localMapping+"|"+remoteMapping;
        if(m_sessionComparisonFingerprint!=comparison) {
            m_sessionRegistryOwners=OwnerFingerprints(state,"registryOwners");
            m_sessionNetworkOwners=OwnerFingerprints(state,"networkIdentityOwners");
            const auto registrySession=PS::RegistryManifestSummary::Negotiate(
                PS::RegistryManifestSummary::Owners(local),m_sessionRegistryOwners);
            const auto networkSession=PS::RegistryManifestSummary::Negotiate(
                PS::NetworkIdentityState::OwnerSnapshot(),m_sessionNetworkOwners);
            const auto parity=PS::RegistryManifestSummary::Compatibility(registrySession,networkSession,
                state.contains("registryOwners")&&state.contains("networkIdentityOwners"),state.contains("networkIdentityLanes"),
                PS::NetworkIdentityState::Snapshot(),OwnerFingerprints(state,"networkIdentityLanes"),
                PS::NetworkIdentityState::MatchesAuthorityOrderProof(state.value("nativeIdAuthority",std::string{})));
            if(parity=="different")
                PS::Log<LogLevel::Warning>(STR("[RS-NET-002] RuneSchema registry parity DIFFERENT: registry ({}; {}); network ({}; {}). Content differences are warnings; ID synchronization is reported separately and operations remain authority-validated.\n"),
                    PS::ToWideSafe(registrySession.Summary().c_str()),PS::ToWideSafe(PS::RegistryManifestSummary::Difference(PS::RegistryManifestSummary::Owners(local),m_sessionRegistryOwners).c_str()),
                    PS::ToWideSafe(networkSession.Summary().c_str()),PS::ToWideSafe(PS::RegistryManifestSummary::Difference(PS::NetworkIdentityState::OwnerSnapshot(),m_sessionNetworkOwners).c_str()));
            else PS::Log<LogLevel::Normal>(STR("RuneSchema shared registry parity {}: registry ({}); network ({}); client extras remain active, unavailable local content is permitted, and actions validate independently.\n"),
                PS::ToWideSafe(parity.c_str()),PS::ToWideSafe(registrySession.Summary().c_str()),PS::ToWideSafe(networkSession.Summary().c_str()));
            PS::Log<LogLevel::Normal>(STR("RuneSchema platform mapping telemetry: {}. Platform-local reflection remains active; mapping files are not shared.\n"),PS::ToWideSafe(MappingState(localMapping,remoteMapping).c_str()));
            m_sessionAuthorityFingerprint=remote;m_sessionComparisonFingerprint=comparison;
        }
        const auto registry=state.value("registryRevision",0u);
        if(registry>m_seenRegistryRevision){m_seenRegistryRevision=registry;SendCompatibilityAck(remote);}
        const auto activation=state.value("activationRevision",0u);
        if(activation>m_seenActivationRevision) {
            if(!state.contains("activation") || !state["activation"].is_object() || !state["activation"].contains("key") || !state["activation"]["key"].is_string())
                throw std::runtime_error("Compact registry activation envelope is invalid");
            m_seenActivationRevision=activation;
            if(ClientNotification)ClientNotification(source,"registry.action",state["activation"]["key"].get<std::string>(),state["activation"].dump());
        }
        const auto persistent=state.value("persistentStateRevision",0u);
        if(persistent>m_seenPersistentRevision){if(!state.contains("persistentState") || !state["persistentState"].is_object())throw std::runtime_error("Compact registry persistent state is invalid");m_seenPersistentRevision=persistent;ReplayPresentationSnapshot(source,state["persistentState"].dump());if(PersistentState)PersistentState(source,"world.state","all",state["persistentState"].dump());}
        return;
    }
    const auto protocol=ReadInt(source,contract.ProtocolVersion);
    if(protocol!=Protocol)throw std::runtime_error(std::format("Registry bridge protocol {} is unsupported",protocol));
    if(name==Names().Registry) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.RegistryRevision));
        if(!revision || revision<=m_seenRegistryRevision)return;
        const auto& local=m_registrySnapshot;
        const auto remote=ReadString(source,contract.RegistryFingerprint);
        const bool manifestMatch=!local.empty()&&EffectiveRegistryFingerprint(local)==remote;
        if(!manifestMatch&&m_sessionAuthorityFingerprint!=remote) {
            PS::Log<LogLevel::Normal>(STR("RuneSchema legacy registry parity unverified: authority fingerprint differs and legacy telemetry cannot verify native network IDs. No registry IDs were rewritten.\n"));
            m_sessionAuthorityFingerprint=remote;
        }
        m_seenRegistryRevision=revision;
        SendCompatibilityAck(remote);
    } else if(name==Names().Activation) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.ActivationRevision));
        if(!revision || revision<=m_seenActivationRevision)return;
        const auto envelope=nlohmann::json::parse(ReadString(source,contract.ActivationEnvelope));
        if(!envelope.is_object() || !envelope.contains("key") || !envelope["key"].is_string())
            throw std::runtime_error("Registry activation envelope is invalid");
        m_seenActivationRevision=revision;
        if(ClientNotification)ClientNotification(source,"registry.action",envelope["key"].get<std::string>(),envelope.dump());
    } else if(name==Names().Persistent) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.PersistentStateRevision));
        if(!revision || revision<=m_seenPersistentRevision)return;
        const auto envelope=nlohmann::json::parse(ReadString(source,contract.PersistentStateEnvelope));
        if(!envelope.is_object())throw std::runtime_error("Registry persistent-state envelope is invalid");
        m_seenPersistentRevision=revision;ReplayPresentationSnapshot(source,envelope.dump());if(PersistentState)PersistentState(source,"world.state","all",envelope.dump());
    }
}

bool RegistryBridge::PublishActivation(const std::string& envelope) {
    if(m_worldAuthorityComponent) {
        const auto world=ResolveWorldContract();const auto parsed=nlohmann::json::parse(envelope);
        if(!parsed.is_object()||!parsed.contains("key")||!parsed["key"].is_string())return false;
        WriteString(m_worldAuthorityComponent,world.ActivationEnvelope,envelope);WriteInt(m_worldAuthorityComponent,world.ActivationRevision,ReadInt(m_worldAuthorityComponent,world.ActivationRevision)+1);return true;
    }
    if(!m_authorityComponent)return false;
    const auto contract=ResolveContract();
    const auto parsed=nlohmann::json::parse(envelope);
    if(!parsed.is_object() || !parsed.contains("key") || !parsed["key"].is_string())return false;
    if(contract.Compact) {
        m_activationEnvelope=envelope;++m_activationRevision;PublishCompactPayload();return true;
    }
    WriteString(m_authorityComponent,contract.ActivationEnvelope,envelope);
    WriteInt(m_authorityComponent,contract.ActivationRevision,ReadInt(m_authorityComponent,contract.ActivationRevision)+1);
    return true;
}

bool RegistryBridge::PublishPersistentState(const std::string& envelope) {
    if(m_worldAuthorityComponent) {
        if(!nlohmann::json::parse(envelope).is_object())return false;const auto world=ResolveWorldContract();
        WriteString(m_worldAuthorityComponent,world.WorldStateEnvelope,envelope);WriteInt(m_worldAuthorityComponent,world.WorldStateRevision,ReadInt(m_worldAuthorityComponent,world.WorldStateRevision)+1);return true;
    }
    if(!m_authorityComponent)return false;
    if(!nlohmann::json::parse(envelope).is_object())return false;
    const auto contract=ResolveContract();
    if(contract.Compact) {
        m_persistentStateEnvelope=envelope;++m_persistentRevision;PublishCompactPayload();return true;
    }
    WriteString(m_authorityComponent,contract.PersistentStateEnvelope,envelope);
    WriteInt(m_authorityComponent,contract.PersistentStateRevision,ReadInt(m_authorityComponent,contract.PersistentStateRevision)+1);
    return true;
}

bool RegistryBridge::UpsertWorldInstance(const std::string& instanceId,const std::string& record) {
    if(!Token(instanceId,512))throw std::runtime_error("World instance id is invalid");
    const auto parsed=nlohmann::json::parse(record);
    if(!parsed.is_object()||parsed.size()>48||record.size()>MaxActionPayloadBytes)
        throw std::runtime_error("World instance record must be a bounded object");
    if(!m_worldInstances.contains(instanceId)&&m_worldInstances.size()>=MaxWorldInstances)
        throw std::runtime_error("World instance ledger capacity exceeded");
    m_worldInstances[instanceId]=record;++m_worldLedgerRevision;
    return PublishWorldSnapshot();
}

bool RegistryBridge::RemoveWorldInstance(const std::string& instanceId,const std::string& reason) {
    if(!Token(instanceId,512))throw std::runtime_error("World instance id is invalid");
    if(!m_worldInstances.erase(instanceId))return false;
    ++m_worldLedgerRevision;RecordDiagnostic("world.state",instanceId,static_cast<int64_t>(m_worldLedgerRevision),"removed",reason);
    return PublishWorldSnapshot();
}

bool RegistryBridge::PublishWorldSnapshot() {
    nlohmann::json instances=nlohmann::json::array();
    std::vector<std::string> keys;keys.reserve(m_worldInstances.size());
    for(const auto& [key,_]:m_worldInstances)keys.push_back(key);std::sort(keys.begin(),keys.end());
    for(const auto& key:keys) {
        auto entry=nlohmann::json::parse(m_worldInstances.at(key));entry["instanceId"]=key;instances.push_back(std::move(entry));
    }
    const nlohmann::json snapshot={{"kind","RuneSchemaWorldSnapshot"},{"version",1},{"revision",m_worldLedgerRevision},{"instances",std::move(instances)}};
    const auto encoded=snapshot.dump();if(encoded.size()>MaxEnvelopeBytes)throw std::runtime_error("World snapshot exceeds cooked bridge capacity");
    const auto published=PublishPersistentState(encoded);
    RecordDiagnostic("world.state","all",static_cast<int64_t>(m_worldLedgerRevision),published?"published":"deferred",std::format("{} instance(s)",m_worldInstances.size()));
    return published;
}

std::string RegistryBridge::DiagnosticOwner(UObject* object) {
    if(!object)return "system";
    auto [at,inserted]=m_diagnosticOwners.try_emplace(object,0);
    if(inserted)at->second=++m_nextDiagnosticOwner;
    return "player-"+std::to_string(at->second);
}

uint64_t RegistryBridge::DiagnosticInstance(UObject* object) {
    auto [at,inserted]=m_diagnosticOwners.try_emplace(object,0);
    if(inserted)at->second=++m_nextDiagnosticOwner;
    return at->second;
}

std::string RegistryBridge::DiagnosticCorrelation(UObject* object,int64_t revision) {
    auto [at,inserted]=m_diagnosticOwners.try_emplace(object,0);
    if(inserted)at->second=++m_nextDiagnosticOwner;
    return m_diagnostics.Correlation(at->second,revision);
}

void RegistryBridge::Trace(const std::string& channel,UObject* owner,const std::string& entity,
    const std::string& action,int64_t revision,const std::string& correlation,
    const std::string& stage,const std::string& outcome,const std::string& reason,
    uint32_t retries,uint64_t elapsedMs,bool warning) {
    bool authority=false;try{authority=ActorAuthority(owner);}catch(...){}
    m_diagnostics.Record({.networkRole=Storefront::IsDedicatedServer()?"dedicated-server":(authority?"listen-server":"client"),
        .side=authority?"authority":"requester",.channel=channel,.owner=DiagnosticOwner(owner),.entity=entity,
        .action=action,.requestRevision=revision,.correlationId=correlation,.stage=stage,.outcome=outcome,
        .reasonCode=reason,.retryCount=retries,.elapsedMs=elapsedMs},warning);
}

std::string RegistryBridge::DiagnosticSnapshot() const {return m_diagnostics.Snapshot().dump(2);}

void RegistryBridge::RecordDiagnostic(const std::string& channel,const std::string& entity,int64_t revision,
    const std::string& result,const std::string& detail) {
    std::string reason="internal_error";
    if(detail.find("rate limit")!=std::string::npos)reason="rate_limited";
    else if(detail.find("stale")!=std::string::npos)reason="stale_revision";
    else if(detail.find("not registered")!=std::string::npos)reason="registry_missing";
    else if(detail.find("replicated")!=std::string::npos||detail.find("unavailable")!=std::string::npos)reason="bridge_not_ready";
    else if(detail.find("removal")!=std::string::npos)reason="inventory_not_confirmed";
    else if(detail.find("layout")!=std::string::npos||detail.find("contract")!=std::string::npos)reason="callback_contract_mismatch";
    else if(result=="expired")reason="expired";
    Trace(channel,nullptr,entity,"observe",revision,{},"diagnostic",result,reason,0,0,result!="accepted"&&result!="published");
}

void RegistryBridge::Start() {
    if(m_started)return;
    try{LoadAuthorityActions();LoadPresentationRoutes();}catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("Registry transport manifest unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");
    options.HookName=TEXT("RegistryBridgeWorldStarting");
    m_worldStarting=Hook::RegisterInitGameStatePreCallback([this](Hook::TCallbackIterationData<void>&,AGameModeBase*){ResetWorld();},options);
    options.HookName=TEXT("RegistryBridgeWorldReady");
    m_worldReady=Hook::RegisterInitGameStatePostCallback([this](Hook::TCallbackIterationData<void>&,AGameModeBase* mode){
        auto* world=mode?static_cast<UObject*>(mode)->GetWorld():nullptr;
        if(!world || !IsGameplayRoleWorld(RC::to_string(world->GetPathName()))) {
            m_pendingMode.Reset();return;
        }
        m_pendingMode.Assign(mode);m_retryElapsed=m_retryInterval=0.0f;
        try{if(!Attach(mode))StartRetryTick();}catch(const std::exception& error){m_pendingMode.Reset();PS::LogStructuredFallback<LogLevel::Warning>(
            PS::StructuredDiagnosticB3D::Report("RS-CORE-051","Warning","Partial",
            "Registry bridge attachment failed",error.what(),"RegistryBridge","Application"),
            STR("Registry bridge unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
    },options);
    options.HookName=TEXT("RegistryAuthorityAction");
    m_authorityPre=Hook::RegisterProcessEventPreCallback([this](Hook::TCallbackIterationData<void>& iteration,UObject* source,UFunction* function,void* parameters){
        if(!source||!function)return;const auto name=function->GetFName();
        if(!Names().Pre(name)&&!AuthorityEntry(name)&&source!=m_activeAuthorityGraph)return;
        try{ObservePresentationPre(source,function,parameters);ObserveAuthorityPre(source,function,parameters);}catch(const std::exception& error){
            const auto name=function?function->GetFName():FName();
            if(source&&function&&(source->GetClassPrivate()->GetPathName()==PluginPresentationClass
                ||name==FName(TEXT("ServerAcknowledgeRuneSchemaPresentation"),FNAME_Add)
                ||name==FName(TEXT("ServerRequestRuneSchemaResync"),FNAME_Add)))iteration.PreventOriginalFunctionCall();
            PS::LogStructuredFallback<LogLevel::Warning>(
                PS::StructuredDiagnosticB3D::Report("RS-CORE-052","Warning","Skipped",
                "Registry transport request rejected",error.what(),"RegistryTransportPrecheck","Validation"),
                STR("Registry transport precheck rejected: {}.\n"),PS::ToWideSafe(error.what()));
        }
    },options);
    options.HookName=TEXT("RegistryBridgeRepNotify");
    m_processEvent=Hook::RegisterProcessEventPostCallback([this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void* parameters){
        if(!source||!function)return;const auto name=function->GetFName();
        if(!Names().Post(name)&&!AuthorityEntry(name))return;
        try{ObservePlayerLifecycle(source,function);ObserveAuthorityPost(source,function,parameters);Observe(source,function);ObserveClientTransport(source,function,parameters);ObservePresentationTransport(source,function,parameters);}catch(const std::exception& error){PS::LogStructuredFallback<LogLevel::Warning>(
            PS::StructuredDiagnosticB3D::Report("RS-CORE-053","Warning","Partial",
                "Registry bridge update failed",error.what(),"RegistryBridgeRepNotify","Application"),
            STR("Registry bridge update rejected: {}.\n"),PS::ToWideSafe(error.what()));}
    },options);
    StartRetryTick();
    m_started=m_worldStarting!=Hook::ERROR_ID && m_worldReady!=Hook::ERROR_ID
        && m_authorityPre!=Hook::ERROR_ID && m_processEvent!=Hook::ERROR_ID
        && m_retryTick!=Hook::ERROR_ID;
    if(!m_started){Stop();PS::LogStructuredFallback<LogLevel::Warning>(
        PS::StructuredDiagnosticB3D::Report("RS-CORE-054","Warning","Blocked",
            "Registry bridge hooks unavailable","One or more native transport lifecycle hooks failed registration.","RegistryBridgeHooks","Validation"),
        STR("Registry bridge hooks could not be installed.\n"));}
    else if(Storefront::IsDedicatedServer())try{
        PS::SlowWork::Measure("network.startup-catchup",[&]{CatchUpAuthorityWorld();});
    }catch(const std::exception& error){PS::LogStructuredFallback<LogLevel::Warning>(
        PS::StructuredDiagnosticB3D::Report("RS-CORE-055","Warning","Partial",
                "Registry bridge startup catch-up failed",error.what(),"RegistryBridgeCatchup","Application"),
        STR("Registry bridge startup catch-up unavailable: {}. Native world-ready hooks remain active.\n"),PS::ToWideSafe(error.what()));}
}

void RegistryBridge::Stop() {
    for(auto* id:{&m_worldStarting,&m_worldReady,&m_retryTick,&m_authorityPre,&m_processEvent})if(*id!=Hook::ERROR_ID){Hook::UnregisterCallback(*id);*id=Hook::ERROR_ID;}
    ResetWorld();m_started=false;
}
}
