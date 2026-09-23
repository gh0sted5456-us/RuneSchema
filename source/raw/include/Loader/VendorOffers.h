#pragma once
#include <cstdint>
#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>
#include "Loader/VendorCategoryLabel.h"
#include "Loader/TimeOfDay.h"

namespace DragonWilds::VendorOffers {
inline int Order(const nlohmann::json& item,int fallback) {
    if(!item.contains("Order"))return fallback;
    const auto& value=item.at("Order");
    if(!value.is_number_integer() || value.get<int64_t>()<0 || value.get<int64_t>()>1000000)
        throw std::runtime_error("Vendor offer Order must be an integer from 0 to 1000000");
    return value.get<int>();
}
// Stable per-mod/per-vendor identities; no std::hash implementation dependence.
// These are identifiers, not cryptographic hashes. Runtime ownership checks
// reject collisions instead of replacing someone else's recipe or merchant row.
inline std::string Identity(std::string_view owner, std::string_view slot) {
    uint64_t a=14695981039346656037ull, b=7809847782465536322ull;
    const auto feed=[&](unsigned char c){a=(a^c)*1099511628211ull;b=(b^c)*14029467366897019727ull;};
    for(unsigned char c:owner)feed(c);
    feed(0);
    for(unsigned char c:slot)feed(c);
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string result;unsigned bits=0;uint32_t buffer=0;
    for(unsigned i=0;i<16;++i) {
        const auto byte=static_cast<unsigned char>((i<8?a:b)>>((i%8)*8));
        buffer=(buffer<<8)|byte;bits+=8;
        while(bits>=6){bits-=6;result+=alphabet[(buffer>>bits)&63];}
    }
    if(bits)result+=alphabet[(buffer<<(6-bits))&63];
    return result;
}
inline std::string Owner(const std::string& mod,const std::string& id) {
    return std::to_string(mod.size())+":"+mod+":"+id;
}
inline std::string RecipeObjectName(std::string_view identity) {
    if(identity.size()!=22 || identity.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")!=std::string_view::npos)
        throw std::runtime_error("Invalid vendor recipe identity");
    return "RSVendor_"+std::string(identity)+"_Runtime";
}
inline nlohmann::json Properties(const nlohmann::json& item) {
    using nlohmann::json;
    if(!item.is_object())throw std::runtime_error("Each vendor offer must be an object");
    (void)Order(item,0);
    const auto path=[&](const char* field) {
        if(!item.contains(field) || !item[field].is_string())throw std::runtime_error(std::string("Vendor offer requires ")+field);
        const auto value=item[field].get<std::string>();
        if(value.empty() || value.front()!='/' || value.size()>1024 || value.find_first_of("\r\n\t")!=std::string::npos)
            throw std::runtime_error(std::string("Invalid vendor offer asset path: ")+field);
        return value;
    };
    const auto number=[&](const char* field,int fallback,int minimum) {
        if(!item.contains(field))return fallback;
        if(!item[field].is_number_integer())throw std::runtime_error(std::string("Vendor offer requires integer ")+field);
        const auto value=item[field].get<double>();
        if(value<minimum || value>std::numeric_limits<int32_t>::max())throw std::runtime_error(std::string("Vendor offer out of range: ")+field);
        return item[field].get<int32_t>();
    };
    if(!item.contains("Price"))throw std::runtime_error("Vendor offer requires Price");
    for(const auto* gate:{"MinPowerLevel","MaxPowerLevel"})if(item.contains(gate)) {
        if(!item.at(gate).is_number_integer() || item.at(gate).get<int>()<0 || item.at(gate).get<int>()>100)
            throw std::runtime_error(std::string("Vendor offer ")+gate+" must be an integer from 0 to 100");
    }
    if(item.contains("MinPowerLevel") && item.contains("MaxPowerLevel") && item.at("MinPowerLevel").get<int>()>item.at("MaxPowerLevel").get<int>())
        throw std::runtime_error("Vendor offer MinPowerLevel cannot exceed MaxPowerLevel");
    if(item.contains("TimeOfDay")) {
        if(!item.at("TimeOfDay").is_string())throw std::runtime_error("Vendor offer TimeOfDay must be Any, Day, or Night");
        (void)TimeOfDay::Parse(item.at("TimeOfDay").get<std::string>());
    }
    if(item.contains("QuestCompleted") && (!item.at("QuestCompleted").is_string() || item.at("QuestCompleted").get<std::string>().empty()
        || item.at("QuestCompleted").get<std::string>().size()>512))throw std::runtime_error("Vendor offer QuestCompleted must be a quest ID");
    const auto count=number("Count",1,1), price=number("Price",0,0);
    const auto product=path("Item"), currency=path("Currency");
    return {{"ItemsCreated",json::array({json{{"ItemData",product},{"Count",count}}})},
        {"ItemsConsumed",json::array({json{{"ItemData",currency},{"Count",price}}})},
        {"bIgnoreNotification",true}};
}
inline std::string Category(const nlohmann::json& item) {
    if (item.contains("Category") && !item.at("Category").is_string())
        throw std::runtime_error("Vendor Category must be a string");
    return VendorCategoryLabel::Validate(item.value("Category", std::string("Items")));
}
inline nlohmann::json OrderedWithinCategories(const nlohmann::json& source) {
    using Json=nlohmann::json;
    struct Row {Json Value;std::size_t CategoryIndex,SourceIndex;int Order;};
    std::vector<Row> rows;std::map<std::string,std::size_t> categoryIndices;
    for(std::size_t i=0;i<source.size();++i) {
        const auto& item=source.at(i);const auto category=Category(item);
        auto [found,inserted]=categoryIndices.emplace(category,categoryIndices.size());
        rows.push_back({item,found->second,i,Order(item,static_cast<int>(i))});
    }
    std::stable_sort(rows.begin(),rows.end(),[](const Row& left,const Row& right) {
        if(left.CategoryIndex!=right.CategoryIndex)return left.CategoryIndex<right.CategoryIndex;
        if(left.Order!=right.Order)return left.Order<right.Order;
        return left.SourceIndex<right.SourceIndex;
    });
    Json result=Json::array();for(auto& row:rows)result.push_back(std::move(row.Value));return result;
}
}
