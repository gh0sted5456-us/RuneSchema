#pragma once
#include "Loader/DragonWildsModLoaderBase.h"
namespace DragonWilds {
class DragonWildsNiagaraLoader final : public DragonWildsModLoaderBase {
public:
    DragonWildsNiagaraLoader();
    ~DragonWildsNiagaraLoader() override;
protected:
    bool CanInitialize(const EEngineLifecyclePhase&) override;
    bool OnInitialize() override;
    void OnLoad(const std::filesystem::path&,const RC::StringType&,const EEngineLifecyclePhase&) override;
    void OnAutoReload(const RC::StringType&,const std::filesystem::path&) override;
};
}
