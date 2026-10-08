#pragma once
#include <cstdint>

namespace DragonWilds::NpcCollision {
    // Native byte enums: NoCollision=0, QueryOnly=1, ECC_Pawn=2,
    // ECR_Ignore=0, ECR_Block=2. Bypass the JSON enum conversion bridge.
    inline uint8_t Enabled(bool queryOnly) {
        return queryOnly ? 1 : 0;
    }
    struct Response { uint8_t Channel; uint8_t NewResponse; };
    inline Response PawnResponse(bool block) {
        return {2, static_cast<uint8_t>(block ? 2 : 0)};
    }
}
