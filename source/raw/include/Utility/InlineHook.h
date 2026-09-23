#pragma once
#include <safetyhook.hpp>
#include <utility>

namespace PS {
// Publish the trampoline before enabling the detour.
inline bool InstallInlineHook(SafetyHookInline& slot, void* target, void* callback) {
    if (!target || !callback || slot) return false;
    auto created = safetyhook::InlineHook::create(target, callback, safetyhook::InlineHook::StartDisabled);
    if (!created) return false;
    slot = std::move(*created);
    if (slot.enable()) return true;
    slot = {};
    return false;
}
}
