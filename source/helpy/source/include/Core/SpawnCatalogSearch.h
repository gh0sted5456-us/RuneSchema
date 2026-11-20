#pragma once
#include "Core/ItemSearchExpression.h"
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
// Lower case, punctuation as spaces, one space between words, and a plural "s" dropped from
// words longer than three letters. Applied to query terms and, for boolean queries, the text.
inline std::string Words(std::string value){
    value=Fold(std::move(value));for(auto& c:value)if(!std::isalnum(static_cast<unsigned char>(c)))c=' ';
    std::istringstream words(value);std::string word,out;
    while(words>>word){if(word.size()>3&&word.back()=='s')word.pop_back();if(!out.empty())out+=' ';out+=word;}
    return out;
}
inline constexpr const char* SyntaxHelp="Words must all match. OR (or |) matches either side, -word or NOT excludes, ( ) groups, \"quotes\" match an exact phrase.\nExample: (goblin OR wolf) -archer";
// Plain words keep the original search: every word, anywhere in the row. Operators and quotes
// use the Item Bay's boolean syntax: OR or |, -word or NOT, ( ) groups, "exact phrase".
struct Query {
    explicit Query(std::string_view text)
        :Expression(PS::ItemSearchExpression::Expression::Parse(text,[](std::string term){return Words(std::move(term));})),
         Boolean(!Expression.Plain()||text.find('"')!=std::string_view::npos){
        if(!Boolean){std::istringstream words(Words(std::string(text)));std::string word;while(words>>word)Plain.push_back(std::move(word));}
    }
    bool Empty()const{return Boolean?Expression.Empty():Plain.empty();}
    bool operator()(const nlohmann::json& row)const{
        if(!Boolean){const auto text=SearchText(row);for(const auto& word:Plain)if(text.find(word)==text.npos)return false;return true;}
        return Expression.Matches(" "+Words(SearchText(row))+" ");
    }
    PS::ItemSearchExpression::Expression Expression;bool Boolean=false;std::vector<std::string> Plain;
};
inline bool Matches(const nlohmann::json& row,std::string query){return Query(query)(row);}
}
