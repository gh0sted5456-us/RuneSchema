#include "Runtime/RegistryBridge.h"

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
#include "Generator/ToolRequest.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/Logging.h"
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
constexpr auto DamageNotifyClass = TEXT("/Game/Gameplay/Character/Player/AnimNotifies/AnimNotify_AttackDamagePoint.AnimNotify_AttackDamagePoint_C");
constexpr int32_t Protocol = 1;
constexpr size_t MaxEnvelopeBytes = 16 * 1024;
constexpr size_t MaxActionPayloadBytes = 4 * 1024;
constexpr uint32_t MaxRequestsPerSecond = 8;
constexpr size_t MaxWorldInstances = 512;
constexpr auto BuildIdentity = "0.7.5.24";

const PS::MappingBackbone::Mapping& LocalMapping() {
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

std::string AssetPackage(std::string path) {
    const auto slash=path.find_last_of('/');
    const auto dot=path.find('.',slash==std::string::npos?0:slash);
    if(dot!=std::string::npos)path.resize(dot);
    return path;
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

void RegistryBridge::ResetWorld() {
    m_authorityComponent=nullptr;m_worldAuthorityComponent=nullptr;
    m_pendingMode=nullptr;
    m_retryElapsed=m_retryInterval=0.0f;
    m_seenRegistryRevision=m_seenActivationRevision=m_seenPersistentRevision=0;
    m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;m_playerBridgeInterval=0.0f;
    m_manifestFingerprint.clear();m_activationEnvelope.clear();m_persistentStateEnvelope.clear();
    m_registryRevision=m_activationRevision=m_persistentRevision=0;
    m_outboundRevision=0;m_requestWindows.clear();m_worldInstances.clear();m_worldLedgerRevision=0;
}

void RegistryBridge::SetRegistrySnapshot(std::string snapshot) {
    if(snapshot.size()>4*1024*1024)throw std::runtime_error("Merged mod registry exceeds 4 MiB");
    const auto parsed=nlohmann::json::parse(snapshot);
    if(!parsed.is_object() || parsed.value("kind",std::string{})!="RuneSchemaRegistryBridgeManifest"
        || parsed.value("schemaVersion",0)!=1 || parsed.value("protocolVersion",0)!=Protocol
        || !parsed.contains("entries") || !parsed["entries"].is_array())
        throw std::runtime_error("Merged mod registry contract is invalid");
    m_registrySnapshot=std::move(snapshot);
    m_manifestFingerprint=Fingerprint(m_registrySnapshot);
    LoadAuthorityActions();
}

std::string RegistryBridge::CompactPayload() const {
    const auto& mapping=LocalMapping();
    nlohmann::json payload={{"kind","RuneSchemaRegistryBridgeState"},{"protocolVersion",Protocol},
        {"build",BuildIdentity},{"channels",{"system.compat","registry.action","quest.control","quest.state","dialogue.session","event.state","spawn.state","npc.state","vendor.state","player.profile","nameplate.profile","world.state","notify.player"}},
        {"registryFingerprint",m_manifestFingerprint},{"mappingFingerprint",mapping.Fingerprint},{"registryRevision",m_registryRevision},
        {"activationRevision",m_activationRevision},{"persistentStateRevision",m_persistentRevision}};
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
    m_authorityActions.clear();m_authoritySelectionPaths.clear();
    for(const auto& entry:parsed["entries"]) {
        if(!entry.contains("authority") || !entry["authority"].is_object())continue;
        const auto& authority=entry["authority"];
        const auto graph=authority.value("graphClass",std::string{}),action=authority.value("action",std::string{}),asset=authority.value("dataAsset",std::string{});
        if(graph.empty() || (action!="SpawnFollower" && action!="ExecuteGraph") || (action=="SpawnFollower" && asset.empty()))
            throw std::runtime_error("Registry authority action is malformed");
        AuthorityAction candidate{action,asset,entry.value("key",std::string{}),graph,authority.value("function",std::string{"Trigger"}),{}};
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
        remember(candidate.DataAsset);remember(candidate.GraphClass);
        if(entry.contains("metadata") && entry["metadata"].is_object()
            && entry["metadata"].contains("selectors") && entry["metadata"]["selectors"].is_array())
            for(const auto& selector:entry["metadata"]["selectors"])
                if(selector.is_string())remember(selector.get<std::string>());
    }
}

void RegistryBridge::ObserveSelectionNotify(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function || !parameters || m_authoritySelectionPaths.empty())return;
    if(source->GetClassPrivate()->GetPathName()!=DamageNotifyClass
        || function->GetFName()!=FName(TEXT("Received_Notify"),FNAME_Add))return;

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

void RegistryBridge::EnsurePlayerBridges() {
    auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
    if(!playerType)return;
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);
    for(auto* player:players)if(player && player->IsA<AActor>() && !player->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))) {
        DragonWilds::ActorHelper::FunctionCall authority(player,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(authority.Result<bool>())EnsureBridgeComponent(static_cast<AActor*>(player),false);
    }
}

