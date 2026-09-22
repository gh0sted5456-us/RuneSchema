#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace PS::QuickInput {
enum class BoolBackend { None, Reflected };
struct BoolSlot {
    BoolBackend backend = BoolBackend::None;
    std::ptrdiff_t offset = -1;
    std::size_t size = 0;
};

inline bool InBounds(std::ptrdiff_t offset, std::size_t size, std::size_t extent) noexcept {
    return offset >= 0 && size > 0 && static_cast<std::size_t>(offset) <= extent
        && size <= extent - static_cast<std::size_t>(offset);
}
inline bool ContainsSpan(std::uintptr_t base, std::size_t size,
                         std::uintptr_t address, std::size_t length) noexcept {
    return length > 0 && address >= base && address - base <= size
        && length <= size - (address - base)
        && length - 1 <= std::numeric_limits<std::uintptr_t>::max() - address;
}

// Record ownership before a flag write so even a failed write/readback can be
// undone. The object reference and the selected backend remain with the caller.
struct SavedFlag {
    bool owned = false;
    bool previous = false;
    template<class Read, class Write> void Acquire(Read read, Write write) {
        if (owned) return;
        previous = read();
        owned = true;
        write(true);
    }
    template<class Write> bool Release(Write write) noexcept {
        if (!owned) return true;
        try { write(previous); } catch (...) { return false; }
        owned = false;
        previous = false;
        return true;
    }
    void ForgetExpiredObject() noexcept { owned = false; previous = false; }
};

// These are stacked engine calls, not plain booleans. Only balance calls that
// returned successfully; never reset another menu's pre-existing input locks.
struct ControllerLocks {
    bool look = false;
    bool move = false;
    template<class Look, class Move> void Acquire(Look setLook, Move setMove) {
        if (look || move) throw std::runtime_error("Previous controller input cleanup is incomplete");
        setLook(true); look = true;
        setMove(true); move = true;
    }
    template<class Look, class Move> bool Release(Look setLook, Move setMove) noexcept {
        if (move) { try { setMove(false); move = false; } catch (...) {} }
        if (look) { try { setLook(false); look = false; } catch (...) {} }
        return !look && !move;
    }
    bool Any() const noexcept { return look || move; }
    void ForgetExpiredObject() noexcept { look = false; move = false; }
};

enum class WatchdogResult { Keep, Closed, LostFocus, FirstFrameTimeout, FrameTimeout };
inline WatchdogResult CheckWatchdog(bool open, bool hasWindow, bool focused,
                                    bool hasFrame, std::uint64_t requestedAt,
                                    std::uint64_t frameAt, std::uint64_t now) noexcept {
    if (!open) return WatchdogResult::Closed;
    if (hasWindow && !focused) return WatchdogResult::LostFocus;
    const auto since = hasFrame ? frameAt : requestedAt;
    // Monotonic timestamps; a clock discontinuity is not a reason to steal input.
    if (now >= since && now - since >= 5000)
        return hasFrame ? WatchdogResult::FrameTimeout : WatchdogResult::FirstFrameTimeout;
    return WatchdogResult::Keep;
}
} // namespace PS::QuickInput
