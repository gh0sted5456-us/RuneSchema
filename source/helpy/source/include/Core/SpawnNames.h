#pragma once
#include "Core/SpawnCatalogSearch.h"
#include <cstring>
namespace PS::SpawnNames {
struct Label{std::string Name,Art;};
inline std::string Words(const std::string& text){std::string out;for(size_t i=0;i<text.size();++i){const auto c=static_cast<unsigned char>(text[i]);if(i&&std::isupper(c)&&std::islower(static_cast<unsigned char>(text[i-1])))out+=' ';out+=char(c);}return out;}
inline Label Display(const nlohmann::json& row){
    auto display=SpawnCatalogSearch::Text(row,"DisplayName");
    const auto name=Words(display.empty()?SpawnCatalogSearch::Text(row,"Name"):display);
    if(SpawnCatalogSearch::Text(row,"Type")=="Resource"){
        auto lower=SpawnCatalogSearch::Fold(name);for(auto& c:lower)if(c=='_')c=' ';
        for(const auto* material:{"copper","tin","iron","coal","gold","silver","blurite","mithril","adamant","granite","stone","clay"}){
            const std::string token=material;const auto at=lower.find(token);if(at==lower.npos||(at&&std::isalnum(static_cast<unsigned char>(lower[at-1])))||(at+token.size()<lower.size()&&std::isalnum(static_cast<unsigned char>(lower[at+token.size()]))))continue;
            if(lower.find("node")==lower.npos&&lower.find("deposit")==lower.npos&&lower.find("mining rock")==lower.npos)continue;
            auto proper=token;proper[0]=char(std::toupper(proper[0]));std::string size;
            for(const auto* candidate:{"small","medium","large"})if(lower.find(candidate)!=lower.npos)size=" ("+std::string(candidate)+")";
            return {proper+" node"+size,proper+(token=="stone"||token=="clay"||token=="coal"||token=="granite"?"":" ore")};
        }
        if(!display.empty())return {display,display};
        struct Prefix{const char* Native;const char* Suffix;const char* State;};
        for(const auto& p:{Prefix{"Fellable Tree "," Tree",""},Prefix{"Felled Tree "," Tree"," (Felled)"},Prefix{"Splittable Log "," Log"," (Splittable)"},Prefix{"Log "," Log",""}})
            if(name.starts_with(p.Native)){auto species=Words(name.substr(std::strlen(p.Native)));std::string state=p.State;if(species.ends_with(" Bare")){species.resize(species.size()-5);state+=" (Bare)";}return {species+p.Suffix+state,species+" Tree"};}
    }
    if(!display.empty())return {display,display};
    auto readable=Words(name);return {readable,readable};
}
inline std::string Key(std::string name){std::string key;for(unsigned char c:name)if(std::isalnum(c))key+=char(std::tolower(c));return key;}
}