void RegistryBridge::ForwardRegistryRequest(UObject* component,const std::string& key) {
    if(!component||!Token(key,512))throw std::runtime_error("Registry bridge request key is invalid");
    if(auto* rpc=component->GetFunctionByNameInChain(TEXT("ServerRequestRuneSchemaAction"));rpc&&rpc->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer)) {
        DragonWilds::ActorHelper::FunctionCall request(component,rpc);
        request.Arg(TEXT("Channel"),std::string("registry.action")).Arg(TEXT("EntityId"),key)
            .Arg(TEXT("ActionKey"),std::string("execute")).Arg(TEXT("Payload"),std::string("{}"))
            .Arg(TEXT("Revision"),++m_outboundRevision).Invoke();
        return;
    }
    auto* legacy=component->GetFunctionByNameInChain(TEXT("ServerRequestRegistryAction"));
    if(!legacy||!legacy->HasAnyFunctionFlags(FUNC_Net|FUNC_NetServer))throw std::runtime_error("Cooked registry server RPC is unavailable");
    DragonWilds::ActorHelper::FunctionCall request(component,legacy);request.Arg(TEXT("ActionKey"),key).Invoke();
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

void RegistryBridge::HandleGenericRequest(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||source->GetClassPrivate()->GetPathName()!=BridgeClass||!parameters)return;
    const auto channel=ParameterString(function,parameters,TEXT("Channel"),64);
    const auto entity=ParameterString(function,parameters,TEXT("EntityId"),512);
    const auto action=ParameterString(function,parameters,TEXT("ActionKey"),64);
    const auto payloadText=ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes);
    const auto revision=ParameterInt64(function,parameters,TEXT("Revision"));
    if(!Token(channel,64)||!Token(entity,512)||!Token(action,64)||revision<=0)throw std::runtime_error("Generic bridge envelope is invalid");
    const auto payload=nlohmann::json::parse(payloadText);if(!payload.is_object()||payload.size()>32)throw std::runtime_error("Generic bridge payload must be a bounded object");
    DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();auto* caster=owner.Result<UObject*>();
    auto* actorClass=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));if(!caster||!actorClass||!caster->IsA(actorClass))throw std::runtime_error("Generic bridge has no owning actor");
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();if(!authority.Result<bool>())return;
    const auto ownerKey=RC::to_string(caster->GetPathName())+"|"+channel;auto& window=m_requestWindows[ownerKey];const auto now=std::chrono::steady_clock::now();
    if(window.Started.time_since_epoch().count()==0||now-window.Started>=std::chrono::seconds(1)){window.Started=now;window.Count=0;}
    if(++window.Count>MaxRequestsPerSecond)throw std::runtime_error("Generic bridge request rate limit exceeded");
    if(revision<=window.Revision)throw std::runtime_error("Generic bridge request revision is stale");window.Revision=revision;
    try {
        if(channel=="system.compat") {
            if(action!="hello")throw std::runtime_error("Unsupported compatibility action verb");
            const bool protocolMatch=payload.value("protocolVersion",0)==Protocol;
            const bool buildMatch=payload.value("build",std::string{})==BuildIdentity;
            const bool registryMatch=payload.value("registryFingerprint",std::string{})==m_manifestFingerprint;
            const bool compatible=protocolMatch&&buildMatch&&registryMatch;
            const auto mappingState=MappingState(LocalMapping().Fingerprint,payload.value("mappingFingerprint",std::string{}));
            SendReceipt(source,channel,entity,revision,compatible,compatible?
                "RuneSchema compatibility accepted: "+std::string(BuildIdentity)+" (mapping="+mappingState+")":
                "RuneSchema compatibility differs (protocol="+std::string(protocolMatch?"match":"different")+
                ", build="+(buildMatch?"match":"different")+", registry="+(registryMatch?"match":"different")+
                ", mapping="+mappingState+").");return;
        }
        if(channel=="registry.action") {
            if(action!="execute")throw std::runtime_error("Unsupported registry action verb");
            const auto found=m_authorityActions.find(entity);if(found==m_authorityActions.end())throw std::runtime_error("Registry action key is not registered");
            InvokeAuthorityActionForCaster(caster,found->second);SendReceipt(source,channel,entity,revision,true,"Registry action executed by authority.");return;
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
            SendReceipt(source,channel,entity,revision,true,detail.empty()?"Authority action completed.":detail);return;
        }
        throw std::runtime_error("Unsupported RuneSchema action channel");
    }catch(const std::exception& error){RecordDiagnostic(channel,entity,revision,"rejected",error.what());SendReceipt(source,channel,entity,revision,false,error.what());throw;}
}

