#pragma once
#include "Core/HelpyPersonalization.h"
#include <vector>

namespace PS::HelpyScrap {
using Json=nlohmann::json;
struct Amount {std::string Path;int64_t Count=0;};
struct Recipe {
    std::string Path;std::vector<Amount> Inputs,Outputs;
    // Set only by the live recipe reader after excluding alternative/random
    // outputs, dynamic quantity modifiers and unverified ingredient records.
    bool Deterministic=false;
};
inline bool Path(const std::string& value){return value.size()<=4096&&value.starts_with('/')&&!value.starts_with("//")
    &&value.find("..")==value.npos&&value.find_first_of("\\:")==value.npos&&HelpyPersonalization::Text(value,4096);}
inline void Amounts(const std::vector<Amount>& values){
    if(values.empty()||values.size()>32)throw std::runtime_error("Recipe must contain 1..32 distinct item types");
    std::set<std::string> seen;for(const auto& value:values)if(!Path(value.Path)||value.Count<1||value.Count>1000000
        ||!seen.insert(HelpyPersonalization::Fold(value.Path)).second)throw std::runtime_error("Invalid native recipe quantities");
}
// This is Helpy's inverse-recipe calculation, not a native Scrap function.
// Only an authoritative adapter may consume the item and deliver these refunds.
inline Json Plan(const Json& item,int64_t quantity,const std::vector<Recipe>& liveRecipes){
    if(HelpyPersonalization::StackKey(item).empty()||!item.contains("Count")||!item["Count"].is_number_integer()
        ||quantity<1||quantity>1000000||item["Count"].get<int64_t>()<quantity||item["Count"].get<int64_t>()>1000000)
        throw std::runtime_error("Select a current character item stack and valid scrap quantity");
    if(item.value("Equipped",false)||HelpyPersonalization::Favorite(item)||HelpyPersonalization::Locked(item))
        throw std::runtime_error("Unequip or unprotect this item before scrapping");
    if(liveRecipes.size()>256)throw std::runtime_error("Too many candidate crafting recipes");
    const auto path=item.at("Path").get<std::string>(),folded=HelpyPersonalization::Fold(path);
    if(!Path(path))throw std::runtime_error("Invalid scrap item path");
    const Recipe* selected=nullptr;
    for(const auto& recipe:liveRecipes){
        if(recipe.Outputs.size()>32)throw std::runtime_error("Native recipe output bound exceeded");
        bool match=false;for(const auto& output:recipe.Outputs)if(HelpyPersonalization::Fold(output.Path)==folded)match=true;
        if(!match)continue;
        if(selected)throw std::runtime_error("Multiple crafting recipes match; no scrap was attempted");
        selected=&recipe;
    }
    if(!selected||!selected->Deterministic||!Path(selected->Path)||selected->Outputs.size()!=1)
        throw std::runtime_error("No unambiguous deterministic crafting refund is available");
    Amounts(selected->Inputs);Amounts(selected->Outputs);
    const auto outputCount=selected->Outputs[0].Count;
    if(quantity%outputCount)throw std::runtime_error("Scrap a whole crafting batch to refund exact components");
    const auto batches=quantity/outputCount;auto refunds=Json::array();
    for(const auto& input:selected->Inputs){
        if(HelpyPersonalization::Fold(input.Path)==folded||input.Count>1000000/batches)
            throw std::runtime_error("Recipe refund would be cyclic or exceed the quantity bound");
        refunds.push_back({{"Path",input.Path},{"Count",input.Count*batches}});
    }
    return {{"PlayerGuid",item.at("PlayerGuid")},{"Instance",item.at("Instance")},{"Path",path},{"Quantity",quantity},
        {"Recipe",selected->Path},{"Refunds",refunds},{"Delivery","WorldDrops"}};
}
}
