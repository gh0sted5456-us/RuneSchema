#pragma once
// Narrow test doubles for compiling the actual map helper without the game.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>
namespace RC::Unreal {
using int32=int32_t;using uint32=uint32_t;using uint8=uint8_t;
struct FMemory {
    static inline std::vector<uint8> bytes;
    static inline size_t requested{};static inline uint32 requestedAlignment{};
    static void* Malloc(size_t size,uint32 alignment=8) {
        requested=size;requestedAlignment=alignment;bytes.assign(size+128,0xcd);return bytes.data();
    }
    static void Free(void*) {}
    static void Memcpy(void* to,const void* from,size_t size) { std::memcpy(to,from,size); }
    static bool GuardIntact() { return std::all_of(bytes.begin()+requested,bytes.end(),[](auto b){return b==0xcd;}); }
};
struct FProperty {
    int32 size,alignment;uint8 marker;
    int32 GetSize() const {return size;} int32 GetElementSize() const {return size;}
    int32 GetMinAlignment() const {return alignment;}
    void InitializeValue(void* p) {std::memset(p,marker,size);}
    bool Identical(void* a,void* b) {return std::memcmp(a,b,size)==0;}
    uint32 GetValueTypeHash(const void*) {return 1;}
};
struct FMapProperty { FProperty* key;FProperty* value;FProperty* GetKeyProp(){return key;}FProperty* GetValueProp(){return value;} };
struct FScriptMapLayout {int32 ValueOffset;struct {int32 Size;} SetLayout;};
struct FScriptMap {
    struct Slot {bool live;std::vector<uint8> bytes;};std::vector<Slot> slots;
    static FScriptMapLayout GetScriptLayout(int32 ks,int32,int32 vs,int32 va) {auto offset=(ks+va-1)&~(va-1);return {offset,{offset+vs+8}};}
    int32 Num() {return static_cast<int32>(std::count_if(slots.begin(),slots.end(),[](auto& s){return s.live;}));}
    int32 GetMaxIndex(){return static_cast<int32>(slots.size());}
    bool IsValidIndex(int32 i){return i>=0 && i<GetMaxIndex() && slots[i].live;}
    void* GetData(int32 i,FScriptMapLayout){return slots.at(i).bytes.data();}
    int32 AddUninitialized(FScriptMapLayout l){slots.push_back({true,std::vector<uint8>(l.SetLayout.Size)});return GetMaxIndex()-1;}
    void RemoveAt(int32 i,FScriptMapLayout){slots.at(i).live=false;}
    template<class F> void Rehash(FScriptMapLayout,F){}
};
}
