#include "Loader/PlayerGhost.h"
#include "Loader/GhostScope.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Loader/AppearanceEvents.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Hooks.hpp"
#include "Helpers/Casting.hpp"
#include "Utility/Logging.h"
#include "Utility/EngineCleanupLifetime.h"
#include "Loader/PreparedVisualEffect.h"
#include "Loader/NiagaraAttachment.h"
#include "Loader/VisualEffectLifetime.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::PlayerGhost {
namespace {
PS::EngineCleanupLifetime engineCleanup;
UObject* Ref(UObject* object,const TCHAR* name) {
    if(!object) return nullptr;
    auto* property=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name));
    return property && property->GetArrayDim()==1 && property->GetOffset_Internal()>=0
        && property->GetElementSize()>0
        && static_cast<int64_t>(property->GetOffset_Internal())+property->GetElementSize()<=object->GetClassPrivate()->GetPropertiesSize()
        ? property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(object)):nullptr;
}
struct Lease {
    PS::WeakObjectHandle object; bool owned;
    explicit Lease(UObject* value):object(value),owned(!value->IsRootSet()) { if(owned)value->SetRootSet(); }
    ~Lease() { engineCleanup.Run([&] {
        if(owned)if(auto* value=object.Get())if(value->IsRootSet())value->ClearRootSet();
    }); }
};
using Material=std::shared_ptr<Lease>;
std::unordered_map<UObject*,std::weak_ptr<Lease>> materialPool;
Material Pin(UObject* value) {
    if(!value)return {};
    auto& entry=materialPool[value];
    if(auto lease=entry.lock())if(lease->object.Get()==value)return lease;
    auto lease=std::make_shared<Lease>(value);entry=lease;return lease;
}
UObject* Get(const Material& material) { return material?material->object.Get():nullptr; }
struct Visual {
    GhostMaterials::Set ghost{};
    nlohmann::json value;
    Material overlayLease;
    std::vector<PS::WeakObjectHandle> roots;
    ~Visual() { engineCleanup.Run([&] {
        for(const auto& ref:roots)if(auto* root=ref.Get())if(root->IsRootSet())root->ClearRootSet();
    }); }
};
using VisualPtr=std::shared_ptr<Visual>;
struct MeshState { PS::WeakObjectHandle mesh; Material overlay; std::vector<Material> materials; std::vector<PS::WeakObjectHandle> niagara; VisualPtr applied; };
struct State {
    PS::WeakObjectHandle player,equipment,customization,cpd;
    bool preview{};
    std::string previewDiagnostic;
    PreparedVisualEffect effect;
    PreparedVisualEffect temporaryEffect;
    double temporarySeconds = 0.0;
    std::string temporaryKey;
    bool temporaryConsumed = false;
    PreparedVisualEffect armorTest;
    uint8_t armorTestSlots{0x0f};
    size_t armorTestMeshes{};
    std::unordered_map<std::string,std::weak_ptr<Visual>> palette;
    std::vector<MeshState> meshes;
};
std::unordered_map<RC::StringType,PreparedVisualEffect> itemEffects;
enum class PreviewSlot : uint8_t { Head, Body, Legs, Cape, Unknown };
std::unordered_map<RC::StringType,PreviewSlot> itemSlots;
Hook::GlobalCallbackId previewBeginPlayCallback=Hook::ERROR_ID;
Hook::GlobalCallbackId previewTickCallback=Hook::ERROR_ID;
PS::WeakObjectHandle previewActor;
bool previewHookWarning{};

PreviewSlot InferPreviewSlot(std::string_view hint) {
    std::string path(hint);
    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    if(path.find("/head/")!=std::string::npos || path.find("_head")!=std::string::npos)return PreviewSlot::Head;
    if(path.find("/body/")!=std::string::npos || path.find("_body")!=std::string::npos)return PreviewSlot::Body;
    if(path.find("/legs/")!=std::string::npos || path.find("_legs")!=std::string::npos)return PreviewSlot::Legs;
    if(path.find("/cape/")!=std::string::npos || path.find("_cape")!=std::string::npos)return PreviewSlot::Cape;
    return PreviewSlot::Unknown;
}
VisualPtr MakeVisual(State& state,const PreparedVisualEffect& effect) {
    auto& entry=state.palette[effect.key];
    if(auto visual=entry.lock())return visual;
    auto visual=std::make_shared<Visual>();
    visual->value=effect.value;
    if(effect.value.value("Type",std::string("Ghost"))=="Ghost") {
        visual->ghost=GhostMaterials::Create(state.player.Get(),effect.value,visual->roots);
        visual->overlayLease=Pin(visual->ghost.Overlay);
    }
    entry=visual;return visual;
}
std::vector<std::unique_ptr<State>> states;
std::mutex queueMutex;
std::array<uintptr_t,128> dirtyTokens{};
size_t dirtyCount{}; bool overflow{};
std::atomic<bool> pending{false};
bool refreshing{};
void TrackPreview(UObject* preview);

