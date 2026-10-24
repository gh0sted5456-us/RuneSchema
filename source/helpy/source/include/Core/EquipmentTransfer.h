#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <optional>
#include <vector>
#include <stdexcept>
namespace PS::EquipmentTransfer {
using Guid=std::array<uint32_t,4>;
struct Instance {Guid Id;int Slot;};
inline std::optional<Instance> NewlyDelivered(const std::set<Guid>& before,const std::vector<Instance>& after){
    std::optional<Instance> result;std::set<Guid> seen;
    for(const auto& item:after){if(item.Id==Guid{}||item.Slot<0||item.Slot>=1024||!seen.insert(item.Id).second)throw std::runtime_error("Invalid or duplicate equipment identity");
        if(before.contains(item.Id))continue;if(result)throw std::runtime_error("Ambiguous new equipment instances; no move attempted");result=item;
    }return result;
}
inline bool Confirmed(bool nativeAccepted,const Guid& expected,const Guid& target,bool remainsInSource){return nativeAccepted&&expected!=Guid{}&&expected==target&&!remainsInSource;}
}
