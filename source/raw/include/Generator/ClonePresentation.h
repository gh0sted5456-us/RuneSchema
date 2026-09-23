#pragma once
#include <algorithm>
#include <array>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PS::ClonePresentation {
// The game uses a 22-character, unpadded URL-safe base64 identifier (128 bits).
// Only A/Q/g/w are canonical in its last position. Never put an arbitrary digit there.
inline std::string ModFragment(std::string_view label) {
    std::string out;
    for(unsigned char c:label) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'))out+=static_cast<char>(c);
        if(out.size()==6)break;
    }
    return out.empty()?"Mod":out;
}
inline std::string Prefix(std::string_view label) {return "RS7"+ModFragment(label);}
inline bool ValidId(std::string_view id,std::string_view label) {
    const auto prefix=Prefix(label);
    if(id.size()!=22 || !id.starts_with(prefix) || std::string_view("AQgw").find(id.back())==std::string_view::npos)return false;
    for(std::size_t i=prefix.size();i<21;++i)if(id[i]<'0'||id[i]>'9')return false;
    return true;
}
inline std::string NewId(std::string_view label) {
    std::random_device random;
    std::uniform_int_distribution<int> digit(0,9),last(0,3);
    auto id=Prefix(label);
    while(id.size()<21)id+=static_cast<char>('0'+digit(random));
    id+="AQgw"[last(random)];return id;
}
// Copy appearance is deliberately narrower than copying every item setting.
// In particular Icon, PowerLevel, identity, equipment slots and stat tables stay out.
inline bool VisualField(std::string_view name) {
    constexpr std::array fields{"StaticMesh","SkeletalMesh","Mesh","MeshData","MaleMeshData","FemaleMeshData",
        "Materials","MaterialOverrides","OverrideMaterials","Material","MaleMesh","FemaleMesh","HeldEquipmentActorClass"};
    return std::find(fields.begin(),fields.end(),name)!=fields.end();
}
inline std::string ClassLeaf(std::string value) {
    const auto at=value.find_last_of("./");if(at!=value.npos)value.erase(0,at+1);
    if(value.ends_with("'"))value.pop_back();
    return value;
}
inline bool VisualAssetType(std::string_view type) {
    const auto leaf=ClassLeaf(std::string(type));
    return leaf=="StaticMesh"||leaf=="SkeletalMesh"||leaf=="WearableEquipmentMeshData";
}
// Held meshes live in an actor blueprint, not necessarily the item's StaticMesh.
// Do not exchange a bow/sword/tool actor across different item categories.
inline bool CompatibleAppearance(std::string_view sourceClass,std::string_view donorClass,
    std::string_view sourceGroup,std::string_view donorGroup) {
    return !sourceClass.empty()&&sourceClass==donorClass&&sourceGroup==donorGroup&&sourceGroup!="held:unresolved";
}
inline bool CompatibleVisual(std::string_view expected,std::string_view actual) {
    return !expected.empty()&&ClassLeaf(std::string(expected))==ClassLeaf(std::string(actual));
}
} // namespace PS::ClonePresentation
