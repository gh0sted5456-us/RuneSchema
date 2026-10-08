#pragma once
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>
namespace PS::InspectionTools {
inline nlohmann::json WithDiagnosticOrigin(nlohmann::json report,const nlohmann::json& origin) {
    if(report.is_object() && !report.contains("origin"))report["origin"]=origin;
    return report;
}
inline std::string DiagnosticOriginLabel(const nlohmann::json& report) {
    if(report.is_object() && report.contains("origin") && report["origin"].is_object()) {
        const auto& value=report["origin"];
        if(value.contains("mode") && value["mode"].is_string()) {
            const auto mode=value["mode"].get<std::string>();
            for(const auto* allowed:{"standalone","client","listen-server","dedicated-server"})if(mode==allowed)return mode;
        }
    }
    return "unknown";
}
inline nlohmann::json SearchReport(const std::string& query, const std::string& scope,
    const std::string& loader, const nlohmann::json& results, size_t limit) {
    if (!results.is_array() || !limit || results.size()>limit)
        throw std::runtime_error("Invalid search report bounds.");
    return {{"Kind","RuneSchemaSearch1"},{"Query",query},{"Scope",scope},{"Loader",loader},
        {"Count",results.size()},{"Limit",limit},{"LimitReached",results.size()==limit},
        {"Results",results},{"Coverage","Cached search results, not an installable mod. A reached limit may omit matches. Paths may become unavailable after world changes. Inspect a match and use a supported loader starter to author changes."}};
}
inline std::string DiagnosticExportName(std::string name, const std::string& fallback,
    const std::string& stamp, bool jsonc, std::string category = {}) {
    if (name.empty()) name = fallback;
    if (name.size() > 96 || name.find("..") != std::string::npos)
        throw std::runtime_error("Export name must be at most 96 characters, without '..'.");
    for (const unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' '))
            throw std::runtime_error("Export name may contain letters, numbers, spaces, '-' and '_' only; omit the extension.");
    if(category.empty())category=fallback.empty()?"export":fallback;
    for(char& c:category) {
        if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-' || c=='_'))
            throw std::runtime_error("Invalid export category.");
    }
    if(category.size()>96)throw std::runtime_error("Export category exceeds limit.");
    return name + "_" + category + "_" + stamp + (jsonc ? ".jsonc" : ".json");
}
}