std::array<UObject*,4> PreviewItems(UObject* preview) {
    constexpr const TCHAR* equipment[]{TEXT("HeadEquipment"),TEXT("BodyEquipment"),
        TEXT("LegsEquipment"),TEXT("CapeEquipment")};
    std::array<UObject*,4> items{};
    for(size_t i=0;i<items.size();++i)items[i]=Ref(Ref(preview,equipment[i]),TEXT("ItemData"));
    return items;
}

bool IsDedicatedProcess() {
    static const bool dedicated=[] {
        std::array<wchar_t,32768> path{};
        const auto length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        return length && length<path.size()
            && std::wstring_view(path.data(),length).ends_with(L"RSDragonwildsServer-Win64-Shipping.exe");
    }();
    return dedicated;
}

void ReleasePreviewLifecycle() {
    if(previewBeginPlayCallback!=Hook::ERROR_ID)Hook::UnregisterCallback(previewBeginPlayCallback);
    if(previewTickCallback!=Hook::ERROR_ID)Hook::UnregisterCallback(previewTickCallback);
    previewBeginPlayCallback=Hook::ERROR_ID;
    previewTickCallback=Hook::ERROR_ID;
    previewActor.Reset();
}

bool IsMenuPreview(UObject* actor) {
    if (!actor || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return false;
    const bool previewClass=actor->GetClassPrivate()
        && actor->GetClassPrivate()->GetNamePrivate()==FName(TEXT("BP_PlayerCharacterPreview_C"),FNAME_Add);
    return previewClass
        && actor->GetPathName().find(TEXT("/Game/Maps/L_FrontEnd."))!=RC::StringType::npos;
}

void EnsurePreviewLifecycle() {
    Hook::FCallbackOptions options{};
    options.OwnerModName=TEXT("RuneSchema");
    if(previewBeginPlayCallback==Hook::ERROR_ID) {
        options.HookName=TEXT("EquipmentVisualPreviewBeginPlay");
        previewBeginPlayCallback=Hook::RegisterBeginPlayPostCallback(
            [](Hook::TCallbackIterationData<void>&,AActor* actor) {
                try {
                    if(!IsMenuPreview(actor))return;
                    previewActor=PS::WeakObject(actor);
                    TrackPreview(actor);
                } catch(const std::exception& error) {
                    PS::Log<LogLevel::Warning>(TEXT("Character preview begin-play refresh skipped: {}\n"),
                        PS::ToWideSafe(error.what()));
                }
            },options);
    }
    if(previewTickCallback==Hook::ERROR_ID) {
        options.HookName=TEXT("EquipmentVisualPreviewTick");
        previewTickCallback=Hook::RegisterEngineTickPostCallback(
            [](Hook::TCallbackIterationData<void>&,UEngine*,float,bool) {
                static unsigned cadence{};
                // WinGDK currently exposes the verified wearable return but
                // not every Steam-only appearance callback. Keep a bounded,
                // menu-only refresh alive after discovery so later preview
                // equipment changes cannot be missed. This callback exists
                // only while an item preview effect is registered.
                if(++cadence%15!=0)return;
                try {
                auto* preview=previewActor.Get();
                if(!preview)preview=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
                    TEXT("/Game/Maps/L_FrontEnd.L_FrontEnd:PersistentLevel.BP_PlayerCharacterPreview_C_1"));
                if(preview) {
                    previewActor=PS::WeakObject(preview);
                    TrackPreview(preview);
                }
                } catch(const std::exception& error) {
                        if(AppearanceEvents::Unavailable() && previewHookWarning)return;
                        if(AppearanceEvents::Unavailable())previewHookWarning=true;
                        PS::Log<LogLevel::Warning>(TEXT("Character preview visual retry skipped: {}\n"),
                            PS::ToWideSafe(error.what()));
                }
            },options);
    }
}
// Declare after owners so this disables engine cleanup before they destruct.
PS::StopStaticEngineCleanup stopStaticEngineCleanup{engineCleanup};
void Dirty(unsigned,uintptr_t token,uint32_t) {
    std::lock_guard lock(queueMutex);
    if(std::find(dirtyTokens.begin(),dirtyTokens.begin()+dirtyCount,token)!=dirtyTokens.begin()+dirtyCount)return;
    if(dirtyCount<dirtyTokens.size())dirtyTokens[dirtyCount++]=token;else overflow=true;
    pending.store(true,std::memory_order_release);
}
bool Stealth(UObject* player) {
    auto* component=Ref(player,TEXT("GameplayEffectsComponent"));
    if(!component)return false;
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.ShadowVeilGameplayEffectInstance"));
    if(!type)return false;
    for(auto* name:{TEXT("ReplicatedInstances"),TEXT("NonReplicatedInstances")}) {
        auto* field=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(component->GetClassPrivate(),name));
        auto* inner=field?CastField<FObjectProperty>(field->GetInner()):nullptr;
        if(!inner || inner->GetElementSize()!=sizeof(UObject*))continue;
        auto* array=field->ContainerPtrToValuePtr<FScriptArray>(component);
        if(array->Num()<0 || array->Num()>512)throw std::runtime_error("Invalid gameplay effect array");
        if(array->Num() && !array->GetData())throw std::runtime_error("Null gameplay effect array");
        for(int32 i=0;i<array->Num();++i) {
            auto* effect=inner->GetObjectPropertyValue(static_cast<uint8*>(array->GetData())+i*sizeof(UObject*));
            if(effect && effect->IsA(type))return true;
        }
    }
    return false;
}
class MaterialAccess {
    UObject* mesh;
    std::optional<ActorHelper::FunctionCall> reader,writer;
public:
    explicit MaterialAccess(UObject* value):mesh(value) {}
    UObject* Read(int32 slot) {
        if(!reader)reader.emplace(mesh,TEXT("/Script/Engine.PrimitiveComponent:GetMaterial"));
        reader->Arg(TEXT("ElementIndex"),slot).Invoke();
        return reader->Result<UObject*>();
    }
    void Write(int32 slot,UObject* material) {
        if(!writer)writer.emplace(mesh,TEXT("/Script/Engine.PrimitiveComponent:SetMaterial"));
        writer->Arg(TEXT("ElementIndex"),slot).Arg(TEXT("Material"),material).Invoke();
    }
};
void SetOverlayMaterial(UObject* mesh,UObject* material) {
    // SetOverlayMaterial can resolve to a reflected UFunction with no callable
    // native implementation during world teardown/re-entry. Write the live
    // object property directly, as GhostMaterials does, to avoid ProcessEvent
    // dispatching through a null function pointer.
    ActorHelper::SetObjectRef(mesh,TEXT("OverlayMaterial"),material);
}
void RestoreMesh(MeshState& saved) {
    if(!saved.applied)return;
    const auto& ghost=saved.applied->ghost;
    if(auto* mesh=saved.mesh.Get()) {
        MaterialAccess materials(mesh);
        if(ghost.Overlay && Ref(mesh,TEXT("OverlayMaterial"))==ghost.Overlay)
            SetOverlayMaterial(mesh,Get(saved.overlay));
        if(ghost.Body)for(size_t i=0;i<saved.materials.size();++i)
            if(materials.Read(static_cast<int32>(i))==ghost.Body)
                materials.Write(static_cast<int32>(i),Get(saved.materials[i]));
    }
    for(const auto& component:saved.niagara) {
        try { NiagaraAttachment::Destroy(component.Get()); }
        catch(const std::exception& error) {
            PS::Log<LogLevel::Verbose>(TEXT("Niagara cleanup skipped: {}\n"),
                PS::ToWideSafe(error.what()));
        }
    }
    saved.niagara.clear();
}
void Restore(State& state) { for(auto& saved:state.meshes)RestoreMesh(saved); }
void Refresh(State& state) {
    auto* player=state.player.Get();if(!player)return;
    state.equipment=PS::WeakObject(Ref(player,state.preview?TEXT("BP_EquipmentMaterialComponent"):TEXT("PlayerEquipmentComponent")));
    state.customization=PS::WeakObject(state.preview?nullptr:Ref(player,TEXT("PlayerCustomizationComponent")));
    state.cpd=PS::WeakObject(state.preview?nullptr:Ref(player,TEXT("CPDManager")));
    const bool stealth=!state.preview && Stealth(player);
    if(stealth) { Restore(state);return; }
    struct Desired { UObject* mesh; VisualPtr visual; };
    std::vector<Desired> meshes;
    auto* skinned=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.SkinnedMeshComponent"));
    auto* statik=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.StaticMeshComponent"));
    const auto add=[&](UObject* mesh,const VisualPtr& visual) {
        if(!mesh || !visual)return false;
        if((!skinned || !mesh->IsA(skinned)) && (!statik || !mesh->IsA(statik)))return false;
        auto it=std::find_if(meshes.begin(),meshes.end(),[&](auto& value){return value.mesh==mesh;});
        if(it!=meshes.end()) { it->visual=visual;return true; }
        if(meshes.size()<64) { meshes.push_back({mesh,visual});return true; }
        return false;
    };
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.MeshComponent"));
    const auto collect=[&](UObject* actor,const VisualPtr& visual) {
        if(!actor || !type || !visual)return;
        auto call=ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
        call.Arg(TEXT("ComponentClass"),type).Invoke();TArray<UObject*> values;
        call.MoveResult(&values,sizeof(values));for(auto* mesh:values)add(mesh,visual);
    };
    const auto& playerEffect = state.temporarySeconds > 0.0 ? state.temporaryEffect : state.effect;
    if(!playerEffect.value.empty()) {
        auto visual=MakeVisual(state,playerEffect);
        if(PlayerMeshOnly(playerEffect.value)) { add(Ref(player,TEXT("BodyMesh")),visual);add(Ref(player,TEXT("HeadMesh")),visual); }
        else {
            collect(player,visual);
            collect(Ref(state.equipment.Get(),TEXT("HeldEquipmentActorLeft")),visual);
            collect(Ref(state.equipment.Get(),TEXT("HeldEquipmentActorRight")),visual);
        }
    }
    const auto itemVisual=[&](UObject* item)->VisualPtr {
        if(!item || itemEffects.empty())return {};
        auto it=itemEffects.find(item->GetPathName());
        return it==itemEffects.end()?VisualPtr{}:MakeVisual(state,it->second);
    };
    if(state.preview) {
        constexpr const TCHAR* meshes[]{TEXT("OutfitHelmet"),TEXT("OutfitBody"),TEXT("OutfitLegs"),TEXT("OutfitCape")};
        std::string diagnostic;
        const auto items=PreviewItems(player);
        for(size_t i=0;i<items.size();++i) {
            auto* item=items[i];
            auto* mesh=Ref(player,meshes[i]);
            auto visual=itemVisual(item);
            if(visual)add(mesh,visual);
            diagnostic+=RC::to_string(meshes[i])+": item="+(item?RC::to_string(item->GetPathName()):"none")
                +", effect="+(visual?"matched":"none")+", mesh="+(mesh?"present":"none")+"; ";
        }
        if(diagnostic!=state.previewDiagnostic) {
            state.previewDiagnostic=diagnostic;
            PS::Log<LogLevel::Verbose>(TEXT("Character preview equipment: {}\n"),PS::ToWideSafe(diagnostic.c_str()));
        }
    } else {
        constexpr const TCHAR* worn[][2]{
            {TEXT("CurrentHeadWearable"),TEXT("OutfitHeadMesh")},
            {TEXT("CurrentBodyWearable"),TEXT("OutfitBodyMesh")},
            {TEXT("CurrentLegsWearable"),TEXT("OutfitLegsMesh")},
            {TEXT("CurrentCapeWearable"),TEXT("OutfitCapeMesh")}};
        state.armorTestMeshes=0;
        const auto armorVisual=state.armorTest.value.empty()?VisualPtr{}:MakeVisual(state,state.armorTest);
        for(size_t slot=0;slot<std::size(worn);++slot) {
            const auto& pair=worn[slot];
            auto* item=Ref(state.equipment.Get(),pair[0]);
            auto* mesh=Ref(state.equipment.Get(),pair[1]);
            if(item && armorVisual && (state.armorTestSlots&(1u<<slot))) {
                if(add(mesh,armorVisual))++state.armorTestMeshes;
            } else if(auto visual=itemVisual(item))add(mesh,visual);
        }
        for(auto* name:{TEXT("HeldEquipmentActorLeft"),TEXT("HeldEquipmentActorRight")}) {
            auto* held=Ref(state.equipment.Get(),name);
            if(auto visual=itemVisual(Ref(held,TEXT("HeldEquipmentData"))))collect(held,visual);
        }
    }
    for(auto it=state.meshes.begin();it!=state.meshes.end();) {
        auto* mesh=it->mesh.Get();
        if(!mesh || std::none_of(meshes.begin(),meshes.end(),[&](auto& value){return value.mesh==mesh;})) {
            if(mesh)RestoreMesh(*it);
            it=state.meshes.erase(it);
        } else ++it;
    }
    for(const auto& desired:meshes) {
        auto* mesh=desired.mesh;
        const auto& ghost=desired.visual->ghost;
        auto found=std::find_if(state.meshes.begin(),state.meshes.end(),[&](auto& s){return s.mesh.Get()==mesh;});
        if(found==state.meshes.end()) { state.meshes.push_back({PS::WeakObject(mesh),{}, {},{},{}});found=std::prev(state.meshes.end()); }
        auto& saved=*found;
        if(saved.applied!=desired.visual) {
            RestoreMesh(saved);
            saved.overlay={};saved.materials.clear();saved.applied=desired.visual;
        }
        auto* overlay=Ref(mesh,TEXT("OverlayMaterial"));
        if(ghost.Overlay && overlay!=ghost.Overlay)saved.overlay=Pin(overlay);
        if(ghost.Body) {
            auto call=ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.PrimitiveComponent:GetNumMaterials"));call.Invoke();
            auto slots=call.Result<int32>();if(slots<0 || slots>32)throw std::runtime_error("Player mesh material count out of range");
            saved.materials.resize(slots);
            MaterialAccess materials(mesh);
            for(int32 i=0;i<slots;++i) {
                auto* current=materials.Read(i);
                if(current!=ghost.Body) { saved.materials[i]=Pin(current);materials.Write(i,ghost.Body); }
            }
        }
        if(ghost.Overlay && overlay!=ghost.Overlay)SetOverlayMaterial(mesh,ghost.Overlay);
        std::erase_if(saved.niagara,[](const auto& value){return !value.Get();});
        if(saved.niagara.empty()
            && desired.visual->value.value("Type",std::string("Ghost"))=="Niagara") {
            if(auto* component=NiagaraAttachment::Attach(state.player.Get(),mesh,desired.visual->value))
                saved.niagara.emplace_back(PS::WeakObject(component));
        }
    }
    std::erase_if(state.palette,[](auto& entry){return entry.second.expired();});
}
void TrackPreview(UObject* preview) {
    if(!preview || !GhostMaterials::CanRender(preview))return;
    auto found=std::find_if(states.begin(),states.end(),[&](auto& state) {
        return state->preview && state->player.Get()==preview;
    });
    if(found==states.end()) {
        std::erase_if(states,[](auto& state){return !state->player.Get();});
        if(states.size()>=16)throw std::runtime_error("Player ghost limit reached");
        auto state=std::make_unique<State>();state->player=PS::WeakObject(preview);state->preview=true;
        try {
            AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty);
        }
        catch (const std::exception& error) {
            if (!previewHookWarning) {
                previewHookWarning=true;
                PS::Log<LogLevel::Warning>(TEXT("Character preview native refresh unavailable; using the bounded menu fallback: {}\n"),
                    PS::ToWideSafe(error.what()));
            }
        }
        try {
            Refresh(*state);
            PS::Log<LogLevel::Verbose>(TEXT("Character preview visuals: {} ({} affected meshes).\n"),
                preview->GetPathName(), state->meshes.size());
            states.push_back(std::move(state));
        }
        catch(...) {
            try { Restore(*state); } catch(...) {}
            if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
            throw;
        }
    } else {
        Refresh(**found);
    }
}
}
bool Apply(UObject* player,const nlohmann::json& effect) {
    if(effect.value("Type",std::string("Ghost"))=="Niagara"
        && !NiagaraAttachment::CanRenderLocally())return true;
    if(!GhostMaterials::CanRender(player))return false;
    auto found=std::find_if(states.begin(),states.end(),[&](auto& s){return s->player.Get()==player;});
    if(found!=states.end()) { (*found)->effect=PreparedVisualEffect(effect);Refresh(**found);return true; }
    std::erase_if(states,[](auto& s){return !s->player.Get();});
    if(states.size()>=16)throw std::runtime_error("Player ghost limit reached");
    auto state=std::make_unique<State>();state->player=PS::WeakObject(player);state->effect=PreparedVisualEffect(effect);
    AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty);
    try {
        Refresh(*state);states.push_back(std::move(state));return true;
    } catch(...) {
        try { Restore(*state); } catch(...) {}
        if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
        throw;
    }
}
bool IsDead(UObject* player) {
    if (!player) return true;
    auto* damage = Ref(player, TEXT("BP_Components_PlayerDamage"));
    auto* fatal = damage ? CastField<FStructProperty>(PropertyHelper::GetPropertyByName(damage->GetClassPrivate(), TEXT("FatalDamageInfo"))) : nullptr;
    auto* field = fatal && fatal->GetStruct() ? CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(fatal->GetStruct().Get(), TEXT("bIsSet"))) : nullptr;
    return !field || field->GetPropertyValue(field->ContainerPtrToValuePtr<void>(fatal->ContainerPtrToValuePtr<void>(damage)));
}
bool ApplyTimed(UObject* player, const nlohmann::json& effect, double seconds, bool restart) {
    if (!std::isfinite(seconds) || seconds <= 0.0 || seconds > 300.0 || IsDead(player)) return false;
    auto found = std::find_if(states.begin(),states.end(),[&](const auto& state){return state->player.Get()==player;});
    if (found == states.end()) {
        if (!Apply(player, nlohmann::json::object())) return false;
        found = std::find_if(states.begin(),states.end(),[&](const auto& state){return state->player.Get()==player;});
    }
    if (found == states.end()) return false;
    auto& state = **found;
    PreparedVisualEffect prepared(effect);
    if (!restart && state.temporaryConsumed && state.temporaryKey==prepared.key) return true;
    if (!restart && state.temporarySeconds > 0.0 && state.temporaryKey==prepared.key) return true;
    state.temporaryEffect = std::move(prepared);
    state.temporaryKey = state.temporaryEffect.key;
    state.temporaryConsumed = true;
    state.temporarySeconds = seconds;
    try { Refresh(state); }
    catch (...) { state.temporarySeconds=0.0; state.temporaryEffect={}; Restore(state); throw; }
    return true;
}
void TickTimed(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) return;
    for (auto& state : states) {
        auto* player = state->player.Get();
        bool refresh=false;
        if(state->temporarySeconds>0.0) {
            state->temporarySeconds = !player || IsDead(player) ? 0.0 : std::max(0.0, state->temporarySeconds-deltaSeconds);
            if(state->temporarySeconds==0.0)refresh=true;
        }
        if (refresh) {
            state->temporaryEffect = {};
            try { if (player) Refresh(*state); }
            catch (const std::exception& error) { PS::Log<LogLevel::Warning>(TEXT("Timed appearance cleanup failed: {}\n"),PS::ToWideSafe(error.what())); }
        }
    }
}
bool HasItemRules() { return !itemEffects.empty(); }
void TrackEquipment(UObject* player) {
    if(!player || itemEffects.empty())return;
    for(auto& state:states)if(state->player.Get()==player) { Refresh(*state);return; }
    Apply(player,nlohmann::json::object());
}
size_t ApplyArmorTest(UObject* player,const nlohmann::json& value,uint8_t slots) {
    if(!player || !GhostMaterials::CanRender(player))return 0;
    if(!slots || (slots&0xf0))throw std::runtime_error("Armor test slot mask is invalid");
    auto effect=SpawnRuntime::ValidateVisualEffect(value);
    if(effect.value("Type",std::string{})!="Niagara")
        throw std::runtime_error("Armor test requires a Niagara visual effect");
    if(effect.contains("Target"))effect.erase("Target");
    auto found=std::find_if(states.begin(),states.end(),[&](auto& state) {
        return !state->preview && state->player.Get()==player;
    });
    if(found==states.end()) {
        std::erase_if(states,[](auto& state){return !state->player.Get();});
        if(states.size()>=16)throw std::runtime_error("Player visual state limit reached");
        auto state=std::make_unique<State>();
        state->player=PS::WeakObject(player);state->armorTest=PreparedVisualEffect(std::move(effect));
        state->armorTestSlots=slots;
        AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty);
        try {
            Refresh(*state);
            const auto count=state->armorTestMeshes;
            states.push_back(std::move(state));return count;
        } catch(...) {
            try { Restore(*state); } catch(...) {}
            if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
            throw;
        }
    }
    (*found)->armorTest=PreparedVisualEffect(std::move(effect));
    (*found)->armorTestSlots=slots;
    Refresh(**found);return (*found)->armorTestMeshes;
}
void ClearArmorTest() {
    for(auto& state:states)if(!state->preview && !state->armorTest.value.empty()) {
        state->armorTest={};state->armorTestMeshes=0;
        if(state->player.Get())Refresh(*state);
        else Restore(*state);
    }
}
void SetItemEffect(UObject* item,const nlohmann::json& value,std::string_view sourceHint) {
    auto* equipmentType=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.EquipmentData"));
    if(!item || !equipmentType || !item->IsA(equipmentType))
        throw std::runtime_error("$VisualEffect requires an EquipmentData asset (armor, cape or held equipment)");
    auto effect=value;
    if(!effect.is_null()) {
        effect=SpawnRuntime::ValidateVisualEffect(effect);
        const auto lifetime=ParseVisualEffectLifetime(effect,"Equip",{"Equip"});
        if(lifetime.IsFinite())
            throw std::runtime_error("Equipment $VisualEffect.DurationSeconds supports only 'INFINITE'");
        if(effect.contains("Target")) {
            if(effect.at("Target")!="ItemMesh")
                throw std::runtime_error("Equipment $VisualEffect.Target must be ItemMesh or omitted");
            effect.erase("Target");
        }
        if(itemEffects.size()>=256 && !itemEffects.contains(item->GetPathName()))
            throw std::runtime_error("Equipment ghost rule limit reached (256)");
        const auto slot=InferPreviewSlot(sourceHint.empty()?RC::to_string(item->GetPathName()):sourceHint);
        effect=VisualEffectStyle(std::move(effect));
        itemEffects.insert_or_assign(item->GetPathName(),PreparedVisualEffect(std::move(effect)));
        if(slot!=PreviewSlot::Unknown && !IsDedicatedProcess()) {
            itemSlots.insert_or_assign(item->GetPathName(),slot);
            EnsurePreviewLifecycle();
        } else itemSlots.erase(item->GetPathName());
    } else { itemEffects.erase(item->GetPathName());itemSlots.erase(item->GetPathName()); }
    if(itemSlots.empty())ReleasePreviewLifecycle();
    std::lock_guard lock(queueMutex);overflow=true;pending.store(true,std::memory_order_release);
}
void Flush() {
    if(refreshing || !pending.exchange(false,std::memory_order_acq_rel))return;
    std::array<uintptr_t,128> tokens{};size_t count;bool all;
    { std::lock_guard lock(queueMutex);tokens=dirtyTokens;count=dirtyCount;all=overflow;dirtyCount=0;overflow=false; }
    refreshing=true;
    struct RefreshGuard { ~RefreshGuard() { refreshing=false; } } guard;
    std::erase_if(states,[](auto& state){return !state->player.Get();});
    if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
    for(auto& state:states) {
        const auto matches=[&](const PS::WeakObjectHandle& ref) {
            const auto token=reinterpret_cast<uintptr_t>(ref.Get());
            return token && std::find(tokens.begin(),tokens.begin()+count,token)!=tokens.begin()+count;
        };
        if(!all && !matches(state->player) && !matches(state->equipment)
            && !matches(state->customization) && !matches(state->cpd))continue;
        try { Refresh(*state); }
        catch(const std::exception& error) { PS::Log<LogLevel::Warning>(TEXT("Player ghost refresh skipped: {}\n"),PS::ToWideSafe(error.what())); }
    }
    std::erase_if(materialPool,[](auto& entry){return entry.second.expired();});
}
void ClearWorld() {
    AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
    previewActor.Reset();
    for(auto& state:states)Restore(*state);
    states.clear();materialPool.clear();
    std::lock_guard lock(queueMutex);dirtyCount=0;overflow=false;pending.store(false);
}
void Clear() {
    ClearWorld();
    ReleasePreviewLifecycle();
    itemEffects.clear();
    itemSlots.clear();
}
}
