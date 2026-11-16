#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace PS {
namespace PluginCatalog {struct Plugin;}
class PluginHost {
public:
    PluginHost();
    ~PluginHost();
    PluginHost(const PluginHost&)=delete;
    PluginHost& operator=(const PluginHost&)=delete;
    void Load(const std::filesystem::path& root);
    void LoadOwned(const std::filesystem::path& root,const std::vector<std::filesystem::path>& enabled);
    void Shutdown() noexcept;
    void OnUiInit();
    void StartDesktop();
    void OnUnrealInit();
    bool HasCapability(const std::string& capability) const;
    bool HasConnection(const std::string& connection) const;
    std::vector<std::string> Connections() const;
    static std::string Call(const std::string& caller,const std::string& service,const std::string& request);
    std::vector<std::string> Diagnostics() const;
private:
    void LoadCatalog(const std::vector<PluginCatalog::Plugin>& manifests);
    struct State;
    std::unique_ptr<State> m_state;
};
}
