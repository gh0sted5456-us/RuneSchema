#pragma once
// Profile UI for mod achievements: your points and per-mod progress, and the summary other players share.
#include "Runtime/HelpyAchievements.h"
#include "Core/ProcessExitFuture.h"
#if defined(HELPY_PREVIEW)
#include "Core/HelpyPreviewHooks.h"
#endif
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyBackgrounds.h"
#include "imgui.h"
#include <ctime>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace PS::AchievementsPanel {
inline bool ShowLocked = true;
inline std::string Date(uint64_t seconds, const char* format = "%Y-%m-%d") {
    if (seconds < 1000000) return {};
    const std::time_t time = static_cast<std::time_t>(seconds); std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    char text[32]{}; std::strftime(text, sizeof(text), format, &local); return text;
}
// One line under the profile name: "120 achievement points · 9 unlocked".
inline void Header() {
    const auto& progress = HelpyAchievements::Progress;
    if (!HelpyAchievements::ProgressLoaded) return;
    const auto points = progress.Points();
    if (!points && HelpyAchievements::Definitions.empty()) return;
    ImGui::TextColored(HelpyImGuiTheme::Current.accent, "%d achievement points", points);
    ImGui::SameLine(); ImGui::TextDisabled("\xC2\xB7 %zu unlocked", progress.UnlockedCount());
    if (ImGui::IsItemHovered()) HelpyImGuiTheme::Tooltip("Earned from mods that include achievements, UE4SS or RuneSchema. Shared with players on servers running RuneSchema and Helpy.");
}
inline void Bar(float fraction, float width, float height) {
    const auto& t = HelpyImGuiTheme::Current; auto* draw = ImGui::GetWindowDrawList(); const auto a = ImGui::GetCursorScreenPos();
    draw->AddRectFilled(a, {a.x + width, a.y + height}, ImGui::GetColorU32(t.border), height * .5f);
    if (fraction > 0) draw->AddRectFilled(a, {a.x + width * std::clamp(fraction, 0.f, 1.f), a.y + height}, ImGui::GetColorU32(t.accent), height * .5f);
    ImGui::Dummy({width, height});
}
// Full list, grouped by mod. Hidden achievements show as "???" until unlocked.
inline void Render() {
    namespace A = ModAchievements;
    const auto& t = HelpyImGuiTheme::Current; const auto& progress = HelpyAchievements::Progress;
    if (!HelpyAchievements::Status.empty()) ImGui::TextWrapped("%s", HelpyAchievements::Status.c_str());
    if (HelpyAchievements::Available && HelpyAchievements::Definitions.empty() && HelpyAchievements::Errors.empty()) {
        ImGui::TextWrapped("None of your enabled mods include achievements yet. Mod authors add an achievements.json to their mod folder, UE4SS or RuneSchema.");
    }
    ImGui::Checkbox("Show locked achievements", &ShowLocked);
    std::map<std::string, std::vector<const A::Definition*>> mods;
    for (const auto& definition : HelpyAchievements::Definitions) mods[definition.Owner].push_back(&definition);
    for (const auto& [owner, list] : mods) {
        int earned = 0, total = 0; size_t unlocked = 0;
        for (const auto* definition : list) { total += definition->Points; if (progress.Unlocked(definition->Key())) { ++unlocked; earned += progress.Entries.at(definition->Key()).Points; } }
        const auto label = list.front()->ModName + "  (" + std::to_string(unlocked) + "/" + std::to_string(list.size()) + ", " + std::to_string(earned) + "/" + std::to_string(total) + " pts)###ach-" + owner;
        if (!ImGui::CollapsingHeader(label.c_str())) continue;
        ImGui::PushID(owner.c_str());
        for (const auto* definition : list) {
            const bool done = progress.Unlocked(definition->Key());
            if (!done && !ShowLocked) continue;
            ImGui::PushID(definition->Id.c_str());
            const bool secret = definition->Hidden && !done;
            ImGui::BeginGroup();
            ImGui::TextColored(done ? t.accent : t.textDisabled, "%s", done ? "\xE2\x98\x85" : "\xE2\x98\x86"); ImGui::SameLine();
            ImGui::TextUnformatted(secret ? "Hidden achievement" : definition->Title.c_str()); ImGui::SameLine();
            ImGui::TextDisabled("%d pts", definition->Points);
            if (!secret && !definition->Description.empty()) ImGui::TextWrapped("%s", definition->Description.c_str());
            if (done) { const auto date = Date(progress.Entries.at(definition->Key()).Unlocked); if (!date.empty()) ImGui::TextDisabled("Unlocked %s", date.c_str()); }
            else if (!secret) Bar(float(progress.Fraction(*definition)), std::min(t.X(260), ImGui::GetContentRegionAvail().x), t.Y(6));
            ImGui::EndGroup();
            ImGui::Spacing();
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    // Points from mods that are disabled or removed stay earned.
    int retained = 0; size_t retainedCount = 0;
    for (const auto& [key, entry] : progress.Entries) if (entry.Unlocked && !mods.contains(entry.Owner)) { retained += entry.Points; ++retainedCount; }
    if (retainedCount) ImGui::TextDisabled("%d points from %zu achievements in mods that are not enabled now.", retained, retainedCount);
    for (const auto& [owner, error] : HelpyAchievements::Errors) ImGui::TextColored(t.warning, "%s: achievements.json was not loaded: %s", owner.c_str(), error.c_str());
}
inline ProcessExitFuture<HelpyAchievements::Loaded> ModRead;
inline HelpyAchievements::Loaded ModData;
inline std::string ModOwner, ModTitle;
inline nlohmann::json ModRequest;
#if defined(HELPY_PREVIEW)
inline size_t PreviewUnlocked = 0, PreviewLocked = 0, PreviewHidden = 0;
inline ImVec4 PreviewModBounds{}, PreviewEarnedRow{}, PreviewLockedRow{}, PreviewHiddenRow{};
inline std::string PreviewTooltip;
inline std::vector<int> PreviewBadgeValues;
#endif
inline void OpenMod(const nlohmann::json& row, const std::string& title) {
    ModOwner = HelpyAchievements::Owner(std::filesystem::u8path(row.value("Folder", row.value("Name", std::string{}))));
    ModTitle = title;
    ModRequest = nlohmann::json::array({row});
}
inline void OpenAll(const nlohmann::json& rows) {
    ModOwner.clear();
    ModTitle.clear();
    ModRequest = rows;
}
inline void Requirements(const ModAchievements::Definition& definition) {
    constexpr std::array labels{"Defeat enemies", "Craft", "Unlock recipes", "Unlock buildings", "Update quests",
        "Gain skill XP", "Die and respawn", "Place buildings", "Use items", "Trigger a mod event"};
    static_assert(labels.size() == ModAchievements::Events.size());
    ImGui::TextUnformatted(definition.Conditions.size() == 1 ? "Unlock requirement"
        : definition.Any ? "Meet any of these requirements" : "Meet all of these requirements");
    for (const auto& condition : definition.Conditions) {
        const auto event = std::find(ModAchievements::Events.begin(), ModAchievements::Events.end(), condition.Event);
        if (condition.AtLeast > 0) ImGui::BulletText("Reach %.12g total XP", condition.AtLeast);
        else ImGui::BulletText("%s × %.9g", event != ModAchievements::Events.end()
            ? labels[static_cast<size_t>(event - ModAchievements::Events.begin())] : condition.Event.c_str(), condition.Count);
        if (!condition.Subject.empty()) ImGui::TextWrapped("%s", condition.Subject.c_str());
        for (const auto& [path, value] : condition.Where.items())
            ImGui::TextWrapped("%s = %s", path.c_str(), value.dump().c_str());
    }
}
inline void DrawPointsBadge(int points, ImVec2 at, float size, bool locked, bool total = false) {
    struct Tier { int Reward, Total; const char* Art; ImVec4 Tint, Fill; };
    constexpr auto neutral = "power-neutralzone.png";
    constexpr ImVec4 white{1,1,1,1};
    constexpr Tier tiers[]{
        {  0,     0, neutral, white, {.55f,.57f,.60f,1}},
        {  1,     1, "power-abovezone.png", white, {.43f,.64f,.28f,1}},
        { 10,   100, neutral, {.35f,.94f,.96f,1}, {.16f,.68f,.71f,1}},
        { 15,   250, neutral, {.45f,.66f,1,1}, {.22f,.41f,.85f,1}},
        { 25,   500, neutral, {.82f,.50f,1,1}, {.56f,.27f,.80f,1}},
        { 35,  1000, "power-atzone.png", white, {.82f,.58f,.20f,1}},
        { 50,  2500, "power-belowzone.png", white, {.85f,.34f,.16f,1}},
        { 75,  5000, "power-severelybelowzone.png", white, {.78f,.17f,.23f,1}},
        {100, 10000, neutral, {.32f,.34f,.40f,1}, {.08f,.09f,.12f,1}}
    };
    const Tier* tier = &tiers[0];
    for (const auto& candidate : tiers) {
        if (points < (total ? candidate.Total : candidate.Reward)) break;
        tier = &candidate;
    }
    const float alpha = locked ? .52f : 1.f;
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end{at.x + size, at.y + size}, center{at.x + size * .5f, at.y + size * .5f};
    HelpyBackgrounds::RequestImage(tier->Art);
    if (const auto* image = HelpyBackgrounds::Find(tier->Art); image && image->Texture) {
        auto tint = tier->Tint;
        tint.w = alpha;
        draw->AddImage(ImTextureRef(image->Texture), at, end, {0,0}, {1,1}, ImGui::GetColorU32(tint));
    } else {
        const ImVec2 diamond[]{{center.x,at.y},{end.x,center.y},{center.x,end.y},{at.x,center.y}};
        auto fill = tier->Fill;
        fill.w = alpha;
        draw->AddConvexPolyFilled(diamond, 4, ImGui::GetColorU32(fill));
        draw->AddPolyline(diamond, 4, ImGui::GetColorU32({1,1,1,alpha * .6f}), ImDrawFlags_Closed, 1);
    }
    const auto label = points < 0 ? std::string("?") : std::to_string(points);
    auto* font = ImGui::GetFont();
    const float baseSize = size * .39f;
    const float measured = font->CalcTextSizeA(baseSize, FLT_MAX, 0, label.c_str()).x;
    const float fontSize = baseSize * std::min(1.f, size * .77f / std::max(1.f, measured));
    const auto text = font->CalcTextSizeA(fontSize, FLT_MAX, 0, label.c_str());
    const ImVec2 position{center.x - text.x * .5f, center.y - text.y * .5f};
    draw->AddText(font, fontSize, {position.x + 1, position.y + 1}, IM_COL32(0,0,0,230), label.c_str());
    draw->AddText(font, fontSize, position, ImGui::GetColorU32({1,1,1,locked ? .72f : 1.f}), label.c_str());
#if defined(HELPY_PREVIEW)
    PreviewBadgeValues.push_back(points);
#endif
}
inline bool RenderModGroup(const std::string& owner, std::span<const ModAchievements::Definition> definitions) {
    const auto& theme = HelpyImGuiTheme::Current;
    const auto& progress = HelpyAchievements::Progress;
    const auto draw = [&](const char* title, int points, const ModAchievements::Definition* definition, uint64_t unlocked) {
        const bool hidden = !unlocked && definition && definition->Hidden;
        if (hidden) title = "Hidden achievement";
        const char* description = !hidden && definition ? definition->Description.c_str() : "";
        const float width = ImGui::GetContentRegionAvail().x;
        const float padding = theme.X(9), badge = theme.Y(42), gap = theme.Y(4);
        const float wrap = std::max(1.f, width - badge - padding * 3);
        const auto titleSize = ImGui::CalcTextSize(title, nullptr, false, wrap);
        const float detailHeight = *description ? ImGui::CalcTextSize(description, nullptr, false, wrap).y + gap : 0;
        const float height = std::max(badge, titleSize.y + detailHeight) + padding * 2;
        const auto at = ImGui::GetCursorScreenPos();
        ImGui::Dummy({width, height});
        if (ImGui::IsItemVisible()) {
            auto* canvas = ImGui::GetWindowDrawList();
            const ImVec2 end{at.x + width, at.y + height};
            const float rounding = theme.X(theme.childRounding);
            auto border = unlocked ? theme.accent : theme.border;
            border.w = unlocked ? .42f : .65f;
            canvas->AddRectFilled(at, end, ImGui::GetColorU32(unlocked ? theme.panelAlt : theme.panel), rounding);
            canvas->AddRect(at, end, ImGui::GetColorU32(border), rounding);
            DrawPointsBadge(hidden ? -1 : points, {at.x + padding, at.y + (height - badge) * .5f}, badge, !unlocked);
            const auto textAt = ImVec2{at.x + badge + padding * 2, at.y + (height - titleSize.y - detailHeight) * .5f};
            auto* font = ImGui::GetFont();
            const float size = ImGui::GetFontSize();
            canvas->AddText(font, size, textAt, ImGui::GetColorU32(unlocked ? theme.text : theme.textDisabled), title, nullptr, wrap);
            if (*description)
                canvas->AddText(font, size, {textAt.x, textAt.y + titleSize.y + gap}, ImGui::GetColorU32(theme.textDisabled), description, nullptr, wrap);
        }
#if defined(HELPY_PREVIEW)
        if (unlocked && PreviewUnlocked++ == 0) PreviewEarnedRow = HelpyPreviewHooks::LastRect();
        if (!unlocked) {
            ++PreviewLocked;
            if (hidden && PreviewHidden++ == 0) PreviewHiddenRow = HelpyPreviewHooks::LastRect();
            if (!hidden && PreviewLocked - PreviewHidden == 1) PreviewLockedRow = HelpyPreviewHooks::LastRect();
        }
#endif
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + theme.X(410));
            if (unlocked) {
                const auto date = Date(unlocked, "%Y-%m-%d %H:%M");
                if (date.empty()) ImGui::TextUnformatted("Unlock date unavailable.");
                else ImGui::Text("Unlocked %s", date.c_str());
#if defined(HELPY_PREVIEW)
                PreviewTooltip = date;
#endif
            } else if (hidden) {
                ImGui::TextUnformatted("Requirements are hidden until this achievement is unlocked.");
#if defined(HELPY_PREVIEW)
                PreviewTooltip = "hidden";
#endif
            } else if (definition) {
                Requirements(*definition);
#if defined(HELPY_PREVIEW)
                PreviewTooltip = definition->Id;
#endif
            }
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::Spacing();
    };
    bool hasEarned = false;
    const auto prefix = owner + "|";
    for (auto it = progress.Entries.lower_bound(prefix); it != progress.Entries.end() && it->first.starts_with(prefix); ++it) {
        const auto& [key, entry] = *it;
        if (!entry.Unlocked) continue;
        if (!hasEarned) { ImGui::SeparatorText("Unlocked"); hasEarned = true; }
        const std::string_view id(key.data() + prefix.size(), key.size() - prefix.size());
        const auto definition = std::find_if(definitions.begin(), definitions.end(), [&](const auto& def) { return def.Id == id; });
        draw(entry.Title.c_str(), entry.Points, definition != definitions.end() ? &*definition : nullptr, entry.Unlocked);
    }
    bool hasLocked = false;
    for (const auto& definition : definitions) {
        if (progress.Unlocked(definition.Key())) continue;
        if (!hasLocked) { ImGui::SeparatorText("Locked"); hasLocked = true; }
        draw(definition.Title.c_str(), definition.Points, &definition, 0);
    }
    return hasEarned || hasLocked;
}
inline void RenderModWindow() {
    const bool all = ModOwner.empty();
    const auto* name = all ? "All achievements###mod-achievements" : "Mod achievements###mod-achievements";
    if (ModRead.valid() && ModRead.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try { ModData = ModRead.get(); }
        catch (const std::exception& error) { ModData.Errors[ModOwner] = error.what(); }
    }
    if (!ModRequest.is_null() && !ModRead.valid()) {
        ImGui::OpenPopup(name);
        ModData = {};
        auto rows = std::move(ModRequest);
        ModRequest = nullptr;
        try {
            ModRead = std::async(std::launch::async, [rows = std::move(rows)] {
                return HelpyAchievements::Read(rows, true, false);
            });
        } catch (const std::exception& error) { ModData.Errors[ModOwner] = error.what(); }
    }
    if (!ImGui::IsPopupOpen(name)) return;

    const auto& theme = HelpyImGuiTheme::Current;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(theme.X(570), display.x * .92f),
        std::min(theme.Y(520), display.y * .90f)}, ImGuiCond_Appearing);
    bool open = true;
    if (!HelpyImGuiTheme::BeginCenteredModal(name, &open, ImGuiWindowFlags_NoSavedSettings)) return;