void RegistryBridge::ObserveClientTransport(UObject* source,UFunction* function,void* parameters) {
    if(!source||!function||source->GetClassPrivate()->GetPathName()!=BridgeClass||!parameters)return;const auto name=function->GetFName();
    if(name!=FName(TEXT("ClientRuneSchemaReceipt"),FNAME_Add)&&name!=FName(TEXT("ClientRuneSchemaNotification"),FNAME_Add))return;
    const auto channel=ParameterString(function,parameters,TEXT("Channel"),64),entity=ParameterString(function,parameters,TEXT("EntityId"),512);
    if(name==FName(TEXT("ClientRuneSchemaReceipt"),FNAME_Add)) {
        (void)ParameterInt64(function,parameters,TEXT("Revision"));
        const auto success=ParameterBool(function,parameters,TEXT("Success"));
        const auto detail=ParameterString(function,parameters,TEXT("Detail"),512);
        if(channel=="system.compat" && (!success || detail.find("mapping=different")!=std::string::npos
            || detail.find("mapping=authority-only")!=std::string::npos || detail.find("mapping=client-only")!=std::string::npos))
            PS::Log<LogLevel::Warning>(STR("{}\n"),PS::ToWideSafe(detail.c_str()));
    } else {
        const auto payload=ParameterString(function,parameters,TEXT("Payload"),MaxActionPayloadBytes);(void)nlohmann::json::parse(payload);
        DragonWilds::ActorHelper::FunctionCall owner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));owner.Invoke();
        if(channel=="world.state"&&PersistentState)PersistentState(owner.Result<UObject*>(),channel,entity,payload);
        if(channel=="helpy.players")try {
            const auto roster=nlohmann::json::parse(payload);
            if(roster.value("Kind",std::string{})=="HelpyPlayers1"&&roster.contains("Players")&&roster["Players"].is_array()) {
                nlohmann::json rows=nlohmann::json::array();
                for(const auto& player:roster["Players"])if(player.is_object())rows.push_back({{"Name",player.value("Name",std::string("Player"))},{"Path",player.value("Guid",std::string{})},{"Guid",player.value("Guid",std::string{})}});
                PS::SpawnToolRequests::CatalogProgress({{"Players",std::move(rows)}});
            }
        }catch(...){}
        if(ClientNotification)ClientNotification(owner.Result<UObject*>(),channel,entity,payload);
    }
}

