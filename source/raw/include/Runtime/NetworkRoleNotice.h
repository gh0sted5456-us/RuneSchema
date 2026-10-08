#pragma once
#include "Runtime/NetworkRole.h"
#include <string>
namespace PS::Network {
inline bool IsGameplayRoleWorld(std::string_view path) {
    return !path.empty() && !path.starts_with("/Game/Maps/L_FrontEnd.");
}
class RoleNotice {
    Role candidate=Role::Unknown, reported=Role::Unknown;
    std::string candidateWorld, reportedWorld;
    unsigned samples=0;
    bool announced=false;
public:
    void Invalidate() {samples=0;candidate=Role::Unknown;candidateWorld.clear();}
    bool Observe(Role role,const std::string& world,bool settled=false) {
        const auto key=role==Role::Unknown?std::string{}:world;
        if(role!=candidate || key!=candidateWorld) {
            candidate=role;candidateWorld=key;samples=1;
        } else if(samples<2)++samples;
        if(settled)samples=2;
        if(samples<2 || (announced && role==reported && key==reportedWorld))return false;
        reported=role;reportedWorld=key;announced=true;return true;
    }
};
class RoleRetryBudget {
    unsigned remaining=0;
public:
    bool Start() {if(remaining)return false;remaining=8;return true;}
    bool Take() {if(!remaining)return false;--remaining;return true;}
    bool Active()const{return remaining!=0;}
    void Clear(){remaining=0;}
};
}
