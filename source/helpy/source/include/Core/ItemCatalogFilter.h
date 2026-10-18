#pragma once
#include "Core/ItemSource.h"
#include "Core/ItemFamily.h"
#include <sstream>
#include <vector>
namespace PS::ItemCatalogFilter {
struct Query {
    ItemSource::Kind Source=ItemSource::Kind::All;std::string Category,Family;std::vector<std::string> Words;
    explicit Query(std::string text){
        if(text.size()>512)throw std::runtime_error("Catalog query and filter controls exceed 512 characters.");
        Source=ItemSource::TakeFilter(text);
        constexpr std::string_view prefix="__helpycat:";
        if(text.starts_with(prefix)){const auto split=text.find(' ');Category=HelpyCacheSources::Lower(text.substr(prefix.size(),split==text.npos?text.npos:split-prefix.size()));text=split==text.npos?std::string{}:text.substr(split+1);}
        Family=ItemFamily::TakeFilter(text);if(text.size()>256)throw std::runtime_error("Catalog search text exceeds 256 characters.");
        std::istringstream input(text);std::string word;while(input>>word)Words.push_back(HelpyCacheSources::Lower(word));
    }
    bool operator()(const nlohmann::json& row)const{
        if(!row.is_object())return false;
        const auto text=[&](const char* key){const auto field=row.find(key);return field!=row.end()&&field->is_string()?field->get<std::string>():std::string{};};
        if(!ItemSource::Matches(Source,ItemSource::Row(row))||(!Category.empty()&&HelpyCacheSources::Lower(text("Category"))!=Category)||!ItemFamily::Matches(Family,text("Name")))return false;
        if(Words.empty())return true;
        const auto searchable=HelpyCacheSources::Lower(text("Path")+" "+text("Name")+" "+text("InternalName")+" "+text("PersistenceID")+" "+text("Tags")+" "+text("Category")+" "+text("Class"));
        for(const auto& word:Words)if(searchable.find(word)==searchable.npos)return false;return true;
    }
};
}
