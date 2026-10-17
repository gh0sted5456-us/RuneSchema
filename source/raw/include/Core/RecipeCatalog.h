#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace PS::RecipeCatalog {
inline constexpr std::size_t MaxEntries=1048576,PageSize=128;
using Json=nlohmann::json;
struct Entry {std::string Path,Name,Owner,Reference,Source;bool Available=true;};
using Index=std::map<std::string,Entry>;
inline std::string Fold(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
inline void Add(Index& index,Entry entry){
    if(entry.Path.empty()&&entry.Reference.empty())return;
    const auto key=entry.Path.empty()?"!definition:"+entry.Reference:entry.Path;
    const auto found=index.find(key);
    if(found==index.end()){if(index.size()>=MaxEntries)throw std::runtime_error("Recipe catalogue exceeds its metadata safety bound");index.emplace(key,std::move(entry));return;}
    auto& prior=found->second;
    if(!entry.Owner.empty())prior.Owner=entry.Owner;if(!entry.Reference.empty())prior.Reference=entry.Reference;
    if(!entry.Source.empty()&&entry.Source!="Cooked candidate")prior.Source=entry.Source;
    prior.Available=entry.Source=="RuneSchema definition"?entry.Available:prior.Available||entry.Available;
}
inline Json Page(const Index& index,const std::string& query,std::size_t offset){
    if(offset>MaxEntries||query.size()>256)throw std::runtime_error("Invalid recipe catalogue page");
    const auto filter=Fold(query);Json rows=Json::array();std::size_t matches=0;
    for(const auto& [key,entry]:index){
        if(!filter.empty()&&Fold(entry.Path+" "+entry.Name+" "+entry.Owner+" "+entry.Reference).find(filter)==std::string::npos)continue;
        if(matches++<offset||rows.size()>=PageSize)continue;
        rows.push_back({{"Path",entry.Path},{"Name",entry.Name},{"Owner",entry.Owner},{"Reference",entry.Reference},{"Source",entry.Source},{"Available",entry.Available}});
    }
    return {{"Recipes",rows},{"Offset",offset},{"Total",matches},{"PageSize",PageSize},{"HasMore",offset+rows.size()<matches}};
}
}
