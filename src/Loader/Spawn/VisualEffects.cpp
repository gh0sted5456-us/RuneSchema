#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/PlayerGhost.h"
#include "Loader/GhostScope.h"
#include "Loader/VisualPolicy.h"
#include "Loader/Spawn/GhostMaterials.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Utility/Logging.h"
#include <cmath>
#include "Loader/Spawn/RuntimeSupport.h"
#include <stdexcept>
#include <string_view>
using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::SpawnRuntime {
    nlohmann::json ValidateVisualEffect(const nlohmann::json& value)
    {
        if (!value.is_object())
            throw std::runtime_error("VisualEffect must be an object");
        if (!value.contains("Type") || !value.at("Type").is_string()
            || value.at("Type").get<std::string>() != "Ghost")
            throw std::runtime_error("VisualEffect.Type currently supports only 'Ghost'");

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
        if (!actor || visualEffect.empty() || !GhostMaterials::CanRender(actor)) return false;
        auto* playerClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if(playerClass && actor->IsA(playerClass)) {
            try { return PlayerGhost::Apply(actor,visualEffect); }
            catch(const std::exception& error) {
                PS::Log<LogLevel::Warning>(TEXT("Player ghost unavailable: {}\n"),PS::ToWideSafe(error.what()));
                return false;
            }
        }
        const auto signature = visualEffect.dump();
        if (const auto applied = m_visualEffectAppliedActors.find(actor);
            applied != m_visualEffectAppliedActors.end()
                && applied->second.Actor.Get() == actor && applied->second.Signature == signature) return true;
        try
        {
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
                        m_rootedVisualEffectMaterials.push_back(material);material->SetRootSet();
                    }
                    entry=m_sharedSpawnVisuals.emplace(signature,materials).first;
                } catch(...) {
                    while(m_rootedVisualEffectMaterials.size()>before) {
                        m_rootedVisualEffectMaterials.back()->ClearRootSet();m_rootedVisualEffectMaterials.pop_back();
                    }
                    throw;
                }
            }
            if (!GhostMaterials::Apply(actor, entry->second, context)) return false;
            if(m_visualEffectAppliedActors.size()>=4096)
                std::erase_if(m_visualEffectAppliedActors,[](auto& value){return !value.second.Actor.Get();});
            if(m_visualEffectAppliedActors.size()<4096)
                m_visualEffectAppliedActors.insert_or_assign(actor,AppliedVisual{FWeakObjectPtr(actor),signature});
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
