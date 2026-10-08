#pragma once

#include "Loader/DragonWildsModLoaderBase.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

namespace RC::Unreal { class UClass; }

namespace DragonWilds {
class DragonWildsGameplayEffectLoader final : public DragonWildsModLoaderBase {
    struct Definition {
        RC::StringType Owner;
        std::string Key;
        std::string ClassPath;
    };
public:
    DragonWildsGameplayEffectLoader();
    ~DragonWildsGameplayEffectLoader() override;
    RC::Unreal::UClass* Resolve(const std::string& key) const;
protected:
    void OnLoad(const std::filesystem::path&, const RC::StringType&, const EEngineLifecyclePhase&) override;
    void OnAutoReload(const RC::StringType&, const std::filesystem::path&) override;
    bool CanInitialize(const EEngineLifecyclePhase&) override;
    bool OnInitialize() override;
    void OnFinalizeLoad(const EEngineLifecyclePhase&) override;
private:
    void Queue(const nlohmann::json&, const RC::StringType&);
    void Apply(const Definition&);
    std::vector<Definition> m_definitions;
};
}