#if defined(HELPY_PREVIEW)
    PreviewModBounds = HelpyPreviewHooks::WindowRect();
    PreviewUnlocked = PreviewLocked = PreviewHidden = 0;
    PreviewEarnedRow = PreviewLockedRow = PreviewHiddenRow = {};
    PreviewTooltip.clear();
    PreviewBadgeValues.clear();
#endif
    if (HelpyAchievements::ProgressLoaded) {
        const auto at = ImGui::GetCursorScreenPos();
        const float size = theme.Y(48);
        ImGui::Dummy({size, size});
        DrawPointsBadge(HelpyAchievements::Progress.Points(), at, size, false, true);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Achievement points");
        if (!all) ImGui::TextDisabled("%s", ModTitle.c_str());
        ImGui::EndGroup();
        ImGui::Separator();
    } else if (!all) ImGui::TextWrapped("%s", ModTitle.c_str());
    if (HelpyImGuiTheme::BeginScroll("achievement-list", {0, -ImGui::GetFrameHeightWithSpacing()})) {
        if (ModRead.valid()) ImGui::TextDisabled("Reading achievements...");
        else if (!HelpyAchievements::ProgressLoaded) ImGui::TextDisabled("Achievement progress is unavailable.");
        else if (all) {
            const auto& definitions = ModData.Definitions;
            if (definitions.empty() && ModData.Errors.empty()) ImGui::TextDisabled("No installed mods define achievements.");
            // Read appends each mod's definitions together.
            for (auto first = definitions.begin(); first != definitions.end();) {
                const auto last = std::find_if(first, definitions.end(), [&](const auto& def) { return def.Owner != first->Owner; });
                ImGui::TextWrapped("%s", first->ModName.c_str());
                RenderModGroup(first->Owner, {first, last});
                ImGui::Spacing();
                first = last;
            }
            for (const auto& [owner, error] : ModData.Errors) ImGui::TextWrapped("%s: %s", owner.c_str(), error.c_str());
        } else {
            const bool hasRows = RenderModGroup(ModOwner, ModData.Definitions);
            if (const auto error = ModData.Errors.find(ModOwner); error != ModData.Errors.end())
                ImGui::TextWrapped("Could not read achievements. %s", error->second.c_str());
            else if (!hasRows) ImGui::TextDisabled("This mod has no achievements.");
        }
    }
    HelpyImGuiTheme::EndScroll(theme);
    if (HelpyImGuiTheme::SoftButton("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
inline void CloseModWindow() {
    if (ModRead.valid()) try { (void)ModRead.get(); } catch (...) {}
    ModRequest = nullptr;
}
// Another player's shared summary (Player profile window).
inline void RenderShared(const nlohmann::json& summary) {
    if (summary.is_null() || !summary.is_object()) return;
    const auto& t = HelpyImGuiTheme::Current;
    ImGui::SeparatorText("Mod achievements");
    ImGui::TextColored(t.accent, "%d points", summary.value("Points", 0)); ImGui::SameLine(); ImGui::TextDisabled("\xC2\xB7 %d unlocked", summary.value("Unlocked", 0));
    for (const auto& row : summary.value("Mods", nlohmann::json::array()))
        ImGui::TextDisabled("%s: %d pts (%d)", row.value("Name", std::string{}).c_str(), row.value("Points", 0), row.value("Unlocked", 0));
    const auto recent = summary.value("Recent", nlohmann::json::array());
    if (!recent.empty()) { ImGui::TextUnformatted("Recently unlocked"); for (const auto& row : recent) ImGui::BulletText("%s (+%d, %s)", row.value("Title", std::string{}).c_str(), row.value("Points", 0), row.value("Mod", std::string{}).c_str()); }
    ImGui::TextDisabled("Reported by this player's Helpy; not verified by the server.");
}
}
