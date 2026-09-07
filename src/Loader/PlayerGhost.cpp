#include "Loader/PlayerGhost.h"
#include "Loader/GhostScope.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Loader/AppearanceEvents.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/FWeakObjectPtr.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Hooks.hpp"
#include "Helpers/Casting.hpp"
#include "Utility/Logging.h"
#include "Utility/EngineCleanupLifetime.h"
#include "Loader/PreparedVisualEffect.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <optional>

using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::PlayerGhost {
namespace {
PS::EngineCleanupLifetime engineCleanup;
UObject* Ref(UObject* object,const TCHAR* name) {
    if(!object) return nullptr;
    auto* property=CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name));
    return property && property->GetArrayDim()==1 && property->GetElementSize()==sizeof(UObject*)
        ? property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(object)):nullptr;
}
struct Lease {
    FWeakObjectPtr object; bool owned;
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
    Material overlayLease;
    std::vector<UObject*> roots;
    ~Visual() { engineCleanup.Run([&] {
        for(auto* root:roots)if(root && root->IsRootSet())root->ClearRootSet();
    }); }
};
using VisualPtr=std::shared_ptr<Visual>;
struct MeshState { FWeakObjectPtr mesh; Material overlay; std::vector<Material> materials; VisualPtr applied; };
struct State {
    FWeakObjectPtr player,equipment,customization,cpd;
    bool preview{};
    std::array<FWeakObjectPtr,4> previewItems{};
    PreparedVisualEffect effect;
    std::unordered_map<std::string,std::weak_ptr<Visual>> palette;
    std::vector<MeshState> meshes;
};
std::unordered_map<RC::StringType,PreparedVisualEffect> itemEffects;
enum class PreviewSlot : uint8_t { Head, Body, Legs, Cape, Unknown };
std::unordered_map<RC::StringType,PreviewSlot> itemSlots;
UFunction* previewSetOutfitFunction{};
int32 previewSetOutfitCallback{};
Hook::GlobalCallbackId previewLoadMapCallback=Hook::ERROR_ID;
Hook::GlobalCallbackId previewBeginPlayCallback=Hook::ERROR_ID;
FWeakObjectPtr previewActor;
bool previewBindPending{};

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
    visual->ghost=GhostMaterials::Create(state.player.Get(),effect.value,visual->roots);
    visual->overlayLease=Pin(visual->ghost.Overlay);
    entry=visual;return visual;
}
std::vector<std::unique_ptr<State>> states;
std::mutex queueMutex;
std::array<uintptr_t,128> dirtyTokens{};
size_t dirtyCount{}; bool overflow{};
std::atomic<bool> pending{false};
bool refreshing{};
void TrackPreview(UObject* preview,const std::array<UObject*,4>& items);

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

void UnbindPreviewOutfit() {
    if(previewSetOutfitFunction && previewSetOutfitCallback)
        previewSetOutfitFunction->UnregisterHook(previewSetOutfitCallback);
    previewSetOutfitFunction=nullptr;previewSetOutfitCallback=0;
}

void ReleasePreviewLifecycle() {
    UnbindPreviewOutfit();
    if(previewLoadMapCallback!=Hook::ERROR_ID)Hook::UnregisterCallback(previewLoadMapCallback);
    if(previewBeginPlayCallback!=Hook::ERROR_ID)Hook::UnregisterCallback(previewBeginPlayCallback);
    previewLoadMapCallback=Hook::ERROR_ID;previewBeginPlayCallback=Hook::ERROR_ID;
    previewActor={};previewBindPending=false;
}

void BindPreviewOutfit() {
    previewBindPending=false;
    if(itemSlots.empty() || previewSetOutfitFunction)return;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
        TEXT("/Game/Maps/L_FrontEnd.L_FrontEnd_New_C:SetOutfit"));
    if(!function)return;
    FArrayProperty* itemsField{};
    for(auto* property:TFieldRange<FProperty>(function,EFieldIterationFlags::None))
        if(property && property->GetName()==TEXT("ItemDatas")) {
            itemsField=CastField<FArrayProperty>(property);break;
        }
    auto* itemInner=itemsField?CastField<FObjectProperty>(itemsField->GetInner()):nullptr;
    if(!itemInner || itemInner->GetElementSize()!=sizeof(UObject*))return;
    const auto id=function->RegisterPostHook(
        [itemsField,itemInner](UnrealScriptFunctionCallableContext& context,void*) {
            try {
                auto* preview=previewActor.Get();
                if(!preview)preview=Ref(context.Context,
                    TEXT("BP_PlayerCharacterPreview_C_1_SetOutfit_MERGED_RefProperty"));
                auto* locals=context.TheStack.Locals();
                auto* values=locals?itemsField->ContainerPtrToValuePtr<FScriptArray>(locals):nullptr;
                if(!preview || !values || values->Num()<0 || values->Num()>16
                    || (values->Num() && !values->GetData()))return;
                std::array<UObject*,4> slotted{};
                for(int32 i=0;i<values->Num();++i) {
                    auto* item=itemInner->GetObjectPropertyValue(
                        static_cast<uint8*>(values->GetData())+i*sizeof(UObject*));
                    if(!item)continue;
                    const auto found=itemSlots.find(item->GetPathName());
                    if(found==itemSlots.end())continue;
                    const auto slot=static_cast<size_t>(found->second);
                    if(slot<slotted.size())slotted[slot]=item;
                }
                TrackPreview(preview,slotted);
            } catch(const std::exception& error) {
                PS::Log<LogLevel::Warning>(TEXT("Character preview visual refresh skipped: {}\n"),
                    PS::ToWideSafe(error.what()));
            }
        });
    if(!id)return;
    previewSetOutfitFunction=function;previewSetOutfitCallback=id;
    if(auto* preview=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,
        TEXT("/Game/Maps/L_FrontEnd.L_FrontEnd:PersistentLevel.BP_PlayerCharacterPreview_C_1")))
        TrackPreview(preview,PreviewItems(preview));
    PS::Log<LogLevel::Verbose>(TEXT("Equipment visuals attached to the character preview outfit event.\n"));
}

