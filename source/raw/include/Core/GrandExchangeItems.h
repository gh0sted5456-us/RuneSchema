#pragma once
#include "Core/ItemDelivery.h"
#include <stdexcept>
#include <utility>
namespace PS::GrandExchangeItems {
inline std::vector<std::pair<std::string,std::string>> Icons(const nlohmann::json& index) {
    if(!index.is_object()||index.value("schema",std::string{})!="grandexchange.items.index/1"||
        !index.contains("items")||!index["items"].is_array()||index["items"].size()>32768)
        throw std::runtime_error("Invalid Grand Exchange public item index");
    std::vector<std::pair<std::string,std::string>> icons;
    std::set<std::pair<std::string,std::string>> seen;
    for(const auto& row:index["items"]) {
        if(!row.is_object()||!row.contains("path")||!row["path"].is_string()||!row.contains("icon")||!row["icon"].is_string())continue;
        const auto path=row["path"].get<std::string>(),icon=row["icon"].get<std::string>();
        if(ItemDelivery::AssetPath(path)&&ItemDelivery::AssetPath(icon)&&seen.emplace(path,icon).second)icons.emplace_back(path,icon);
    }
    return icons;
}
}
