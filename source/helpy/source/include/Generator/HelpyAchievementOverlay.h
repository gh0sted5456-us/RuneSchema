#pragma once
// The native HUD and windowed fallback share the same award timeline.
#include "Core/AchievementToastTimeline.h"
#include "Core/HelpyOverlayGeometry.h"
#include "Generator/HelpyImGuiTheme.h"
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <cstddef>
#include <numbers>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <string>
#include <string_view>

namespace PS {
class HelpyAchievementOverlay {
    static constexpr const wchar_t* ClassName = L"RuneSchema.Helpy.Achievements.v1";
    static constexpr COLORREF ColorKey = RGB(255, 0, 255);
    HINSTANCE m_module{}; HWND m_window{}; bool m_registered = false, m_showing = false;
    AchievementToastTimeline::Timeline m_timeline;
    HelpyImGuiTheme::Theme m_theme{};
    float m_scale = 1; int m_width = 430, m_height = 92, m_margin = 26;
    AchievementToastTimeline::Frame m_frame;
    ULONGLONG m_soundStarted = 0;
    std::chrono::steady_clock::time_point m_origin = std::chrono::steady_clock::now();

    static bool PlayChime() {
        static const auto wave = [] {
            constexpr unsigned rate = 16000;
            constexpr unsigned samples = rate * 46 / 100;
            struct Wave {
                char Riff[4] = {'R','I','F','F'};
                uint32_t Size = 36 + samples * 2;
                char Format[8] = {'W','A','V','E','f','m','t',' '};
                uint32_t FormatSize = 16;
                uint16_t Encoding = 1, Channels = 1;
                uint32_t Rate = rate, BytesPerSecond = rate * 2;
                uint16_t BlockAlign = 2, Bits = 16;
                char Data[4] = {'d','a','t','a'};
                uint32_t DataSize = samples * 2;
                std::array<int16_t, samples> Pcm{};
            } result;
            static_assert(offsetof(Wave, Pcm) == 44);
            constexpr double notes[] = {523.25, 659.25, 1046.50};
            for (unsigned i = 0; i < samples; ++i) {
                double signal = 0;
                for (unsigned note = 0; note < std::size(notes); ++note) {
                    const double time = double(i) / rate - note * .085;
                    if (time < 0 || time >= .28) continue;
                    const double envelope = std::min(time / .012, 1.0) * std::pow(1 - time / .28, 2);
                    const double phase = time * notes[note] * 2 * std::numbers::pi;
                    signal += envelope * (std::sin(phase) + .18 * std::sin(phase * 2));
                }
                result.Pcm[i] = static_cast<int16_t>(4200 * signal);
            }
            return result;
        }();
        return PlaySoundW(reinterpret_cast<LPCWSTR>(&wave), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP) != FALSE;
    }
    static COLORREF Color(const ImVec4& value) {
        const auto channel = [](float x) { return static_cast<int>(std::lround(std::clamp(x, 0.f, 1.f) * 255.f)); };
        return RGB(channel(value.x), channel(value.y), channel(value.z));
    }
    static COLORREF Blend(COLORREF a, COLORREF b, float amount) {
        const auto mix = [&](int x, int y) { return static_cast<int>(std::lround(x + (y - x) * amount)); };
        return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)), mix(GetBValue(a), GetBValue(b)));
    }
    static std::wstring Wide(std::string_view text) {
        if (text.empty()) return {};
        const int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (chars <= 0) return L"";
        std::wstring result(static_cast<size_t>(chars), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), chars);
        return result;
    }
    int Px(float value) const { return std::max(1, static_cast<int>(std::lround(value * m_scale))); }
    static void Rounded(HDC dc, RECT box, int radius, COLORREF fill, COLORREF border) {
        const auto brush = CreateSolidBrush(fill); const auto pen = CreatePen(PS_SOLID, 1, border);
        const auto oldBrush = SelectObject(dc, brush); const auto oldPen = SelectObject(dc, pen);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(brush); DeleteObject(pen);
    }
    static void Text(HDC dc, HFONT font, COLORREF color, const std::wstring& text, RECT box, UINT flags) {
        const auto old = SelectObject(dc, font); SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &box, flags | DT_NOPREFIX);
        SelectObject(dc, old);
    }
    void Paint(HDC dc, RECT bounds) {
        const auto clear = CreateSolidBrush(ColorKey); FillRect(dc, &bounds, clear); DeleteObject(clear);
        const auto* unlock = m_timeline.Current();
        if (!unlock || !m_frame.Visible) return;
        SetBkMode(dc, TRANSPARENT);
        const int top = static_cast<int>(std::lround(m_frame.Offset * (m_height + m_margin)));
        const COLORREF bg = Color(m_theme.panelAlt), gold = Color(m_theme.accent), text = Color(m_theme.text), muted = Color(m_theme.textDisabled);
        const int radius = std::clamp(static_cast<int>(std::lround(m_theme.childRounding * m_scale)) * 2, 8, 40);
        RECT shadow{4, top + 4, m_width - 1, top + m_height - 1};
        Rounded(dc, shadow, radius, Blend(bg, RGB(0, 0, 0), .55f), Blend(bg, RGB(0, 0, 0), .65f));
        RECT body{1, top + 1, m_width - 5, top + m_height - 5};
        Rounded(dc, body, radius, bg, gold);
        const auto name = Wide(m_theme.fontFamily);
        const auto* face = name.empty() ? L"Segoe UI" : name.c_str();
        const auto font = [&](float size, int weight) { return CreateFontW(-std::max(9, Px(size * m_theme.fontScale)), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face); };
        if (m_frame.Rune > 0 || m_frame.Brackets < 1) {
            const HFONT emblem = font(34, FW_SEMIBOLD);
            const int middle = (body.left + body.right) / 2;
            const int logoTop = top + Px(17), logoBottom = top + m_height - Px(18);
            const int shift = Px(22) + static_cast<int>(m_frame.Brackets * Px(73));
            const int lift = static_cast<int>(m_frame.Brackets * Px(9));
            const COLORREF bracket = Blend(bg, gold, 1.f - m_frame.Brackets);
            const UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
            if (m_frame.Brackets < 1) {
                Text(dc, emblem, bracket, L"[", {middle-shift-Px(20),logoTop-lift,middle-shift+Px(20),logoBottom-lift}, flags);
                Text(dc, emblem, bracket, L"]", {middle+shift-Px(20),logoTop-lift,middle+shift+Px(20),logoBottom-lift}, flags);
            }
            if (m_frame.Rune > 0)
                Text(dc, emblem, Blend(bg, gold, m_frame.Rune), L"R", {middle-Px(20),logoTop,middle+Px(20),logoBottom}, flags);
            DeleteObject(emblem);
        }
        if (m_frame.Text <= 0) return;
        const HFONT labelFont = font(10.5f, FW_SEMIBOLD), heading = font(16, FW_SEMIBOLD), regular = font(12, FW_NORMAL);
        const int textLeft = body.left + Px(18), right = body.right - Px(16);
        const auto labelColor = Blend(bg, gold, m_frame.Text);
        // Points pill on the right; a plugin notice has a label of its own and no points.
        int textRight = right;
        if (unlock->Label.empty()) {
            const auto points = L"+" + std::to_wstring(unlock->Points);
            SIZE pointsSize{}; { const auto old = SelectObject(dc, heading); GetTextExtentPoint32W(dc, points.c_str(), static_cast<int>(points.size()), &pointsSize); SelectObject(dc, old); }
            const int pillWidth = pointsSize.cx + Px(22), pillHeight = Px(28), pillTop = top + (m_height - pillHeight) / 2 - Px(2);
            RECT pill{right - pillWidth, pillTop, right, pillTop + pillHeight};
            Rounded(dc, pill, pillHeight, Blend(bg, gold, .2f * m_frame.Text), Blend(bg, gold, .55f * m_frame.Text));
            Text(dc, heading, labelColor, points, pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            textRight = pill.left - Px(12);
        }
        int y = top + Px(13);
        Text(dc, labelFont, labelColor, unlock->Label.empty() ? std::wstring(L"RUNESCHEMA GAMER POINTS") : Wide(unlock->Label), RECT{textLeft, y, textRight, y + Px(15)}, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += Px(16);
        Text(dc, heading, Blend(bg, text, m_frame.Text), Wide(unlock->Title), RECT{textLeft, y, textRight, y + Px(22)}, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += Px(23);
        const auto detail = unlock->Description.empty() ? Wide(unlock->ModName) : Wide(unlock->Description) + L"  \x00B7  " + Wide(unlock->ModName);
        Text(dc, regular, Blend(bg, muted, m_frame.Text), detail, RECT{textLeft, y, textRight, y + Px(18)}, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        DeleteObject(labelFont); DeleteObject(heading); DeleteObject(regular);
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<HelpyAchievementOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) { auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam); self = static_cast<HelpyAchievementOverlay*>(create->lpCreateParams); SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)); }
        if (message == WM_NCHITTEST) return HTTRANSPARENT;
        if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{}; const auto dc = BeginPaint(hwnd, &paint); RECT bounds{}; GetClientRect(hwnd, &bounds);
            const auto buffer = CreateCompatibleDC(dc); const auto bitmap = CreateCompatibleBitmap(dc, std::max(1L, bounds.right), std::max(1L, bounds.bottom));
            if (buffer && bitmap) { const auto prior = SelectObject(buffer, bitmap); if (self) self->Paint(buffer, bounds); BitBlt(dc, 0, 0, bounds.right, bounds.bottom, buffer, 0, 0, SRCCOPY); SelectObject(buffer, prior); }
            else if (self) self->Paint(dc, bounds);
            if (bitmap) DeleteObject(bitmap);
            if (buffer) DeleteDC(buffer);
            EndPaint(hwnd, &paint); return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    bool Ensure(HINSTANCE module) {
        if (m_window) return true;
        m_module = module;
        WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = WindowProc; cls.hInstance = module; cls.lpszClassName = ClassName;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        m_registered = true;
        m_window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            ClassName, L"RuneSchema achievements", WS_POPUP, 0, 0, m_width, m_height, nullptr, nullptr, module, this);
        if (!m_window) return false;
        SetLayeredWindowAttributes(m_window, ColorKey, 0, LWA_COLORKEY | LWA_ALPHA);
        return true;
    }
