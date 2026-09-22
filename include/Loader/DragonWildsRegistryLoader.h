#pragma once

#include <set>
#include <string>
#include "Loader/DragonWildsModLoaderBase.h"

namespace PS::Network { class RegistryBridge; }

namespace DragonWilds {
class DragonWildsRegistryLoader final : public DragonWildsModLoaderBase {
public:
    explicit DragonWildsRegistryLoader(PS::Network::RegistryBridge& bridge);

protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase) override;
    bool OnInitialize() override;
    void OnLoad(const std::filesystem::path& path,const RC::StringType& mod,
        const EEngineLifecyclePhase& phase) override;
    void OnFinalizeLoad(const EEngineLifecyclePhase& phase) override;
    void OnAutoReload(const RC::StringType& mod,const std::filesystem::path& path) override;

private:
    PS::Network::RegistryBridge& m_bridge;
    nlohmann::json m_modEntries=nlohmann::json::array();
    nlohmann::json m_audit=nlohmann::json::array();
    std::set<std::string> m_keys;

    void LoadDocument(const nlohmann::json& document,const std::string& owner,const std::string& source);
    nlohmann::json NormalizeEntry(const nlohmann::json& entry,const std::string& owner);
    void LoadCookedRegistries();
    void WriteMerged();
};
}
