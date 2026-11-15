#pragma once
// Narrow test doubles for compiling the actual map helper without the game.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>
#include <Unreal/Core/HAL/UnrealMemory.hpp>
namespace RC::Unreal {
using int32=int32_t;using uint32=uint32_t;using uint8=uint8_t;
struct FProperty {
    int32 size,alignment;uint8 marker;
    int32 GetSize() const {return size;} int32 GetElementSize() const {return size;}
    int32 GetMinAlignment() const {return alignment;}
    int live=0;
    bool failInitialize=false;
    void InitializeValue(void* p) {
        if(failInitialize) throw std::runtime_error("Injected initialize failure");
        std::memset(p,marker,size);
        ++live;
    }
    void DestroyValue(void* p) {
        if(live<=0) std::terminate();
        --live;
        std::memset(p,0,size);
    }
    void CopySingleValue(void* to,const void* from) {std::memcpy(to,from,size);}
    bool Identical(const void* a,const void* b) {return std::memcmp(a,b,size)==0;}
    uint32 GetValueTypeHash(const void*) {return 1;}
};
struct FScriptArray {
    std::vector<uint8> bytes;
    int32 count=0;
    int32 Add(int32 num,int32 size,int32) {
        const auto start=count;
        count+=num;
        bytes.resize(count*size);
        return start;
    }
    int32 Num() const {return count;}
    void* GetData(){return bytes.data();}
    void Empty(int32,int32,int32){bytes.clear();count=0;}
};
struct FArrayProperty {FProperty* inner;FProperty* GetInner(){return inner;}};
inline constexpr int32 INDEX_NONE=-1;
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
    template<class Hash,class Equal>
    int32 FindPairIndex(const void* key,FScriptMapLayout layout,Hash,Equal equal) {
        for(int32 i=0;i<GetMaxIndex();++i)
            if(IsValidIndex(i)&&equal(key,GetData(i,layout)))return i;
        return INDEX_NONE;
    }
    template<class Hash,class Equal>
    void* FindValue(const void* key,FScriptMapLayout layout,Hash hash,Equal equal) {
        const auto i=FindPairIndex(key,layout,hash,equal);
        return i==INDEX_NONE?nullptr:static_cast<uint8*>(GetData(i,layout))+layout.ValueOffset;
    }
    template<class Hash,class Equal,class KeyInit,class ValueInit,class Replace,class KeyDestroy,class ValueDestroy>
    void Add(const void* key,const void*,FScriptMapLayout layout,Hash hash,Equal equal,
             KeyInit keyInit,ValueInit valueInit,Replace replace,KeyDestroy,ValueDestroy) {
        if(auto* value=FindValue(key,layout,hash,equal)){replace(value);return;}
        auto* pair=static_cast<uint8*>(GetData(AddUninitialized(layout),layout));
        keyInit(pair);
        valueInit(pair+layout.ValueOffset);
    }
    void RemoveAt(int32 i,FScriptMapLayout){slots.at(i).live=false;}
    template<class F> void Rehash(FScriptMapLayout,F){}
};
}
