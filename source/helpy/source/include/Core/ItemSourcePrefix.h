#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
// Typed source scoping for item searches: "source:Name" (or "src:Name", quoted when the
// name has spaces) picks the same source as the Source dropdown, and the rest of the text
// is searched as usual. The dropdown writes its choice back into an existing prefix, so the
// two never disagree.
namespace PS::ItemSourcePrefix {
inline std::string Lower(std::string_view text){std::string out(text);std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return char(std::tolower(c));});return out;}
struct Parsed{std::string Text;std::optional<std::string> Source;size_t Begin=0,End=0;};
// Finds the first source token at a word start. Everything else is kept, with the gap the
// token leaves collapsed to one space.
inline Parsed Parse(std::string_view input){
    Parsed out;out.Text=std::string(input);
    const auto lower=Lower(input);
    for(size_t i=0;i<lower.size();++i){
        if(i&&!std::isspace(static_cast<unsigned char>(lower[i-1]))&&lower[i-1]!='(')continue;
        size_t value=std::string::npos;
        for(const std::string_view key:{"source:","src:"})if(lower.compare(i,key.size(),key)==0){value=i+key.size();break;}
        if(value==std::string::npos)continue;
        size_t end=value;std::string name;
        if(end<input.size()&&input[end]=='"'){const auto close=input.find('"',end+1);name=std::string(input.substr(end+1,(close==input.npos?input.size():close)-end-1));end=close==input.npos?input.size():close+1;}
        else{while(end<input.size()&&!std::isspace(static_cast<unsigned char>(input[end]))&&input[end]!=')')++end;name=std::string(input.substr(value,end-value));}
        out.Source=name;out.Begin=i;out.End=end;
        auto left=std::string(input.substr(0,i)),right=std::string(input.substr(end));
        while(!left.empty()&&std::isspace(static_cast<unsigned char>(left.back())))left.pop_back();
        size_t skip=0;while(skip<right.size()&&std::isspace(static_cast<unsigned char>(right[skip])))++skip;right.erase(0,skip);
        out.Text=left.empty()||right.empty()?left+right:left+" "+right;
        return out;
    }
    return out;
}
// The token as the user would type it: quoted only when needed.
inline std::string Token(std::string_view name){
    const bool quote=name.find_first_of(" \t()")!=name.npos;
    return std::string("source:")+(quote?"\"":"")+std::string(name)+(quote?"\"":"");
}
// Rewrites an existing token (or removes it when name is empty). Text without a token is
// returned unchanged: choosing in the dropdown does not add text the user did not type.
inline std::string Replace(std::string_view input,std::string_view name){
    const auto parsed=Parse(input);if(!parsed.Source)return std::string(input);
    if(name.empty())return parsed.Text;
    return std::string(input.substr(0,parsed.Begin))+Token(name)+std::string(input.substr(parsed.End));
}
// Resolves a typed name to a group keyword ("all", "vanilla", "expansion", "modded",
// "unknown") or one provider from choices (provider id, label). Exact matches win; a prefix
// must be unique. Returns nothing when the name is unknown or ambiguous.
inline std::optional<std::string> Resolve(std::string_view typed,const std::vector<std::pair<std::string,std::string>>& choices){
    const auto name=Lower(typed);if(name.empty())return std::nullopt;
    const std::pair<std::string_view,std::string_view> groups[]{{"all","all"},{"any","all"},{"vanilla","vanilla"},{"base","vanilla"},{"core","vanilla"},{"game","vanilla"},
        {"expansion","expansion"},{"plugins","expansion"},{"plugin","expansion"},{"modded","modded"},{"mods","modded"},{"mod","modded"},{"unknown","unknown"}};
    for(const auto& [word,group]:groups)if(name==word)return std::string(group);
    const auto folder=[](const std::string& provider){const auto slash=provider.find('/');return slash==provider.npos?provider:provider.substr(slash+1);};
    for(const auto& [provider,label]:choices)if(name==Lower(provider)||name==Lower(label)||name==Lower(folder(provider)))return provider;
    std::optional<std::string> found;
    for(const auto& [provider,label]:choices)if(Lower(label).starts_with(name)||Lower(folder(provider)).starts_with(name)){if(found&&*found!=provider)return std::nullopt;found=provider;}
    return found;
}
}