void EnsurePreviewLifecycle() {
    Hook::FCallbackOptions options{};
    options.OwnerModName=TEXT("RuneSchema");
    if(previewLoadMapCallback==Hook::ERROR_ID) {
        options.HookName=TEXT("EquipmentVisualPreviewMapChange");
        previewLoadMapCallback=Hook::RegisterLoadMapPreCallback(
            [](Hook::TCallbackIterationData<bool>&,UEngine*,FWorldContext&,FURL,UPendingNetGame*,FString&) {
                UnbindPreviewOutfit();previewActor={};previewBindPending=true;
            },options);
    }
    if(previewBeginPlayCallback==Hook::ERROR_ID) {
        options.HookName=TEXT("EquipmentVisualPreviewBeginPlay");
        previewBeginPlayCallback=Hook::RegisterBeginPlayPostCallback(
            [](Hook::TCallbackIterationData<void>&,AActor* actor) {
                try {
                    static const FName previewName(TEXT("BP_PlayerCharacterPreview_C"),FNAME_Add);
                    if(!actor || !actor->GetClassPrivate()
                        || actor->GetClassPrivate()->GetNamePrivate()!=previewName)return;
                    previewActor=actor;
                    TrackPreview(actor,PreviewItems(actor));
                } catch(const std::exception& error) {
                    PS::Log<LogLevel::Warning>(TEXT("Character preview begin-play refresh skipped: {}\n"),
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
    auto call=ActorHelper::FunctionCall(mesh,TEXT("/Script/Engine.MeshComponent:SetOverlayMaterial"));
    call.Arg(TEXT("NewOverlayMaterial"),material).Invoke();
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
}
void Restore(State& state) { for(auto& saved:state.meshes)RestoreMesh(saved); }
void Refresh(State& state) {
    auto* player=state.player.Get();if(!player)return;
    state.equipment=Ref(player,state.preview?TEXT("BP_EquipmentMaterialComponent"):TEXT("PlayerEquipmentComponent"));
    state.customization=state.preview?nullptr:Ref(player,TEXT("PlayerCustomizationComponent"));
    state.cpd=state.preview?nullptr:Ref(player,TEXT("CPDManager"));
    const bool stealth=!state.preview && Stealth(player);
    if(stealth) { Restore(state);return; }
    struct Desired { UObject* mesh; VisualPtr visual; };
    std::vector<Desired> meshes;
    auto* skinned=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.SkinnedMeshComponent"));
    auto* statik=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.StaticMeshComponent"));
    const auto add=[&](UObject* mesh,const VisualPtr& visual) {
        if(!mesh || !visual)return;
        if((!skinned || !mesh->IsA(skinned)) && (!statik || !mesh->IsA(statik)))return;
        auto it=std::find_if(meshes.begin(),meshes.end(),[&](auto& value){return value.mesh==mesh;});
        if(it!=meshes.end()) { it->visual=visual;return; }
        if(meshes.size()<64)meshes.push_back({mesh,visual});
    };
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.MeshComponent"));
    const auto collect=[&](UObject* actor,const VisualPtr& visual) {
        if(!actor || !type || !visual)return;
        auto call=ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
        call.Arg(TEXT("ComponentClass"),type).Invoke();TArray<UObject*> values;
        call.MoveResult(&values,sizeof(values));for(auto* mesh:values)add(mesh,visual);
    };
    if(!state.effect.value.empty()) {
        auto visual=MakeVisual(state,state.effect);
        if(PlayerMeshOnly(state.effect.value)) { add(Ref(player,TEXT("BodyMesh")),visual);add(Ref(player,TEXT("HeadMesh")),visual); }
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
        for(size_t i=0;i<state.previewItems.size();++i)
            if(auto visual=itemVisual(state.previewItems[i].Get()))add(Ref(player,meshes[i]),visual);
    } else {
        constexpr const TCHAR* worn[][2]{
            {TEXT("CurrentHeadWearable"),TEXT("OutfitHeadMesh")},
            {TEXT("CurrentBodyWearable"),TEXT("OutfitBodyMesh")},
            {TEXT("CurrentLegsWearable"),TEXT("OutfitLegsMesh")},
            {TEXT("CurrentCapeWearable"),TEXT("OutfitCapeMesh")}};
        for(const auto& pair:worn)if(auto visual=itemVisual(Ref(state.equipment.Get(),pair[0])))
            add(Ref(state.equipment.Get(),pair[1]),visual);
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
        if(found==state.meshes.end()) { state.meshes.push_back({FWeakObjectPtr(mesh),{}, {},{}});found=std::prev(state.meshes.end()); }
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
    }
    std::erase_if(state.palette,[](auto& entry){return entry.second.expired();});
}
void TrackPreview(UObject* preview,const std::array<UObject*,4>& items) {
    if(!preview || !GhostMaterials::CanRender(preview))return;
    auto found=std::find_if(states.begin(),states.end(),[&](auto& state) {
        return state->preview && state->player.Get()==preview;
    });
    if(found==states.end()) {
        std::erase_if(states,[](auto& state){return !state->player.Get();});
        if(states.size()>=16)throw std::runtime_error("Player ghost limit reached");
        auto state=std::make_unique<State>();state->player=preview;state->preview=true;
        for(size_t i=0;i<items.size();++i)state->previewItems[i]=items[i];
        AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty);
        try { Refresh(*state);states.push_back(std::move(state)); }
        catch(...) {
            try { Restore(*state); } catch(...) {}
            if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
            throw;
        }
    } else {
        for(size_t i=0;i<items.size();++i)(*found)->previewItems[i]=items[i];
        Refresh(**found);
    }
}
}
bool Apply(UObject* player,const nlohmann::json& effect) {
    if(!GhostMaterials::CanRender(player))return false;
    auto found=std::find_if(states.begin(),states.end(),[&](auto& s){return s->player.Get()==player;});
    if(found!=states.end()) { (*found)->effect=PreparedVisualEffect(effect);Refresh(**found);return true; }
    std::erase_if(states,[](auto& s){return !s->player.Get();});
    if(states.size()>=16)throw std::runtime_error("Player ghost limit reached");
    auto state=std::make_unique<State>();state->player=player;state->effect=PreparedVisualEffect(effect);
    AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty);
    try {
        Refresh(*state);states.push_back(std::move(state));return true;
    } catch(...) {
        try { Restore(*state); } catch(...) {}
        if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
        throw;
    }
}
bool HasItemRules() { return !itemEffects.empty(); }
void TrackEquipment(UObject* player) {
    if(!player || itemEffects.empty())return;
    for(auto& state:states)if(state->player.Get()==player) { Refresh(*state);return; }
    Apply(player,nlohmann::json::object());
}
void SetItemEffect(UObject* item,const nlohmann::json& value,std::string_view sourceHint) {
    auto* equipmentType=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.EquipmentData"));
    if(!item || !equipmentType || !item->IsA(equipmentType))
        throw std::runtime_error("$VisualEffect requires an EquipmentData asset (armor, cape or held equipment)");
    auto effect=value;
    if(!effect.is_null()) {
        if(effect.contains("Target"))throw std::runtime_error("Equipment $VisualEffect targets its own mesh; omit Target");
        effect=SpawnRuntime::ValidateVisualEffect(effect);
        if(itemEffects.size()>=256 && !itemEffects.contains(item->GetPathName()))
            throw std::runtime_error("Equipment ghost rule limit reached (256)");
        const auto slot=InferPreviewSlot(sourceHint.empty()?RC::to_string(item->GetPathName()):sourceHint);
        itemEffects.insert_or_assign(item->GetPathName(),PreparedVisualEffect(std::move(effect)));
        if(slot!=PreviewSlot::Unknown && !IsDedicatedProcess()) {
            itemSlots.insert_or_assign(item->GetPathName(),slot);
            EnsurePreviewLifecycle();previewBindPending=true;
        } else itemSlots.erase(item->GetPathName());
    } else { itemEffects.erase(item->GetPathName());itemSlots.erase(item->GetPathName()); }
    if(itemSlots.empty())ReleasePreviewLifecycle();
    std::lock_guard lock(queueMutex);overflow=true;pending.store(true,std::memory_order_release);
}
void Flush() {
    if(previewBindPending)BindPreviewOutfit();
    if(refreshing || !pending.exchange(false,std::memory_order_acq_rel))return;
    std::array<uintptr_t,128> tokens{};size_t count;bool all;
    { std::lock_guard lock(queueMutex);tokens=dirtyTokens;count=dirtyCount;all=overflow;dirtyCount=0;overflow=false; }
    refreshing=true;
    struct RefreshGuard { ~RefreshGuard() { refreshing=false; } } guard;
    std::erase_if(states,[](auto& state){return !state->player.Get();});
    if(states.empty())AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
    for(auto& state:states) {
        const auto matches=[&](const FWeakObjectPtr& ref) {
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
void Clear() {
    AppearanceEvents::Unsubscribe(AppearanceEvents::Consumer::Ghost);
    ReleasePreviewLifecycle();
    states.clear();materialPool.clear();itemSlots.clear();
    std::lock_guard lock(queueMutex);dirtyCount=0;overflow=false;pending.store(false);
}
}
