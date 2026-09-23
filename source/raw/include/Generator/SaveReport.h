#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

namespace PS::SaveReport {
    inline std::string Guid(std::string text) {
        std::string result;
        for(unsigned char c:text) {
            if(c=='-' || c=='{' || c=='}')continue;
            if(c>='a' && c<='f')c-=32;
            if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return {};
            result+=static_cast<char>(c);
        }
        return result.size()==32 && result.find_first_not_of('0')!=result.npos?result:std::string{};
    }
    inline nlohmann::json Inventory(const nlohmann::json& save) {
        using nlohmann::json;
        if(!save.is_object() || !save.contains("GameProgress") || !save["GameProgress"].is_object())
            throw std::runtime_error("Unsupported character-save layout.");
        json rows=json::array();
        const auto& progress=save["GameProgress"];
        for(const auto* section:{"Inventory","PersonalInventory"}) {
            if(!progress.contains(section))continue;
            if(!progress[section].is_object())throw std::runtime_error("Unsupported inventory layout.");
            for(const auto& [slot,item]:progress[section].items()) {
                if(rows.size()>=4096)throw std::runtime_error("Saved inventory exceeds 4096 entries.");
                if(!item.is_object() || !item.contains("ItemData") || !item["ItemData"].is_string())continue;
                rows.push_back({{"Location",std::string(section)+"/"+slot},{"PersistenceID",item["ItemData"]},
                    {"DisplayName",nullptr},{"InternalName",nullptr},{"AssetPath",nullptr},{"Origin","Unresolved"}});
            }
        }
        return rows;
    }
    inline std::string Origin(const std::string& path) {
        if(path.starts_with("/Game/RuneSchema/"))return "RuneSchema namespace";
        if(path.starts_with("/Game/Mods/"))return "Mod namespace (pak origin unverified)";
        return "Other loaded asset (origin unverified)";
    }
}