public:
    void Hide() { if (m_window && m_showing) ShowWindow(m_window, SW_HIDE); m_showing = false; }
    void Reset() { m_timeline.Reset(); m_frame = {}; Hide(); }
    void Close() {
        Reset();
        if (m_soundStarted && GetTickCount64() - m_soundStarted < 500) PlaySoundW(nullptr, nullptr, 0);
        m_soundStarted = 0;
        if (m_window) { DestroyWindow(m_window); m_window = nullptr; }
        if (m_registered) { UnregisterClassW(ClassName, m_module); m_registered = false; }
    }
    bool Busy() const { return m_timeline.Current() != nullptr; }
    bool Animating() const { return m_frame.Animating; }
    const auto* Current() const { return m_timeline.Current(); }
    const auto& Frame() const { return m_frame; }
#if defined(HELPY_PREVIEW)
    void PreviewAt(double elapsed, std::deque<ModAchievements::Unlock>& queue, bool animate = true) {
        m_timeline.Animate = animate;
        m_timeline.Advance(queue, 0, true);
        m_frame = m_timeline.At(elapsed);
    }
#endif
    void Update(HINSTANCE module, HWND game, const HelpyImGuiTheme::Theme& theme, bool native, bool desktop, bool animate, std::deque<ModAchievements::Unlock>& queue) {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_origin).count();
        // Turning gamer-points popups off drops achievements only; plugin notices (Label set) still show.
        if (!theme.achievementPopups) {
            const bool award = Busy() && Current() && Current()->Label.empty();
            std::erase_if(queue, [](const ModAchievements::Unlock& unlock) { return unlock.Label.empty(); });
            if (award) Reset();
        }
        if (queue.empty() && !Busy()) return;
        if (!native && (!desktop || !game || IsIconic(game) || !Ensure(module))) {
            m_timeline.Pause(now);
            m_frame = {};
            Hide();
            return;
        }
        m_timeline.Animate = animate;
        if (m_timeline.Advance(queue, now, true) && theme.achievementSound && PlayChime())
            m_soundStarted = GetTickCount64();
        m_frame = m_timeline.At(now);
        if (native || !m_frame.Visible) { Hide(); return; }
        m_theme = theme;
        const UINT dpi = GetDpiForWindow(game);
        m_scale = std::clamp((theme.dpiAware && dpi ? float(dpi) / 96.f : 1.f) * theme.userScale, .75f, 3.f);
        RECT client{}; GetClientRect(game, &client); POINT origin{client.left, client.top}; ClientToScreen(game, &origin);
        MONITORINFO monitor{sizeof(monitor)}; GetMonitorInfoW(MonitorFromWindow(game, MONITOR_DEFAULTTONEAREST), &monitor);
        const auto bounds = HelpyOverlayGeometry::Normalize(origin, client, monitor.rcWork);
        m_margin = Px(AchievementToastTimeline::Margin);
        m_width = std::min(Px(AchievementToastTimeline::Width), HelpyOverlayGeometry::AvailableWidth(bounds, m_margin));
        m_height = Px(AchievementToastTimeline::Height);
        if (m_width < 220 || client.bottom - client.top < m_height * 2) { Hide(); return; }
        // The window spans the card plus the bottom margin; the card slides inside it and clips at the edge.
        const int x = std::clamp(bounds.ClientX + (bounds.ClientWidth - m_width) / 2, bounds.MonitorLeft, std::max(bounds.MonitorLeft, bounds.MonitorRight - m_width));
        const int bottom = std::min(int(origin.y + client.bottom), bounds.MonitorBottom);
        const int y = bottom - m_height - m_margin;
        SetWindowPos(m_window, HWND_TOPMOST, x, y, m_width, m_height + m_margin, UINT(SWP_NOACTIVATE) | (m_showing ? 0u : UINT(SWP_SHOWWINDOW)));
        SetLayeredWindowAttributes(m_window, ColorKey, BYTE(std::lround(246 * m_frame.Alpha)), LWA_COLORKEY | LWA_ALPHA);
        m_showing = true;
        InvalidateRect(m_window, nullptr, FALSE); UpdateWindow(m_window);
    }
};
}
