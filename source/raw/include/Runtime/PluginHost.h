#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RC::Unreal {class UObject;}
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
    // runeschema.server.request (Core/PluginServerRequest.h). The transport sends a plugin request
    // to the server over the owning-player bridge and returns its revision, or 0 when unavailable.
    using ServerTransport=std::function<int64_t(const std::string& plugin,const std::string& action,const std::string& payload)>;
    static void SetServerTransport(ServerTransport transport);
    // Server side: run the same plugin's "<Plugin>.server" for the player whose pawn sent it.
    // Returns the result object as JSON text for the plugin.result notification.
    static std::string HandleRemotePluginRequest(RC::Unreal::UObject* pawn,const std::string& plugin,const std::string& action,const std::string& payload);
    // Sending side: hand a plugin.result answer to "<Plugin>.server-result" if this game asked for it.
    static void DeliverPluginResult(int64_t revision,const std::string& plugin,const std::string& payload);
    std::vector<std::string> Diagnostics() const;
private:
    void LoadCatalog(const std::vector<PluginCatalog::Plugin>& manifests);
    struct State;
    std::unique_ptr<State> m_state;
};
}
