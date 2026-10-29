#pragma once
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <stdexcept>
namespace PS::ItemFamily {
// Browsing identity only: strip equipment-slot words, retain set, material,
// quality and dye/trim words. No asset loading, grants or item mutation.
inline std::string Key(std::string_view name){
    if(name.size()>256)return {};
    std::string words;for(size_t i=0;i<name.size();++i){
        const unsigned char c=name[i];
        if(c=='\''&&i+1<name.size()&&(name[i+1]=='s'||name[i+1]=='S')&&(i+2==name.size()||!std::isalnum(static_cast<unsigned char>(name[i+2])))){++i;continue;}
        if(i+3<name.size()&&name.substr(i,3)=="\xe2\x80\x99"&&name[i+3]=='s'&&(i+4==name.size()||!std::isalnum(static_cast<unsigned char>(name[i+4])))){i+=3;continue;}
        if(c>127){words+=' ';continue;}
        if(std::isupper(c)&&i&&std::islower(static_cast<unsigned char>(name[i-1])))words+=' ';
        words+=std::isalnum(c)?static_cast<char>(std::tolower(c)):' ';
    }
    static const std::set<std::string> slots{"helm","helmet","hood","coif","cowl","hat","mask","cap","chestplate","chestpiece","platebody","cuirass","hauberk","body","chest","tunic","coat","robe","robes","legs","legplates","platelegs","greaves","leggings","chaps","tassets","skirt","boots","shoes","gloves","gauntlets","bracers","vambraces","pauldrons","shoulders"};
    std::istringstream input(words);std::string word,key;bool slot=false,robe=false;
    while(input>>word){
        const bool robePart=robe&&(word=="top"||word=="bottom");robe=word=="robe"||word=="robes";
        if(robePart)continue;
        if(slots.contains(word)){
            slot=true;
            if(word=="helm"||word=="helmet"){
                if(key.ends_with("_med"))key.resize(key.size()-4);
                else if(key.ends_with("_full"))key.resize(key.size()-5);
                else if(key=="med"||key=="full")key.clear();
            }
            continue;
        }
        if(word=="item"||word=="armor"||word=="armour")continue;if(!key.empty())key+='_';key+=word;
    }
    return slot&&key.size()>=3?key:std::string{};
}
inline bool Matches(const std::string& filter,std::string_view name){return filter.empty()||Key(name)==filter;}
inline std::string TakeFilter(std::string& query){
    constexpr std::string_view prefix="__helpyfamily:";if(!query.starts_with(prefix))return {};
    const auto end=query.find(' ');auto key=query.substr(prefix.size(),end==query.npos?std::string::npos:end-prefix.size());
    if(key.empty()||key.size()>192||key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=key.npos)throw std::runtime_error("Invalid item family filter");
    query=end==query.npos?std::string{}:query.substr(end+1);return key;
}
}
