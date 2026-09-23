#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/DefinitionRegistry.h"
#include "Loader/PlayerGhost.h"
#include "Loader/GhostScope.h"
#include "Loader/VisualPolicy.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Utility/Logging.h"
#include "Loader/NiagaraAttachment.h"
#include "Loader/TimeOfDayRuntime.h"
#include <array>
#include <cmath>
#include "Loader/Spawn/RuntimeSupport.h"
#include <stdexcept>
#include <string_view>
using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::SpawnRuntime {
    nlohmann::json ValidateVisualEffect(const nlohmann::json& input)
    {
        const auto value=DefinitionRegistry::Visual(input);
        if (!value.is_object())
            throw std::runtime_error("VisualEffect must be an object");
        if (!value.contains("Type") || !value.at("Type").is_string())
            throw std::runtime_error("VisualEffect.Type must be 'Ghost' or 'Niagara'");
        const auto type=value.at("Type").get<std::string>();
        if (type!="Ghost" && type!="Niagara")
            throw std::runtime_error("VisualEffect.Type must be 'Ghost' or 'Niagara'");
        if(type=="Niagara") {
            if(value.contains("TimeOfDay")) {
                if(!value.at("TimeOfDay").is_string())throw std::runtime_error("Niagara VisualEffect.TimeOfDay must be Any, Day, or Night");
                (void)TimeOfDay::Parse(value.at("TimeOfDay").get<std::string>());
            }
            if(!value.contains("System") || !value.at("System").is_string())
                throw std::runtime_error("Niagara VisualEffect.System must be a cooked NiagaraSystem path");
            const auto system=value.at("System").get<std::string>();
            if(system.size()>1024 || system.empty() || system.front()!='/' || system.find('.')==std::string::npos)
                throw std::runtime_error("Niagara VisualEffect.System must be a canonical cooked object path");
            if(value.contains("Socket") && (!value.at("Socket").is_string() || value.at("Socket").get<std::string>().size()>128))
                throw std::runtime_error("Niagara VisualEffect.Socket must be a short string");
            if(value.contains("AutoActivate") && !value.at("AutoActivate").is_boolean())
                throw std::runtime_error("Niagara VisualEffect.AutoActivate must be a boolean");
            const auto triple=[&](const char* field,const std::array<const char*,3>& keys,double limit) {
                if(!value.contains(field))return;
                const auto& object=value.at(field);
                if(!object.is_object() || object.size()!=3)
                    throw std::runtime_error(std::string("Niagara VisualEffect.")+field+" must contain exactly three numeric fields");
                for(const auto* key:keys) {
                    if(!object.contains(key) || !object.at(key).is_number())
                        throw std::runtime_error(std::string("Niagara VisualEffect.")+field+"."+key+" must be numeric");
                    const auto number=object.at(key).get<double>();
                    if(!std::isfinite(number) || std::abs(number)>limit)
                        throw std::runtime_error(std::string("Niagara VisualEffect.")+field+"."+key+" is outside the supported range");
                }
            };
            triple("LocationOffset",{"X","Y","Z"},100000.0);
            triple("RotationOffset",{"Pitch","Yaw","Roll"},360000.0);
            if(value.contains("Parameters")) {
                const auto& parameters=value.at("Parameters");
                if(!parameters.is_object() || parameters.size()>32)
                    throw std::runtime_error("Niagara VisualEffect.Parameters must contain at most 32 values");
                for(const auto& [name,parameter]:parameters.items()) {
                    if(name.empty() || name.size()>128 || name.rfind("User.",0)!=0)
                        throw std::runtime_error("Niagara parameter names must use the User. prefix");
                    for(unsigned char c:name)if(c<32 || c==127)
                        throw std::runtime_error("Niagara parameter names cannot contain control characters");
                    const bool color=parameter.is_object() && parameter.contains("R") && parameter.contains("G")
                        && parameter.contains("B") && parameter.contains("A");
                    const bool vector=parameter.is_object() && parameter.contains("X") && parameter.contains("Y")
                        && parameter.contains("Z") && !parameter.contains("R");
                    if(!parameter.is_boolean() && !parameter.is_number() && !color && !vector)
                        throw std::runtime_error("Niagara parameters support booleans, numbers, XYZ vectors, and RGBA colors");
                    if(color)for(const auto* key:{"R","G","B","A"})
                        if(!parameter.at(key).is_number())throw std::runtime_error("Niagara color channels must be numeric");
                    if(vector)for(const auto* key:{"X","Y","Z"})
                        if(!parameter.at(key).is_number())throw std::runtime_error("Niagara vector channels must be numeric");
                }
            }
            if(value.contains("Emitters")) {
                const auto& emitters=value.at("Emitters");
                if(!emitters.is_object() || emitters.size()>16)
                    throw std::runtime_error("Niagara VisualEffect.Emitters must contain at most 16 values");
                for(const auto& [name,enabled]:emitters.items()) {
                    if(name.empty() || name.size()>96 || !enabled.is_boolean())
                        throw std::runtime_error("Niagara emitter names require boolean enable values");
                    for(unsigned char c:name)if(c<32 || c==127)
                        throw std::runtime_error("Niagara emitter names cannot contain control characters");
                }
            }
            if(value.contains("Target")) {
                if(!value.at("Target").is_string())
                    throw std::runtime_error("Niagara VisualEffect.Target must be a string");
                const auto target=value.at("Target").get<std::string>();
                if(target!="ItemMesh" && target!="EntirePerson" && target!="PlayerMesh"
                    && target!="ActorRoot" && target!="ActorMesh")
                    throw std::runtime_error("Niagara VisualEffect.Target is unsupported");
            }
            return value;
        }

        (void)DragonWilds::PlayerMeshOnly(value);
        ValidateVisualLayers(value);

        const auto validateColor = [&](const char* field) {
            if (!value.contains(field)) return;
            const auto& color = value.at(field);
            if (!color.is_object())
                throw std::runtime_error(std::string("VisualEffect.") + field
                    + " must be an RGBA object");
            for (const auto* channel : {"R", "G", "B", "A"})
            {
                if (!color.contains(channel)) continue;
                if (!color.at(channel).is_number())
                    throw std::runtime_error(std::string("VisualEffect.") + field
                        + "." + channel + " must be a number");
                const auto component = color.at(channel).get<double>();
                const auto maximum = std::string_view(channel) == "A" ? 1.0 : 100.0;
                if (!std::isfinite(component) || component < 0.0 || component > maximum)
                    throw std::runtime_error(std::string("VisualEffect.") + field
                        + "." + channel + " is out of range");
            }
        };
        validateColor("MainColor");
        validateColor("SecondaryColor");
        return value;
    }
}
namespace DragonWilds {
    bool DragonWildsSpawnLoader::ApplyVisualEffect(UObject* actor,
        const nlohmann::json& visualEffect, const RC::StringType& context)
    {
        if (!actor || visualEffect.empty()) return false;
        if(visualEffect.contains("TimeOfDay")
            && !TimeOfDay::Allows(actor,TimeOfDay::Parse(visualEffect.at("TimeOfDay").get<std::string>())))return true;
        const auto type=visualEffect.value("Type",std::string("Ghost"));
        auto* playerClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if(playerClass && actor->IsA(playerClass)) {
            try { return PlayerGhost::Apply(actor,visualEffect); }
            catch(const std::exception& error) {
                PS::Log<LogLevel::Warning>(TEXT("Player ghost unavailable: {}\n"),PS::ToWideSafe(error.what()));
                return false;
            }
        }
        const auto signature = visualEffect.dump();
        if (const auto applied=m_visualEffectAppliedActors.find(actor);
            applied!=m_visualEffectAppliedActors.end() && applied->second.Actor.Get()==actor
                && applied->second.Signature==signature) {
            if(type!="Niagara" || applied->second.Component.Get())return true;
            m_visualEffectAppliedActors.erase(applied);
        }
        try
        {
            if(type=="Niagara") {
                if(!NiagaraAttachment::CanRenderLocally())return true;
                if(auto applied=m_visualEffectAppliedActors.find(actor);applied!=m_visualEffectAppliedActors.end()) {
                    NiagaraAttachment::Destroy(applied->second.Component.Get());
                    m_visualEffectAppliedActors.erase(applied);
                }
                auto* target=ActorHelper::GetObjectRef(actor,TEXT("RootComponent"));
                if(visualEffect.value("Target",std::string("ActorRoot"))=="ActorMesh") {
                    auto* meshClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,
                        TEXT("/Script/Engine.MeshComponent"));
                    if(!meshClass)throw std::runtime_error("Spawn Niagara mesh class was unavailable");
                    auto call=ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:K2_GetComponentsByClass"));
                    call.Arg(TEXT("ComponentClass"),meshClass).Invoke();
                    TArray<UObject*> meshes;call.MoveResult(&meshes,sizeof(meshes));
                    if(meshes.Num()>0 && meshes.GetData())target=meshes[0];
                    else throw std::runtime_error("Spawn Niagara ActorMesh target had no mesh component");
                }
                auto* component=NiagaraAttachment::Attach(actor,target,visualEffect);
                if(!component)throw std::runtime_error("Spawn Niagara component was not created");
                if(m_visualEffectAppliedActors.size()>=4096)
                    std::erase_if(m_visualEffectAppliedActors,[](auto& value){return !value.second.Actor.Get();});
                if(m_visualEffectAppliedActors.size()>=4096) {
                    NiagaraAttachment::Destroy(component);
                    throw std::runtime_error("Spawn visual instance limit reached (4096)");
                }
                m_visualEffectAppliedActors.insert_or_assign(actor,
                    AppliedVisual{PS::WeakObject(actor),PS::WeakObject(component),signature});
                return true;
            }
            if(!GhostMaterials::CanRender(actor))return false;
            auto entry=m_sharedSpawnVisuals.find(signature);
            if(entry==m_sharedSpawnVisuals.end()) {
                if(m_sharedSpawnVisuals.size()>=256)throw std::runtime_error("Spawn visual style limit reached (256)");
                auto* owner=SpawnRuntime::CallWorldContextGetter(TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),
                    TEXT("/Script/Engine.Default__GameplayStatics"),actor);
                if(!owner)throw std::runtime_error("Spawn visual game instance unavailable");
                const auto before=m_rootedVisualEffectMaterials.size();
                try {
                    auto materials=GhostMaterials::Create(owner,visualEffect,m_rootedVisualEffectMaterials);
                    for(auto* material:{materials.Overlay,materials.Body})if(material && !material->IsRootSet()) {
                        m_rootedVisualEffectMaterials.emplace_back(PS::WeakObject(material));material->SetRootSet();
                    }
                    entry=m_sharedSpawnVisuals.emplace(signature,materials).first;
                } catch(...) {
                    while(m_rootedVisualEffectMaterials.size()>before) {
                        if(auto* material=m_rootedVisualEffectMaterials.back().Get())material->ClearRootSet();
                        m_rootedVisualEffectMaterials.pop_back();
                    }
                    throw;
                }
            }
            if (!GhostMaterials::Apply(actor, entry->second, context)) return false;
            if(m_visualEffectAppliedActors.size()>=4096)
                std::erase_if(m_visualEffectAppliedActors,[](auto& value){return !value.second.Actor.Get();});
            if(m_visualEffectAppliedActors.size()<4096)
                m_visualEffectAppliedActors.insert_or_assign(actor,
                    AppliedVisual{PS::WeakObject(actor),{},signature});
            return true;
        }
        catch (const std::exception& error)
        {
            PS::Log<LogLevel::Warning>(STR("{} visual effect failed safely: {}\n"),
                context, PS::ToWideSafe(error.what()));
            return false;
        }
    }

}
