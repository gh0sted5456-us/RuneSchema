#pragma once
#include "Loader/DragonWildsModLoaderBase.h"
#include "Loader/EquipmentRules.h"

namespace DragonWilds {
class DragonWildsEquipmentLoader final : public DragonWildsModLoaderBase {
public:
    DragonWildsEquipmentLoader();
    ~DragonWildsEquipmentLoader() override;
protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase) override;
    bool OnInitialize() override { return true; }
    void OnLoad(const std::filesystem::path&, const RC::StringType&, const EEngineLifecyclePhase&) override;
    void OnFinalizeLoad(const EEngineLifecyclePhase&) override;
    void OnAutoReload(const RC::StringType&, const std::filesystem::path&) override;
private:
    EquipmentRules::Rules m_rules;
    bool m_finalized = false;
};
}
