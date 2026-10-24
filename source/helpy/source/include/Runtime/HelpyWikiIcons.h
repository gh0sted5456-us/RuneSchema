#pragma once
#include <imgui.h>
#include <string>
#include <vector>
namespace PS::HelpyWikiIcons {
void Initialize(); // Local files only. Network requires explicit opt-in.
bool Start(const std::vector<std::string>& names,const std::string& build);
void Poll();
bool Busy();
const std::string& Status();
bool Draw(const std::string& name,ImVec2 minimum,ImVec2 maximum,ImU32 tint=IM_COL32_WHITE,bool circle=false);
std::vector<std::string> Portraits();
std::vector<std::string> Names();
std::string Label(const std::string& portrait);
std::string Journal(const std::string& name);
std::string Page(const std::string& name);
void Cancel()noexcept;
void Shutdown()noexcept;
}
