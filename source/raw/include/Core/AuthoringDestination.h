#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
namespace PS::Authoring {
inline bool SafeModFolder(std::string_view name){
    if(name.empty()||name.size()>64)return false;
    std::string lower;
    for(unsigned char c:name){if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;lower+=static_cast<char>(std::tolower(c));}
    if(lower=="con"||lower=="prn"||lower=="aux"||lower=="nul")return false;
    if(lower.size()==4&&(lower.starts_with("com")||lower.starts_with("lpt"))&&lower[3]>='1'&&lower[3]<='9')return false;
    return true;
}
inline std::filesystem::path AuthoredDirectory(const std::filesystem::path& modsRoot,std::string_view mod,std::string_view category){
    if(!SafeModFolder(mod))throw std::runtime_error("Mod folder must be 1..64 letters, digits, underscores or hyphens, and not a reserved device name.");
    if(category!="spawns"&&category!="assets"&&category!="raw"&&category!="journal"&&category!="recipes"&&category!="npc")throw std::runtime_error("Invalid authoring category");
    const auto root=std::filesystem::weakly_canonical(modsRoot);
    const auto target=std::filesystem::weakly_canonical(root/std::string(mod)/std::string(category));
    const auto relative=target.lexically_relative(root);
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Authoring destination resolves outside RuneSchema's content-mod folder.");
    return target;
}
}
