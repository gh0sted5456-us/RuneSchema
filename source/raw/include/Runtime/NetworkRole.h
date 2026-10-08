#pragma once
#include <string_view>
namespace PS::Network {
enum class Role { Unknown, Standalone, Client, ListenServer, DedicatedServer };
inline Role Classify(bool standalone, bool server, bool dedicated) {
    if(dedicated)return server && !standalone?Role::DedicatedServer:Role::Unknown;
    if(standalone)return server?Role::Standalone:Role::Unknown;
    return server?Role::ListenServer:Role::Client;
}
inline std::string_view Label(Role role) {
    switch(role) {
    case Role::Standalone:return "standalone";
    case Role::Client:return "client";
    case Role::ListenServer:return "listen-server";
    case Role::DedicatedServer:return "dedicated-server";
    default:return "unknown";
    }
}
inline bool OwnsGameplay(Role role) {
    return role==Role::Standalone || role==Role::ListenServer || role==Role::DedicatedServer;
}
inline bool HasLocalPresentation(Role role) {
    return role==Role::Standalone || role==Role::ListenServer || role==Role::Client;
}
}
