#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace PS::JournalSections {
inline constexpr const char* Names[]{"All entries","World","Recipes","Knowledge"};
inline std::string For(const nlohmann::json& row){
    const auto section=row.value("Section",std::string{});if(section=="World"||section=="Recipes"||section=="Knowledge")return section;
    const auto type=row.value("Type",std::string{});if(type=="World")return "World";if(type=="Recipe")return "Recipes";
    if(type=="Lore"||type=="People"||type=="Place"||type=="Treasure"||type=="Tutorial")return "Knowledge";
    return {}; // No classification from title/icon/owner guesses.
}
}
