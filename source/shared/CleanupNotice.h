#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <set>

namespace PS::CleanupNotice {
inline bool ModName(const std::string& name) {
    return !name.empty()&&name.size()<=128&&std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127||c=='['||c==']';});
}
inline std::string Message(std::uint64_t removed,const std::string& mod={}) {
    if(!removed)return {};
    return "Removed "+std::to_string(removed)+" records"+(ModName(mod)?" from uninstalled mod: "+mod:std::string{})+".";
}
inline bool Valid(const nlohmann::json& value) {
    return value.is_object()&&value.size()==2&&value.contains("Removed")&&value["Removed"].is_number_unsigned()
        &&value["Removed"].get<std::uint64_t>()>0&&value["Removed"].get<std::uint64_t>()<=1'000'000'000
        &&value.contains("Mod")&&value["Mod"].is_string()
        &&(value["Mod"].get_ref<const std::string&>().empty()||ModName(value["Mod"].get_ref<const std::string&>()));
}
inline std::string Message(const nlohmann::json& value) {
    return Valid(value)?Message(value["Removed"].get<std::uint64_t>(),value["Mod"].get<std::string>()):std::string{};
}
class WorldReceipt {
    std::wstring Slot;
    const void* Previous=nullptr;
    const void* World=nullptr;
    std::set<const void*> Sent;
public:
    std::uint64_t Removed=0;
    void Begin(std::wstring slot){Slot=std::move(slot);Removed=0;World=nullptr;Sent.clear();}
    void Record(const std::wstring& slot,std::size_t count){if(!Slot.empty()&&slot==Slot)Removed+=count;}
    void Entering(const void* previous){Previous=previous;World=nullptr;Sent.clear();}
    void Entered(const void* world){World=world!=Previous?world:nullptr;}
    bool Pending(const void* player,const void* world)const{return Removed&&World&&World==world&&player&&!Sent.contains(player)&&Sent.size()<256;}
    void Acknowledge(const void* player){Sent.insert(player);}
    bool Bound()const{return World!=nullptr;}
};
}
