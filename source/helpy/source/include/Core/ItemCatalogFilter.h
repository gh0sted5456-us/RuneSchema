#pragma once
#include "Core/ItemSource.h"
#include "Core/ItemCategory.h"
#include "Core/ItemFamily.h"
#include "Core/LoadoutEligibility.h"
#include "Core/ItemSearchExpression.h"
#include <sstream>
#include <vector>
namespace PS::ItemCatalogFilter {
inline std::string Searchable(const nlohmann::json& row,const std::string& category){
    return HelpyCacheSources::Lower(ItemCategory::Text(row,"Path")+" "+ItemCategory::Text(row,"Name")+" "+ItemCategory::Text(row,"InternalName")+" "+ItemCategory::Text(row,"PersistenceID")+" "+ItemCategory::Text(row,"Tags")+" "+category+" "+ItemCategory::Text(row,"Class"));
}
struct Query {
    ItemSource::Kind Source=ItemSource::Kind::All;std::string Category,Family,Slot,Provider;
    // Words: terms every match contains (all terms of a plain AND query). Index prefilters
    // narrow by these and may clear them; a boolean query still applies Text in full.
    std::vector<std::string> Words;ItemSearchExpression::Expression Text;
    explicit Query(std::string text){
        if(text.size()>512)throw std::runtime_error("Catalog query and filter controls exceed 512 characters.");
        constexpr std::string_view slotPrefix="__helpyslot:";
        if(text.starts_with(slotPrefix)){const auto split=text.find(' ');Slot=text.substr(slotPrefix.size(),split==text.npos?text.npos:split-slotPrefix.size());if(!LoadoutEligibility::Valid(Slot))throw std::runtime_error("Invalid native loadout slot filter");text=split==text.npos?std::string{}:text.substr(split+1);}
        Source=ItemSource::TakeFilter(text);
        Provider=ItemSource::TakeProvider(text);
        constexpr std::string_view prefix="__helpycat:";
        if(text.starts_with(prefix)){const auto split=text.find(' ');Category=HelpyCacheSources::Lower(text.substr(prefix.size(),split==text.npos?text.npos:split-prefix.size()));text=split==text.npos?std::string{}:text.substr(split+1);}
        Family=ItemFamily::TakeFilter(text);if(text.size()>256)throw std::runtime_error("Catalog search text exceeds 256 characters.");
        Text=ItemSearchExpression::Expression::Parse(text,[](std::string word){return HelpyCacheSources::Lower(word);});
        Words=Text.Required();
    }
    // A boolean query can have no required terms ("-rusty") and still filter.
    bool HasText()const{return !Text.Empty();}
    bool Boolean()const{return !Text.Plain();}
    bool operator()(const nlohmann::json& row)const{
        if(!row.is_object())return false;
        if(!Slot.empty()&&!LoadoutEligibility::Matches(row,Slot))return false;
        const auto category=Category.empty()&&Words.empty()&&!Boolean()?std::string{}:ItemCategory::Resolve(row);
        if(((Source!=ItemSource::Kind::All||!Provider.empty())&&!ItemSource::Matches(Source,ItemSource::Row(row),Provider))||(!Category.empty()&&HelpyCacheSources::Lower(category)!=Category)||!ItemFamily::MatchesRow(Family,row))return false;
        if(Boolean())return Text.Matches(Searchable(row,category));
        if(Words.empty())return true;
        const auto searchable=Searchable(row,category);
        for(const auto& word:Words)if(searchable.find(word)==searchable.npos)return false;return true;
    }
};
}
