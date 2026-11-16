#pragma once
#include <cstdint>

namespace PS {
// One shared surface is presented at a time. Interactive panels can replace a
// passive HUD, but cannot replace another live interactive panel.
struct HudSurfaceOwner {
    uint64_t Client=0;
    bool Interactive=false;
    bool Owns(uint64_t client) const { return client&&client==Client; }
    bool CanOpen(uint64_t client,bool interactive,bool alive) const {
        return client&&(!Client||Owns(client)||!alive||(!Interactive&&interactive));
    }
    void Open(uint64_t client,bool interactive){Client=client;Interactive=interactive;}
    void Release(uint64_t client){if(Owns(client)){Client=0;Interactive=false;}}
};
}
