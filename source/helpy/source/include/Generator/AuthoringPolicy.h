#pragma once
// Small, engine-independent constraints shared by both authoring UIs.
#include <atomic>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PS::Authoring {
inline constexpr char Version[] = "R3v6";
// This is a conservative authoring-tool limit, not a claim about the game's cap.
inline constexpr int MinPower = 1, MaxPower = 100;
inline constexpr std::size_t MaxCloneFields = 512, MaxCloneValueBytes = 16384;
inline constexpr std::size_t MaxCloneDocumentBytes = 262144;
inline std::atomic<bool> PermanentSpawn{false}, PermanentAsset{false};
inline std::atomic<int> EnemyPower{-1}; // -1 means inherit the native/template value.
inline void ValidatePower(int value) {
    if(value != -1 && (value < MinPower || value > MaxPower))
        throw std::runtime_error("Power level must be native/default or 1..100.");
}
inline bool IdentityField(std::string_view name) {
    return name == "PersistenceID" || name == "InternalName";
}
inline bool SoftDeleteName(std::string_view name) {
    return name == "bSoftDeleted" || name == "IsSoftDelete" || name == "IsSoftDeleted"
        || name == "bIsSoftDelete" || name == "bIsSoftDeleted";
}
inline bool SafeFieldName(std::string_view name) {
    if(name.empty() || name.size()>256 || IdentityField(name) || SoftDeleteName(name) || name=="Modded" || name=="RuneSchema")return false;
    for(unsigned char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    return true;
}
inline void ValidateObjectPath(std::string_view path) {
    if(path.empty() || path.size()>2048 || path.front()!='/' || path.find('\0')!=path.npos
        || path.find("..")!=path.npos || path.find('\\')!=path.npos)
        throw std::runtime_error("Select a complete, loaded item object path.");
    for(unsigned char c:path)if(c<32 || c==127)throw std::runtime_error("Object paths cannot contain control characters.");
}
inline std::string NewPersistenceId() {
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::random_device random;
    std::string out(22,'A');
    for(std::size_t i=0;i<21;++i)out[i]=alphabet[random()&63];
    out[21]=alphabet[(random()&3)<<4];
    return out;
}
inline std::string NewName() {
    auto id=NewPersistenceId();
    // UE package names may not contain '-'; the PersistenceID itself is untouched.
    for(auto& c:id)if(c=='-')c='_';
    return "RS_v5_"+id;
}
inline bool SafeFileStem(std::string_view stem) {
    if(stem.empty() || stem.size()>96 || !stem.starts_with("RS_v5_"))return false;
    for(unsigned char c:stem)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    return true;
}
} // namespace PS::Authoring
