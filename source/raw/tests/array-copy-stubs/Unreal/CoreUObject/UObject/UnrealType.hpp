#pragma once
#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
// Reflection API double; the test executes the production FArrayCopy header.
namespace RC::Unreal {
struct FScriptArray {std::vector<std::string> Entries;};
struct FArrayProperty {
    int Dimensions=1,Size=sizeof(FScriptArray),FailAfter=-1,Live=0;
    bool FailInitialize=false;
    int GetArrayDim()const{return Dimensions;}
    int GetElementSize()const{return Size;}
    int GetMinAlignment()const{return alignof(FScriptArray);}
    void InitializeValue(void* value){if(FailInitialize)throw std::bad_alloc();new(value)FScriptArray;++Live;}
    void CopySingleValue(void* target,void* source){
        auto& to=static_cast<FScriptArray*>(target)->Entries;const auto& from=static_cast<FScriptArray*>(source)->Entries;
        to.clear();int copied=0;for(const auto& row:from){to.push_back(row);if(copied++==FailAfter)throw std::runtime_error("Injected reflected copy failure");}
    }
    void DestroyValue(void* value){static_cast<FScriptArray*>(value)->~FScriptArray();--Live;}
};
}
