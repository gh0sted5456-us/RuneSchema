#pragma once
#include "Generator/ToolRequest.h"
#include <imgui.h>

namespace PS::QuestStatusPanel {
inline void Render()
{
    const auto result=QuestStatusRequests::Read();
    ImGui::TextWrapped("Read-only native quest status. The authority (host or dedicated server) owns counters and save progress; clients display the replicated native state.");
    if(ImGui::Button("Refresh all RuneSchema quest status"))
        QuestStatusRequests::Submit({{"Action","Status"}});
    ImGui::SameLine();ImGui::TextDisabled("Safe: does not start, advance, reset, reward or cancel quests.");
    if(result.contains("Players"))for(const auto& player:result["Players"]) {
        const auto name=player.value("Name",std::string("Player"));
        if(!ImGui::CollapsingHeader(name.c_str()))continue;
        if(!player.contains("Quests"))continue;
        for(const auto& quest:player["Quests"]) {
            ImGui::PushID((player.value("Path",std::string{})+quest.value("Key",std::string{})).c_str());
            const auto title=quest.value("Title",quest.value("Key",std::string{}));
            if(ImGui::TreeNode(title.c_str())) {
                ImGui::Text("State: %s",quest.value("State",std::string("Unavailable")).c_str());
                ImGui::Text("Run: %d",quest.value("Run",0));
                if(quest.contains("Error"))ImGui::TextWrapped("Error: %s",quest["Error"].get_ref<const std::string&>().c_str());
                if(quest.contains("Objectives"))for(const auto& objective:quest["Objectives"])
                    ImGui::BulletText("%s: %d / %d%s",objective.value("Text",objective.value("Id",std::string{})).c_str(),
                        objective.value("Count",0),objective.value("Required",0),objective.value("Hidden",false)?" (hidden)":"");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::TextWrapped("%s",result.value("Status",std::string{}).c_str());
}
}