void RegistryBridge::SendCompatibilityAck(const std::string& remoteFingerprint) {
    if(remoteFingerprint.empty())return;auto* playerType=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));if(!playerType)return;
    TArray<UObject*> players;UECustom::UObjectGlobals::GetObjectsOfClass(playerType,players,true);const auto contract=ResolveContract();
    for(auto* player:players)if(player&&player->IsA<AActor>())try {
        DragonWilds::ActorHelper::FunctionCall local(player,TEXT("/Script/Engine.Pawn:IsLocallyControlled"));local.Invoke();if(!local.Result<bool>())continue;
        const auto components=static_cast<AActor*>(player)->GetComponentsByClass(contract.Type);if(!components.Num())continue;auto* rpc=components[0]->GetFunctionByNameInChain(TEXT("ServerRequestRuneSchemaAction"));if(!rpc)continue;
        const nlohmann::json payload={{"protocolVersion",Protocol},{"build",BuildIdentity},{"registryFingerprint",m_manifestFingerprint},
            {"mappingFingerprint",LocalMapping().Fingerprint},{"channels",{"system.compat","registry.action","quest.control","quest.state","dialogue.session","event.state","spawn.state","npc.state","vendor.state","player.profile","nameplate.profile","world.state","notify.player"}}};
        DragonWilds::ActorHelper::FunctionCall request(components[0],rpc);request.Arg(TEXT("Channel"),std::string("system.compat")).Arg(TEXT("EntityId"),std::string("client"))
            .Arg(TEXT("ActionKey"),std::string("hello")).Arg(TEXT("Payload"),payload.dump()).Arg(TEXT("Revision"),++m_outboundRevision).Invoke();
        DragonWilds::ActorHelper::FunctionCall resync(components[0],rpc);resync.Arg(TEXT("Channel"),std::string("quest.state")).Arg(TEXT("EntityId"),std::string("all"))
            .Arg(TEXT("ActionKey"),std::string("resync")).Arg(TEXT("Payload"),std::string("{}")).Arg(TEXT("Revision"),++m_outboundRevision).Invoke();
        DragonWilds::ActorHelper::FunctionCall world(components[0],rpc);world.Arg(TEXT("Channel"),std::string("world.state")).Arg(TEXT("EntityId"),std::string("all"))
            .Arg(TEXT("ActionKey"),std::string("resync")).Arg(TEXT("Payload"),std::string("{}")).Arg(TEXT("Revision"),++m_outboundRevision).Invoke();return;
    }catch(...){}
}

bool RegistryBridge::RequestAuthority(const std::string& action,const std::string& payload) {
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
    m_authorityActions.clear();m_authoritySelectionPaths.clear();
    for(const auto& entry:parsed["entries"]) {
        if(!entry.contains("authority") || !entry["authority"].is_object())continue;
        const auto& authority=entry["authority"];
        const auto graph=authority.value("graphClass",std::string{});
        const auto action=authority.value("action",std::string{});
        const auto asset=authority.value("dataAsset",std::string{});
        if(graph.empty() || (action!="SpawnFollower" && action!="ExecuteGraph") || (action=="SpawnFollower" && asset.empty()))
            throw std::runtime_error("Registry authority action is malformed");
        AuthorityAction candidate{action,asset,entry.value("key",std::string{}),graph,authority.value("function",std::string{"Trigger"}),{}};
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
        remember(candidate.DataAsset);remember(candidate.GraphClass);
        if(entry.contains("metadata") && entry["metadata"].is_object()
            && entry["metadata"].contains("selectors") && entry["metadata"]["selectors"].is_array())
            for(const auto& selector:entry["metadata"]["selectors"])
                if(selector.is_string())remember(selector.get<std::string>());
    }

    m_manifestFingerprint=Fingerprint(m_registrySnapshot);
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
    m_pendingMode=nullptr;
    return true;
}

