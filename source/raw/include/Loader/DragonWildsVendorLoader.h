#pragma once
#include "Loader/DragonWildsModLoaderBase.h"
namespace DragonWilds {
class DragonWildsNpcLoader;
class DragonWildsVendorLoader final : public DragonWildsModLoaderBase {
public:
    explicit DragonWildsVendorLoader(DragonWildsNpcLoader* npcs);
protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase) override;
    bool OnInitialize() override;
    void OnLoad(const std::filesystem::path& path,const RC::StringType& mod,const EEngineLifecyclePhase& phase) override;
    void OnAutoReload(const RC::StringType& mod,const std::filesystem::path& path) override;
private:
    DragonWildsNpcLoader* m_npcs;
};
}
