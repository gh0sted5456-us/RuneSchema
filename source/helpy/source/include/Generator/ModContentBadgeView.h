#pragma once
// Draws ModContentBadges as compact pills that fit a width budget, with a "+N" pill for the rest.
// Depends only on ImGui so it renders the same in the window, the overlay and preview fixtures.
#include "Core/ModContentBadges.h"
#include "imgui.h"
#include <string>

namespace PS::ModContentBadgeView {
inline size_t Drawn = 0; // Pills drawn this frame, for preview fixtures.
inline ImVec4 Color(ModContentBadges::Category kind) {
    switch (kind) {
    case ModContentBadges::Category::Content: return {.96f, .80f, .41f, 1.f};      // gold
    case ModContentBadges::Category::World: return {.55f, .86f, .55f, 1.f};        // green
    case ModContentBadges::Category::Story: return {.80f, .66f, .98f, 1.f};        // violet
    case ModContentBadges::Category::Presentation: return {.98f, .62f, .68f, 1.f}; // rose
    case ModContentBadges::Category::Technical: return {.55f, .78f, .98f, 1.f};    // blue
    default: return {.90f, .68f, .45f, 1.f};                                       // amber: packaging
    }
}
// Draws pills from `at` on one line within `width` pixels. Returns the hovered badge description, if any.
inline std::string Draw(ImDrawList* draw, ImVec2 at, float width, float height, float padding, const std::vector<ModContentBadges::Badge>& badges) {
    std::string hovered;
    if (!draw || badges.empty() || width <= 0) return hovered;
    const auto mouse = ImGui::GetIO().MousePos;
    const float gap = padding;
    float x = at.x;
    const auto pill = [&](std::string_view label, ImVec4 color, std::string_view description) {
        const auto text = ImGui::CalcTextSize(label.data(), label.data() + label.size());
        const ImVec2 a{x, at.y}, b{x + text.x + padding * 2, at.y + height};
        auto fill = color; fill.w = .16f;
        draw->AddRectFilled(a, b, ImGui::GetColorU32(fill), height * .3f);
        draw->AddText({a.x + padding, a.y + (height - text.y) * .5f}, ImGui::GetColorU32(color), label.data(), label.data() + label.size());
        if (mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y) hovered = std::string(description);
        x = b.x + gap; ++Drawn;
    };
    size_t shown = 0;
    for (; shown < badges.size(); ++shown) {
        const auto& label = badges[shown].Label;
        const auto need = ImGui::CalcTextSize(label.data(), label.data() + label.size()).x + padding * 2;
        const size_t left = badges.size() - shown - 1;
        const auto overflow = left ? ImGui::CalcTextSize(("+" + std::to_string(left)).c_str()).x + padding * 2 + gap : 0.f;
        if (x + need + overflow > at.x + width) break;
        pill(badges[shown].Label, Color(badges[shown].Kind), badges[shown].Description);
    }
    if (shown < badges.size()) {
        std::vector<ModContentBadges::Badge> rest(badges.begin() + static_cast<std::ptrdiff_t>(shown), badges.end());
        const auto more = "+" + std::to_string(rest.size()), also = "Also: " + ModContentBadges::Summary(rest);
        pill(more, {.75f, .75f, .78f, 1.f}, also);
    }
    return hovered;
}
}
