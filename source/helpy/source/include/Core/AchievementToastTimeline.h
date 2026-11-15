#pragma once
// Times are seconds on a monotonic clock.
#include "Core/ModAchievements.h"
#include <algorithm>
#include <deque>
#include <optional>

namespace PS::AchievementToastTimeline {
inline constexpr double Rise = .30, Seal = .20, Break = .38, Reveal = .28, Hold = 4.2;
inline constexpr double Fade = .25, RuneReturn = .16, CloseBrackets = .34, Exit = .32, Gap = .25;
inline constexpr double BreakAt = Rise + Seal, RevealAt = BreakAt + Break, HoldAt = RevealAt + Reveal;
inline constexpr double FadeAt = HoldAt + Hold, RuneReturnAt = FadeAt + Fade;
inline constexpr double CloseAt = RuneReturnAt + RuneReturn, ExitAt = CloseAt + CloseBrackets;

struct Frame {
    bool Visible = false;
    bool Animating = false;
    float Offset = 1; // 0 = fully raised, 1 = below the bottom edge.
    float Alpha = 0;
    float Brackets = 0; // 0 = assembled, 1 = fully separated.
    float Rune = 0;
    float Text = 0;
};
class Timeline {
    std::optional<ModAchievements::Unlock> m_current;
    double m_started = 0, m_pausedAt = -1;

    static float Ease(double progress) {
        const auto u = static_cast<float>(std::clamp(progress, 0.0, 1.0));
        return u * u * (3.f - 2.f * u);
    }
public:
    bool Animate = true;
    const ModAchievements::Unlock* Current() const { return m_current ? &*m_current : nullptr; }
    bool Advance(std::deque<ModAchievements::Unlock>& queue, double now, bool showable) {
        if (!showable) { Pause(now); return false; }
        Resume(now);
        if (m_current && now - m_started >= Total()) m_current.reset();
        if (m_current || queue.empty()) return false;
        m_current = std::move(queue.front());
        queue.pop_front();
        m_started = now;
        return true;
    }
    double Total() const { return (Animate ? ExitAt + Exit : Hold) + Gap; }
    Frame At(double now) const {
        Frame frame;
        if (!m_current) return frame;
        const double elapsed = std::max(0.0, (m_pausedAt >= 0 ? m_pausedAt : now) - m_started);
        if (elapsed >= (Animate ? ExitAt + Exit : Hold)) return frame;
        frame.Visible = true;
        frame.Alpha = 1;
        frame.Offset = 0;
        if (!Animate) { frame.Brackets = 1; frame.Text = 1; return frame; }

        frame.Animating = elapsed < HoldAt || elapsed >= FadeAt;
        frame.Brackets = Ease((elapsed - BreakAt) / Break) * (1.f - Ease((elapsed - CloseAt) / CloseBrackets));
        const auto reveal = (elapsed - RevealAt) / Reveal;
        frame.Rune = std::max(1.f - Ease(reveal * 2), Ease((elapsed - RuneReturnAt) / RuneReturn));
        frame.Text = Ease(reveal * 2 - 1) * (1.f - Ease((elapsed - FadeAt) / Fade));
        if (elapsed < Rise) {
            frame.Alpha = Ease(elapsed / Rise);
            frame.Offset = 1.f - frame.Alpha;
        } else if (elapsed >= ExitAt) {
            frame.Offset = Ease((elapsed - ExitAt) / Exit);
            frame.Alpha = 1.f - frame.Offset;
        }
        return frame;
    }
    // Freezes the current popup while the HUD cannot show it (game unfocused), so it is never lost.
    void Pause(double now) { if (m_current && m_pausedAt < 0) m_pausedAt = now; }
    void Resume(double now) { if (m_pausedAt >= 0) { m_started += now - m_pausedAt; m_pausedAt = -1; } }
    void Reset() { m_current.reset(); m_pausedAt = -1; }
};
// Card height and text layout in logical pixels (multiply by the HUD scale).
inline constexpr float Width = 430, Height = 92, Margin = 26;
// Y of the card's top edge in client pixels for a given slide offset.
inline float Top(float clientHeight, float scale, float offset) {
    const float height = Height * scale, margin = Margin * scale;
    return clientHeight - margin - height + offset * (height + margin);
}
}
