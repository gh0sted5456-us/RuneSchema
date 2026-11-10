#pragma once
#include "Generator/HelpyMenu.h"
#include "Generator/RuneSchemaSettingsPanel.h"
#include "Generator/ToolRequest.h"

namespace PS::MultiplayerSettingsPanel {
using Json = nlohmann::json;
inline Json Snapshot = Json::object();
inline std::string Selected, Status;
inline uint64_t Generation = ~uint64_t{};
inline bool UseRange = false, UseFrequency = false;
inline double Range = 100, Frequency = 10;

inline void Select(const Json& row) {
    Selected = row.at("Key").get<std::string>();
    const auto& rule = row.at("Rule");
    UseRange = rule.contains("RangeMeters");
    UseFrequency = rule.contains("UpdatesPerSecond");
    Range = rule.value("RangeMeters", 100.0);
    Frequency = rule.value("UpdatesPerSecond", 10.0);
}

inline bool Call(Json request, Json& reply) {
    request["Action"] = "MultiplayerTuning";
    try {
        if (RuneSchemaSettingsPanel::Remote && RuneSchemaSettingsPanel::Remote(request, reply)) return true;
    } catch (const std::exception& error) { Status = error.what(); return false; }
    Status = "Connect Helpy to a RuneSchema host with the multiplayer plugin.";
    return false;
}

inline void Read() {
    Status.clear();
    Json reply;
    if (!Call({{"Operation", "Read"}}, reply)) { Snapshot = Json::object(); return; }
    Snapshot = std::move(reply);
    Status = Snapshot.value("Status", std::string{});
    const auto rows = Snapshot.value("Rows", Json::array());
    const auto row = std::find_if(rows.begin(), rows.end(), [](const Json& value) {
        return value.at("Key").get<std::string>() == Selected;
    });
    if (row != rows.end()) Select(*row);
    else if (!rows.empty()) Select(rows.front());
    else Selected.clear();
}

inline void Save(bool reset) {
    Json request = {{"Operation", reset ? "Reset" : "Set"}, {"Key", Selected},
        {"World", Snapshot.at("World")}, {"Revision", Snapshot.at("Revision")}};
    if (!reset) {
        auto& rule = request["Rule"] = Json::object();
        if (UseRange) rule["RangeMeters"] = Range;
        if (UseFrequency) rule["UpdatesPerSecond"] = Frequency;
    }
    Json reply;
    if (!Call(request, reply)) return;
    if (!reply.value("Ok", false)) { Status = reply.value("Status", std::string("Settings could not be saved.")); return; }
    Read();
    if (Status.empty()) Status = "Saved. Refresh to check how many live actors accepted the settings.";
}

inline void Render() {
    const auto current = SpawnToolRequests::CurrentGeneration();
    if (Generation != current) {
        Generation = current;
        Selected.clear();
        Read();
    }
    ImGui::SeparatorText("Mod replication");
    ImGui::TextWrapped("Set replication range and update rate for actor classes declared by enabled mods. Changes apply on the host.");
    HelpyMenu::NextControl("Refresh multiplayer settings");
    if (HelpyImGuiTheme::SoftButton("Refresh")) Read();
    if (!Status.empty()) ImGui::TextWrapped("%s", Status.c_str());
    if (!Snapshot.value("Available", false)) return;
    const auto& rows = Snapshot.at("Rows");
    if (rows.empty()) { ImGui::TextDisabled("No enabled mods have declared a multiplayer actor class."); return; }
    HelpyMenu::NextControl("Multiplayer actor class");
    if (ImGui::BeginCombo("##multiplayer-class", Selected.c_str())) {
        for (const auto& row : rows) {
            const auto key = row.at("Key").get<std::string>();
            if (ImGui::Selectable(key.c_str(), key == Selected)) Select(row);
        }
        ImGui::EndCombo();
    }
    const auto row = std::find_if(rows.begin(), rows.end(), [](const Json& value) {
        return value.at("Key").get<std::string>() == Selected;
    });
    if (row == rows.end()) return;
    ImGui::TextWrapped("%s", row->at("Class").get<std::string>().c_str());
    ImGui::TextDisabled("%zu applied, %zu skipped", row->value("Applied", std::size_t{}), row->value("Skipped", std::size_t{}));
    if (Snapshot.value("Pending", false)) ImGui::TextDisabled("Applying on the host");
    if (const auto detail = row->value("Detail", std::string{}); !detail.empty()) ImGui::TextWrapped("%s", detail.c_str());
    ImGui::TextDisabled("Applied means the actor fields were verified on the host.");
    const bool manage = Snapshot.value("CanManage", false);
    if (!manage) ImGui::TextDisabled("Open Helpy on the multiplayer host to make changes.");
    ImGui::BeginDisabled(!manage);
    HelpyMenu::NextControl("Override replication range");
    HelpyImGuiTheme::Toggle("Override replication range", &UseRange);
    ImGui::BeginDisabled(!UseRange);
    HelpyMenu::NextControl("Range in meters");
    ImGui::InputDouble("Range (m)", &Range, 0, 0, "%.1f");
    ImGui::EndDisabled();
    HelpyMenu::NextControl("Override update rate");
    HelpyImGuiTheme::Toggle("Override update rate", &UseFrequency);
    ImGui::BeginDisabled(!UseFrequency);
    HelpyMenu::NextControl("Updates per second");
    ImGui::InputDouble("Updates per second", &Frequency, 0, 0, "%.1f");
    ImGui::EndDisabled();
    const bool hasOverride = !row->at("Rule").empty();
    ImGui::BeginDisabled(!UseRange && !UseFrequency);
    HelpyMenu::NextControl("Apply replication settings");
    if (HelpyImGuiTheme::SoftButton("Apply")) Save(false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasOverride);
    HelpyMenu::NextControl("Restore engine defaults");
    if (HelpyImGuiTheme::SoftButton("Restore defaults")) Save(true);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
}
}
