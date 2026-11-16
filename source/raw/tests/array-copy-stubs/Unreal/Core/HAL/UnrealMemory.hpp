#pragma once
#include <cstddef>
#include <cstdlib>
namespace RC::Unreal {
struct FMemory {
    static inline int Allocations=0;
    static inline bool FailAllocation=false;
    static void* Malloc(std::size_t size,unsigned){if(FailAllocation)return nullptr;auto* result=std::malloc(size);if(result)++Allocations;return result;}
    static void Free(void* value){if(value){--Allocations;std::free(value);}}
};
}