void RegistryBridge::ObserveAuthorityPre(UObject* source,UFunction* function,void* parameters) {
    if(!source || !function)return;
    ObserveSelectionNotify(source,function,parameters);
    const auto name=function->GetFName();
    if(name==FName(TEXT("ServerRequestRuneSchemaAction"),FNAME_Add)) {HandleGenericRequest(source,function,parameters);return;}
    if(name==FName(TEXT("ServerRequestRegistryAction"),FNAME_Add)
        && source->GetClassPrivate()->GetPathName()==BridgeClass) {
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
    const auto path=RC::to_string(source->GetClassPrivate()->GetPathName());
    const auto action=std::find_if(m_authorityActions.begin(),m_authorityActions.end(),[&](const auto& row){return row.second.GraphClass==path;});
    if(action==m_authorityActions.end())return;
    if(RC::to_string(name.ToString())!=action->second.EntryFunction)return;
    auto* casterField=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Caster")));
    auto* caster=casterField?casterField->GetObjectPropertyValue(casterField->ContainerPtrToValuePtr<void>(source)):nullptr;
    if(!caster || !caster->IsA<AActor>())return;
    DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
    // On the host/server the original cooked graph is already authoritative.
    // The bridge only forwards client-originated casts to that same graph.
    if(authority.Result<bool>())return;
    const auto contract=ResolveContract();auto components=static_cast<AActor*>(caster)->GetComponentsByClass(contract.Type);
    if(!components.Num())throw std::runtime_error("Owned player registry bridge has not replicated yet");
    ForwardRegistryRequest(components[0],action->second.Key);
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
    if(!graph || !trigger || trigger->GetParmsSize()!=0)
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
    try {graph->ProcessEvent(trigger,nullptr);}catch(...){m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;throw;}
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

void RegistryBridge::ObserveAuthorityPost(UObject* source,UFunction* function) {
    if(!source || !function)return;
    const auto path=RC::to_string(source->GetClassPrivate()->GetPathName());
    const auto found=std::find_if(m_authorityActions.begin(),m_authorityActions.end(),[&](const auto& row){return row.second.GraphClass==path;});
    if(found==m_authorityActions.end() || RC::to_string(function->GetFName().ToString())!=found->second.EntryFunction)return;
    if(source!=m_activeAuthorityGraph) {
        auto* casterField=CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(source->GetClassPrivate(),TEXT("Caster")));
        auto* caster=casterField?casterField->GetObjectPropertyValue(casterField->ContainerPtrToValuePtr<void>(source)):nullptr;
        if(!caster || !caster->IsA<AActor>())return;
        DragonWilds::ActorHelper::FunctionCall authority(caster,TEXT("/Script/Engine.Actor:HasAuthority"));authority.Invoke();
        if(authority.Result<bool>())PublishActionActivation(caster,found->second);
        return;
    }
    auto* graph=m_activeAuthorityGraph;
    m_activeAuthorityGraph=nullptr;m_nativeAuthorityActionObserved=false;
    (void)graph;
}

void RegistryBridge::RetryAttach(float deltaSeconds) {
    m_playerBridgeInterval+=std::max(0.0f,deltaSeconds);
    if(m_playerBridgeInterval>=1.0f){
        m_playerBridgeInterval=0;
        // Registry JSON is loaded after this runtime starts.  A dedicated
        // client therefore has no saved manifest on first launch and the old
        // one-shot load left its action map permanently empty.  Retry only
        // until the registry loader has produced the local validated manifest.
        if(m_authorityActions.empty())try{LoadAuthorityActions();}catch(...){}
        try{EnsurePlayerBridges();}catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("Player registry bridge deferred: {}.\n"),PS::ToWideSafe(error.what()));}
    }
    if(!m_pendingMode || m_authorityComponent)return;
    m_retryElapsed+=std::max(0.0f,deltaSeconds);
    m_retryInterval+=std::max(0.0f,deltaSeconds);
    if(m_retryInterval<0.25f)return;
    m_retryInterval=0.0f;
    try {
        if(Attach(m_pendingMode))return;
        if(m_retryElapsed>=30.0f) {
            PS::Log<LogLevel::Warning>(STR("Registry bridge deferred attachment timed out after 30 seconds; this world has no authoritative GameState.\n"));
            m_pendingMode=nullptr;
        }
    } catch(const std::exception& error) {
        PS::Log<LogLevel::Warning>(STR("Registry bridge unavailable: {}.\n"),PS::ToWideSafe(error.what()));
        m_pendingMode=nullptr;
    }
}

