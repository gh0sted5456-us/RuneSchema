#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <cctype>
namespace PS::JournalPlacement {
struct Group { std::string Id, DisplayName; bool CreateIfMissing=false; };
struct Placement { std::string SubCategory, Key; std::optional<Group> TargetGroup; };
inline std::string RequiredString(const nlohmann::json& object,const char* field) {
    if(!object.contains(field) || !object.at(field).is_string())
        throw std::runtime_error(std::string("Journal placement requires string ")+field);
    const auto value=object.at(field).get<std::string>();
    if(value.empty() || value.find_first_not_of(" \t\r\n")==std::string::npos)
        throw std::runtime_error(std::string("Journal placement has empty ")+field);
    return value;
}
inline std::string FriendlyKey(std::string value) {
    std::string result;
    result.reserve(value.size());
    for(unsigned char c:value)if(std::isalnum(c))result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}
inline std::string NativeSubCategory(const std::string& section,const std::string& category) {
    struct Route {const char* Section;const char* Category;const char* Asset;};
    static constexpr std::array Routes{
        Route{"World","Flora","JOURNAL_SC_World_Flora"},
        Route{"World","Fauna","JOURNAL_SC_World_Fauna"},
        Route{"World","Material Sources","JOURNAL_SC_World_Material_Sources"},
        Route{"World","Factions","JOURNAL_SC_World_Factions"},
        Route{"Recipes","Tools","Journal_SC_Recipe_Tools"},
        Route{"Recipes","Weapons","JOURNAL_SC_Recipe_Weapons"},
        Route{"Recipes","Armor","JOURNAL_SC_Recipe_Armor"},
        Route{"Recipes","Ammo & Runes","JOURNAL_SC_Recipe_Ammo_Runes"},
        Route{"Recipes","Trinkets","Journal_SC_Recipe_Trinkets"},
        Route{"Recipes","Masterworks","JOURNAL_SC_Recipe_Masterworks2"},
        Route{"Recipes","Food & Potions","JOURNAL_SC_Recipe_Food_Potions"},
        Route{"Recipes","Basic Materials","Journal_SC_Recipe_Materials_Basic"},
        Route{"Recipes","Processed Materials","Journal_SC_Recipe_Materials_Processed"},
        Route{"Recipes","Special Materials","Journal_SC_Recipe_Materials_Special"},
        Route{"Recipes","Quest Items","Journal_SC_Recipe_Quest_Items"},
        Route{"Recipes","Capes","JOURNAL_SC_Recipe_Capes"},
        Route{"Knowledge","Tutorials","JOURNAL_SC_Know_Tutorial"},
        Route{"Knowledge","Lore Scraps","JOURNAL_SC_Know_LoreScraps"},
        Route{"Knowledge","People","JOURNAL_SC_Know_People"},
        Route{"Knowledge","Places","JOURNAL_SC_Know_Places"}
    };
    const auto requestedSection=FriendlyKey(section),requestedCategory=FriendlyKey(category);
    for(const auto& route:Routes)if(requestedSection==FriendlyKey(route.Section) && requestedCategory==FriendlyKey(route.Category))
        return std::string("/Game/UI/JournalData/")+route.Asset+'.'+route.Asset;
    throw std::runtime_error("Unknown journal Section/Category route: "+section+" / "+category);
}
inline Placement Parse(const nlohmann::json& value,const std::string& defaultKey) {
    if(!value.is_object())throw std::runtime_error("Journal AddTo must be an object");
    for(const auto& [key,unused]:value.items())
        if(key!="Section" && key!="Category" && key!="SubCategory" && key!="Key" && key!="Group")
            throw std::runtime_error("Unknown journal AddTo field: "+key);
    const bool hasPath=value.contains("SubCategory"),hasSection=value.contains("Section"),hasCategory=value.contains("Category");
    if(hasPath && (hasSection || hasCategory))
        throw std::runtime_error("Journal AddTo uses either SubCategory or Section plus Category, not both");
    if(hasSection!=hasCategory)
        throw std::runtime_error("Journal AddTo friendly placement requires both Section and Category");
    if(!hasPath && !hasSection)
        throw std::runtime_error("Journal AddTo requires SubCategory or Section plus Category");
    Placement result{hasPath?RequiredString(value,"SubCategory"):
        NativeSubCategory(RequiredString(value,"Section"),RequiredString(value,"Category")),defaultKey,{}};
    if(value.contains("Key"))result.Key=RequiredString(value,"Key");
    if(result.Key.empty())throw std::runtime_error("Journal placement requires an entry key");
    if(value.contains("Group")) {
        const auto& group=value.at("Group");
        if(!group.is_object())throw std::runtime_error("Journal AddTo.Group must be an object");
        for(const auto& [key,unused]:group.items())
            if(key!="Id" && key!="DisplayName" && key!="CreateIfMissing")
                throw std::runtime_error("Unknown journal group field: "+key);
        Group target{RequiredString(group,"Id"),{},false};
        if(group.contains("CreateIfMissing")) {
            if(!group.at("CreateIfMissing").is_boolean())throw std::runtime_error("CreateIfMissing must be Boolean");
            target.CreateIfMissing=group.at("CreateIfMissing").get<bool>();
        }
        if(group.contains("DisplayName"))target.DisplayName=RequiredString(group,"DisplayName");
        if(target.CreateIfMissing && target.DisplayName.empty())throw std::runtime_error("Creating a journal group requires DisplayName");
        result.TargetGroup=std::move(target);
    }
    return result;
}
}
