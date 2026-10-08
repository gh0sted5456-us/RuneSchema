#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace PS::QuickCatalogRules {
inline std::string Lower(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
    return value;
}
inline bool MiningName(std::string_view name) {
    const auto n=Lower(std::string(name));
    return n.starts_with("bp_orenode_")||n.starts_with("bp_miningrock_")
        ||n.starts_with("bp_stonenode_")||n.starts_with("bp_rocknode_");
}
inline bool ResourceAncestry(std::string_view name) {
    const auto n=Lower(std::string(name));
    for(const auto* token:{"resource","harvest","orenode","ore_node","mining","stonenode","rocknode",
        "tree","fellabletree","sapling","fishing","crop","farmplot","forag","bush","woodcut",
        "splittablelog","felledlog","logbase","blightwood","elderwood","gatherable",
        "gatheringnode","harvestable","fishingpool","fishinghole","fishingnet","pickable"})
        if(n.find(token)!=std::string::npos)return true;
    return false;
}
inline bool MissingClassMetadata(std::string_view type) {
    const auto t=Lower(std::string(type));return t.empty()||t=="none"||t=="null";
}
inline std::string GeneratedClassPath(const std::string& package,const std::string& name) {
    if(package.empty()||package.front()!='/'||package.size()>2048||name.empty()||name.size()>512
        ||name.find_first_of("/.'\"\r\n\t ")!=name.npos||package.find_first_of(".'\"\r\n\t ")!=package.npos)return {};
    for(const auto ch:package+name)if(static_cast<unsigned char>(ch)<32)return {};
    auto result=package+"."+name;if(!result.ends_with("_C"))result+="_C";
    return result;
}
inline bool BlueprintCandidate(std::string_view name,std::string_view type) {
    const auto t=Lower(std::string(type)),n=Lower(std::string(name));
    // Generated class metadata is primary. These naming fallbacks also cover
    // the packaged mining classes used by the existing settings /spawns path.
    // A candidate is NOT admitted until the actual class inheritance is checked.
    if(t.find("blueprint")!=t.npos||t=="class"||t.ends_with(".class")||MiningName(name)||n.ends_with("_c"))return true;
    // Missing metadata is not permission to omit an asset. Try its generated
    // class path; failures are retained as unclassified coverage issues.
    return n.starts_with("bp_")||MissingClassMetadata(type);
}
inline int Priority(std::string_view package,std::string_view name) {
    if(MiningName(name))return 0;
    const auto p=Lower(std::string(package));
    if(p.find("/ai/")!=p.npos||p.find("/enemies/")!=p.npos||p.find("/characters/")!=p.npos)return 1;
    if(p.find("/world/")!=p.npos||ResourceAncestry(name))return 2;
    return 3;
}
} // namespace PS::QuickCatalogRules
