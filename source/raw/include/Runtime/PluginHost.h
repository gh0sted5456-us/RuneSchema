#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace PS {
class PluginHost {
public:
    PluginHost();
    ~PluginHost();
    PluginHost(const PluginHost&)=delete;
    PluginHost& operator=(const PluginHost&)=delete;
    void Load(const std::filesystem::path& root);
    void Shutdown() noexcept;
    void OnUiInit();
    void OnUnrealInit();
    bool HasCapability(const std::string& capability) const;
    bool HasConnection(const std::string& connection) const;
    std::vector<std::string> Connections() const;
    std::string Call(const std::string& caller,const std::string& service,const std::string& request) const;
    std::vector<std::string> Diagnostics() const;
private:
    struct State;
    std::unique_ptr<State> m_state;
};
}
