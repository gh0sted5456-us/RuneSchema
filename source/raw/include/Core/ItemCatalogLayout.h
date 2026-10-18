#pragma once
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
namespace PS {
// One cursor covers priority runtime clones, live objects and mounted hints.
// It never truncates a valid roster to a UI-sized or network-sized count.
struct ItemCatalogLayout {
    std::array<std::size_t,3> Counts;
    std::size_t Total() const {
        std::size_t result=0;
        for(const auto count:Counts){if(count>std::numeric_limits<std::size_t>::max()-result)throw std::runtime_error("Item catalogue cursor overflow");result+=count;}
        return result;
    }
    std::pair<std::size_t,std::size_t> At(std::size_t index) const {
        for(std::size_t segment=0;segment<Counts.size();++segment){if(index<Counts[segment])return {segment,index};index-=Counts[segment];}
        throw std::runtime_error("Item catalogue cursor is outside its roster");
    }
};
}
