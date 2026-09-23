#pragma once

namespace DragonWilds {
// For an explicitly rooted, session-scoped asset lease only. The caller looks
// up the saved GUObjectArray index before supplying the current pointer/flags.
// Do not use this as a general replacement for Unreal weak actor references.
inline bool PinnedObjectSlotMatches(const void* expected, const void* current,
    bool rooted, bool valid)
{
    return expected && current == expected && rooted && valid;
}
}
