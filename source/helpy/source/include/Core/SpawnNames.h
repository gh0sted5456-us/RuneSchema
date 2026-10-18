#pragma once
#include "Core/SpawnCatalogSearch.h"
#include <cstring>
namespace PS::SpawnNames {
struct Label{std::string Name,Art;};
inline std::string Words(const std::string& text){std::string out;for(size_t i=0;i<text.size();++i){const auto c=static_cast<unsigned char>(text[i]);if(i&&std::isupper(c)&&std::islower(static_cast<unsigned char>(text[i-1])))out+=' ';out+=char(c);}return out;}
inline Label Display(const nlohmann::json& row){
    auto display=SpawnCatalogSearch::Text(row,"DisplayName");if(!display.empty())return {display,display};
    const auto name=SpawnCatalogSearch::Text(row,"Name");
    if(SpawnCatalogSearch::Text(row,"Type")=="Resource"){
        struct Prefix{const char* Native;const char* Suffix;const char* State;};
        for(const auto& p:{Prefix{"FellableTree "," Tree",""},Prefix{"FelledTree "," Tree"," (Felled)"},Prefix{"SplittableLog "," Log"," (Splittable)"},Prefix{"Log "," Log",""}})
            if(name.starts_with(p.Native)){auto species=Words(name.substr(std::strlen(p.Native)));std::string state=p.State;if(species.ends_with(" Bare")){species.resize(species.size()-5);state+=" (Bare)";}return {species+p.Suffix+state,species+" Tree"};}
    }
    auto readable=Words(name);return {readable,readable};
}
inline std::string Key(std::string name){std::string key;for(unsigned char c:name)if(std::isalnum(c))key+=char(std::tolower(c));return key;}
}