void RegistryBridge::Observe(UObject* source,UFunction* function) {
    if(!source||!function)return;
    if(source->GetClassPrivate()->GetPathName()==WorldBridgeClass) {
        const auto contract=ResolveWorldContract();const auto name=function->GetFName();
        if(ReadInt(source,contract.ProtocolVersion)!=Protocol)throw std::runtime_error("World bridge protocol is unsupported");
        if(name==FName(TEXT("OnRep_WorldStateRevision"),FNAME_Add)) {
            const auto revision=static_cast<uint32_t>(ReadInt(source,contract.WorldStateRevision));if(!revision||revision<=m_seenPersistentRevision)return;
            const auto envelope=nlohmann::json::parse(ReadString(source,contract.WorldStateEnvelope));
            if(!envelope.is_object()||envelope.value("kind",std::string{})!="RuneSchemaWorldSnapshot")throw std::runtime_error("World bridge snapshot is invalid");
            m_seenPersistentRevision=revision;if(PersistentState)PersistentState(source,"world.state","all",envelope.dump());
        } else if(name==FName(TEXT("OnRep_ActivationRevision"),FNAME_Add)) {
            const auto revision=static_cast<uint32_t>(ReadInt(source,contract.ActivationRevision));if(!revision||revision<=m_seenActivationRevision)return;
            const auto envelope=nlohmann::json::parse(ReadString(source,contract.ActivationEnvelope));if(!envelope.is_object()||!envelope.contains("key")||!envelope["key"].is_string())throw std::runtime_error("World activation envelope is invalid");m_seenActivationRevision=revision;
            if(ClientNotification)ClientNotification(source,"registry.action",envelope["key"].get<std::string>(),envelope.dump());
        }
        return;
    }
    if(source->GetClassPrivate()->GetPathName()!=BridgeClass)return;
    const auto contract=ResolveContract();
    const auto name=function->GetFName();
    if(contract.Compact) {
        if(name!=FName(TEXT("OnRep_IdentityPayload"),FNAME_Add))return;
        const auto state=nlohmann::json::parse(ReadString(source,contract.IdentityPayload));
        if(!state.is_object() || state.value("kind",std::string{})!="RuneSchemaRegistryBridgeState"
            || state.value("protocolVersion",0)!=Protocol)throw std::runtime_error("Compact registry bridge state is invalid");
        const auto& local=m_registrySnapshot;
        const auto remote=state.value("registryFingerprint",std::string{});
        const bool manifestMatch=!local.empty()&&Fingerprint(local)==remote;
        if(!manifestMatch)PS::Log<LogLevel::Warning>(STR("Registry bridge manifest differs from authority; continuing in degraded mode and validating each action independently.\n"));
        const auto mappingState=MappingState(LocalMapping().Fingerprint,state.value("mappingFingerprint",std::string{}));
        if(mappingState=="different"||mappingState=="authority-only"||mappingState=="client-only")
            PS::Log<LogLevel::Warning>(STR("RuneSchema mapping differs from authority ({}). Live reflection remains active.\n"),PS::ToWideSafe(mappingState.c_str()));
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
        if(persistent>m_seenPersistentRevision){if(!state.contains("persistentState") || !state["persistentState"].is_object())throw std::runtime_error("Compact registry persistent state is invalid");m_seenPersistentRevision=persistent;if(PersistentState)PersistentState(source,"world.state","all",state["persistentState"].dump());}
        return;
    }
    const auto protocol=ReadInt(source,contract.ProtocolVersion);
    if(protocol!=Protocol)throw std::runtime_error(std::format("Registry bridge protocol {} is unsupported",protocol));
    if(name==FName(TEXT("OnRep_RegistryRevision"),FNAME_Add)) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.RegistryRevision));
        if(!revision || revision<=m_seenRegistryRevision)return;
        const auto& local=m_registrySnapshot;
        const auto remote=ReadString(source,contract.RegistryFingerprint);
        const bool manifestMatch=!local.empty()&&Fingerprint(local)==remote;
        if(!manifestMatch)PS::Log<LogLevel::Warning>(STR("Registry bridge manifest differs from authority; continuing in degraded mode and validating each action independently.\n"));
        m_seenRegistryRevision=revision;
        SendCompatibilityAck(remote);
    } else if(name==FName(TEXT("OnRep_ActivationRevision"),FNAME_Add)) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.ActivationRevision));
        if(!revision || revision<=m_seenActivationRevision)return;
        const auto envelope=nlohmann::json::parse(ReadString(source,contract.ActivationEnvelope));
        if(!envelope.is_object() || !envelope.contains("key") || !envelope["key"].is_string())
            throw std::runtime_error("Registry activation envelope is invalid");
        m_seenActivationRevision=revision;
        if(ClientNotification)ClientNotification(source,"registry.action",envelope["key"].get<std::string>(),envelope.dump());
    } else if(name==FName(TEXT("OnRep_PersistentStateRevision"),FNAME_Add)) {
        const auto revision=static_cast<uint32_t>(ReadInt(source,contract.PersistentStateRevision));
        if(!revision || revision<=m_seenPersistentRevision)return;
        const auto envelope=nlohmann::json::parse(ReadString(source,contract.PersistentStateEnvelope));
        if(!envelope.is_object())throw std::runtime_error("Registry persistent-state envelope is invalid");
        m_seenPersistentRevision=revision;if(PersistentState)PersistentState(source,"world.state","all",envelope.dump());
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

void RegistryBridge::RecordDiagnostic(const std::string& channel,const std::string& entity,int64_t revision,
    const std::string& result,const std::string& detail) const {
    try {
        const auto path=HostServices::ExportsDirectory()/"NetworkBridgeDiagnostics.jsonl";
        std::filesystem::create_directories(path.parent_path());std::ofstream stream(path,std::ios::app|std::ios::binary);
        stream<<nlohmann::json{{"channel",channel},{"entity",entity},{"revision",revision},{"result",result},{"detail",detail.substr(0,512)}}.dump()<<'\n';
    }catch(...) {}
}

void RegistryBridge::Start() {
    if(m_started)return;
    try{LoadAuthorityActions();}catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("Registry authority manifest unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");
    options.HookName=TEXT("RegistryBridgeWorldStarting");
    m_worldStarting=Hook::RegisterInitGameStatePreCallback([this](Hook::TCallbackIterationData<void>&,AGameModeBase*){ResetWorld();},options);
    options.HookName=TEXT("RegistryBridgeWorldReady");
    m_worldReady=Hook::RegisterInitGameStatePostCallback([this](Hook::TCallbackIterationData<void>&,AGameModeBase* mode){
        m_pendingMode=mode;m_retryElapsed=m_retryInterval=0.0f;
        try{Attach(mode);}catch(const std::exception& error){m_pendingMode=nullptr;PS::Log<LogLevel::Warning>(STR("Registry bridge unavailable: {}.\n"),PS::ToWideSafe(error.what()));}
    },options);
    options.HookName=TEXT("RegistryBridgeDeferredAttach");
    m_retryTick=Hook::RegisterEngineTickPostCallback([this](Hook::TCallbackIterationData<void>&,UEngine*,float deltaSeconds,bool){RetryAttach(deltaSeconds);},options);
    options.HookName=TEXT("RegistryAuthorityAction");
    m_authorityPre=Hook::RegisterProcessEventPreCallback([this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void* parameters){
        try{ObserveAuthorityPre(source,function,parameters);}catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("Registry authority precheck rejected: {}.\n"),PS::ToWideSafe(error.what()));}
    },options);
    options.HookName=TEXT("RegistryBridgeRepNotify");
    m_processEvent=Hook::RegisterProcessEventPostCallback([this](Hook::TCallbackIterationData<void>&,UObject* source,UFunction* function,void* parameters){
        try{ObserveAuthorityPost(source,function);Observe(source,function);ObserveClientTransport(source,function,parameters);}catch(const std::exception& error){PS::Log<LogLevel::Warning>(STR("Registry bridge update rejected: {}.\n"),PS::ToWideSafe(error.what()));}
    },options);
    m_started=m_worldStarting!=Hook::ERROR_ID && m_worldReady!=Hook::ERROR_ID
        && m_retryTick!=Hook::ERROR_ID && m_authorityPre!=Hook::ERROR_ID && m_processEvent!=Hook::ERROR_ID;
    if(!m_started){Stop();PS::Log<LogLevel::Warning>(STR("Registry bridge hooks could not be installed.\n"));}
}

void RegistryBridge::Stop() {
    for(auto* id:{&m_worldStarting,&m_worldReady,&m_retryTick,&m_authorityPre,&m_processEvent})if(*id!=Hook::ERROR_ID){Hook::UnregisterCallback(*id);*id=Hook::ERROR_ID;}
    ResetWorld();m_started=false;
}
}
