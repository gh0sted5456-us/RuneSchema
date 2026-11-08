#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace RC::Unreal {
struct FMemory {
    static inline std::vector<uint8_t> bytes;
    static inline size_t requested{};
    static inline uint32_t requestedAlignment{};
    static inline int live=0;
    static inline bool failAllocation=false;
    static void* Malloc(size_t size,uint32_t alignment) {
        if(failAllocation) return nullptr;
        requested=size;requestedAlignment=alignment;
        bytes.assign(size+128,0xcd);++live;
        return bytes.data();
    }
    static void Free(void*) { --live; }
    static bool GuardIntact() {
        return std::all_of(bytes.begin()+requested,bytes.end(),[](auto b){return b==0xcd;});
    }
};
}
