#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <sstream>
#include <cctype>
namespace PS::SpawnCatalogSearch {
inline std::string Fold(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
inline std::string Text(const nlohmann::json& row,const char* field){const auto found=row.find(field);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};}
inline std::string SearchText(const nlohmann::json& row){
    auto text=Fold(Text(row,"Key")+" "+Text(row,"Name")+" "+Text(row,"Class")+" "+Text(row,"Type")+" "+Text(row,"ModId")+" "+Text(row,"ResourceFamily"));
    const auto type=Text(row,"Type");
    if(type=="Resource"){
        text+=" resource node actor";
        if(text.find("mineral")!=text.npos||text.find("mining")!=text.npos||text.find("ore")!=text.npos||text.find("rock")!=text.npos)text+=" ore mineral mining rock node";
        if(text.find("tree")!=text.npos||text.find("log")!=text.npos||text.find("sapling")!=text.npos)text+=" tree wood resource";
    }else if(type=="AI")text+=" ai enemy enemies creature actor";
    return text;
}
inline bool Matches(const nlohmann::json& row,std::string query){
    const auto text=SearchText(row);query=Fold(std::move(query));for(auto& c:query)if(!std::isalnum(static_cast<unsigned char>(c)))c=' ';
    std::istringstream words(query);std::string word;while(words>>word){if(word.size()>3&&word.back()=='s')word.pop_back();if(text.find(word)==text.npos)return false;}return true;
}
}
