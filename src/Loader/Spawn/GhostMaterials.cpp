#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Utility/Logging.h"
#include <unordered_set>
#include <stdexcept>
using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::GhostMaterials {
bool CanRender(UObject* context) {
    if(!context || !context->GetWorld())return false;
    auto* library=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__KismetSystemLibrary"));
    if(!library)return false;
    auto call=ActorHelper::FunctionCall(library,TEXT("/Script/Engine.KismetSystemLibrary:IsDedicatedServer"));
    call.Arg(TEXT("WorldContextObject"),context).Invoke();
    return !call.Result<bool>();
}
Set Create(UObject* actor, const nlohmann::json& visualEffect, std::vector<UObject*>& roots) {
            constexpr auto ghostMaterialPath =
                "/Game/Materials/Character/M_VFX_Ghost_Overlay."
                "M_VFX_Ghost_Overlay";
            const bool overlayEnabled=visualEffect.value("Overlay",true);
            auto* ghostMaterial = overlayEnabled ? ActorHelper::ResolveObject(
                ActorHelper::NormalizeObjectPath(RC::to_generic_string(ghostMaterialPath))) : nullptr;
            if (overlayEnabled && !ghostMaterial)
                throw std::runtime_error("baked ghost overlay material was unavailable");

            const bool hasMain = visualEffect.contains("MainColor");
            const bool hasSecondary = visualEffect.contains("SecondaryColor");
            const auto createTintedMaterial = [&](UObject* parent,
                const TCHAR* mainParameter, const TCHAR* secondaryParameter) {
                auto* library = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                    nullptr, nullptr, TEXT("/Script/Engine.Default__KismetMaterialLibrary"));
                if (!library)
                    throw std::runtime_error("KismetMaterialLibrary default object was unavailable");
                auto create = ActorHelper::FunctionCall(library,
                    TEXT("/Script/Engine.KismetMaterialLibrary:CreateDynamicMaterialInstance"));
                create.Arg(TEXT("WorldContextObject"), actor)
                    .Arg(TEXT("Parent"), parent).Invoke();
                auto* dynamic = create.Result<UObject*>();
                if (!dynamic)
                    throw std::runtime_error("ghost dynamic material instance was not created");
                dynamic->SetRootSet();
                try
                {
                    struct RuntimeLinearColor { float R; float G; float B; float A; };
                    static_assert(sizeof(RuntimeLinearColor) == 16);
                    const auto setColor = [&](const TCHAR* parameter,
                        const nlohmann::json& color) {
                        auto set = ActorHelper::FunctionCall(dynamic,
                            TEXT("/Script/Engine.MaterialInstanceDynamic:SetVectorParameterValue"));
                        set.Arg(TEXT("ParameterName"), FName(parameter, FNAME_Add))
                            .Arg(TEXT("Value"), RuntimeLinearColor{
                                color.value("R", 1.0f), color.value("G", 1.0f),
                                color.value("B", 1.0f), color.value("A", 1.0f)}).Invoke();
                    };
                    if (hasMain) setColor(mainParameter, visualEffect.at("MainColor"));
                    if (hasSecondary) setColor(secondaryParameter, visualEffect.at("SecondaryColor"));
                    roots.push_back(dynamic);
                }
                catch (...)
                {
                    dynamic->ClearRootSet();
                    throw;
                }
                return dynamic;
            };
            UObject* overlay = ghostMaterial;
            if (overlayEnabled && (hasMain || hasSecondary))
                overlay = createTintedMaterial(ghostMaterial,
                    TEXT("MainColor_Top"), TEXT("SecondaryColor_Top"));

            UObject* body = nullptr;
            if (visualEffect.value("BodyMaterial", false))
            {
                auto* parent = ActorHelper::ResolveObject(ActorHelper::NormalizeObjectPath(
                    TEXT("/Game/Art/Skeleton/Shared/Materials/MI_Ghost.MI_Ghost")));
                if (!parent)
                    throw std::runtime_error("baked ghost body material was unavailable");
                body = createTintedMaterial(parent, TEXT("Color A"), TEXT("Color B"));
            }
return {overlay, body};
}
bool Apply(UObject* actor, const Set& materials, const RC::StringType& context) {
if (!actor) return false;
auto* overlay=materials.Overlay;
auto* body=materials.Body;
try {
            // Exclude WidgetComponent to avoid rendering nameplate quads.
            auto* skinnedMeshClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Engine.SkinnedMeshComponent"));
            auto* staticMeshClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Engine.StaticMeshComponent"));
            static constexpr const CharType* componentNames[]{
                TEXT("SkeletalMeshComponent"), TEXT("StaticMeshComponent"),
                TEXT("SkeletalMesh"), TEXT("BodyMesh"), TEXT("FaceMesh"),
                TEXT("HairZone1Mesh"), TEXT("FacialHairZone1Mesh"),
                TEXT("InvisHairMesh"), TEXT("DefaultOutfitBody"),
                TEXT("DefaultOutfitLegs"), TEXT("OutfitHelmet"),
                TEXT("OutfitBody"), TEXT("OutfitLegs"), TEXT("OutfitCape"),
                TEXT("OutfitTrinket")};
            std::unordered_set<UObject*> visited;
            std::size_t appliedCount{};
            const auto applyToComponent = [&](UObject* component) {
                if (!component || !visited.insert(component).second) return;
                if ((!skinnedMeshClass || !component->IsA(skinnedMeshClass))
                    && (!staticMeshClass || !component->IsA(staticMeshClass))) return;
                if (!PropertyHelper::GetPropertyByName(
                        component->GetClassPrivate(), TEXT("OverlayMaterial"))) return;
                if (body)
                {
                    auto count = ActorHelper::FunctionCall(component,
                        TEXT("/Script/Engine.PrimitiveComponent:GetNumMaterials"));
                    count.Invoke();
                    const auto slots = count.Result<int32>();
                    if (slots < 0 || slots > 256)
                        throw std::runtime_error("ghost mesh material count was invalid");
                    for (int32 slot = 0; slot < slots; ++slot)
                    {
                        auto set = ActorHelper::FunctionCall(component,
                            TEXT("/Script/Engine.PrimitiveComponent:SetMaterial"));
                        set.Arg(TEXT("ElementIndex"), slot).Arg(TEXT("Material"), body).Invoke();
                    }
                }
                if(overlay)ActorHelper::SetObjectRef(component, TEXT("OverlayMaterial"), overlay);
                ++appliedCount;
            };
            applyToComponent(actor);

            try
            {
                auto* meshComponentClass =
                    UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                        nullptr, nullptr, TEXT("/Script/Engine.MeshComponent"));
                if (meshComponentClass)
                {
                    auto getComponents = ActorHelper::FunctionCall(actor,
                        TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
                    getComponents.Arg(TEXT("ComponentClass"), meshComponentClass)
                        .Invoke();
                    TArray<UObject*> components;
                    getComponents.MoveResult(&components, sizeof(components));
                    for (auto* component : components)
                        applyToComponent(component);
                }
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Warning>(
                    STR("{} Ghost mesh enumeration failed safely: {}\n"),
                    context, PS::ToWideSafe(error.what()));
            }
            for (const auto* name : componentNames)
            {
                if (!PropertyHelper::GetPropertyByName(
                        actor->GetClassPrivate(), name)) continue;
                try { applyToComponent(ActorHelper::GetObjectRef(actor, name)); }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Warning>(
                        STR("{} Ghost overlay skipped component {} safely: {}\n"),
                        context, name, PS::ToWideSafe(error.what()));
                }
            }
            if (appliedCount == 0)
            {
                PS::Log<LogLevel::Warning>(
                    STR("{} requested Ghost, but no compatible mesh component was available.\n"),
                    context);
                return false;
            }
            PS::Log<LogLevel::Verbose>(
                STR("{} applied Ghost materials to {} mesh component(s).\n"),
                context, appliedCount);
            return true;
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(STR("{} Ghost visual effect failed safely: {}\n"),
                context, PS::ToWideSafe(error.what()));
            return false;
        }
}
}
