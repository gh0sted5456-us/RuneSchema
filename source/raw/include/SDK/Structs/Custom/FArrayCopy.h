#pragma once
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Core/HAL/UnrealMemory.hpp>
#include <new>
#include <stdexcept>
#include <utility>
namespace UECustom {
class FArrayCopy {
    RC::Unreal::FArrayProperty* Property;
    void* Target;
public:
    void* Data=nullptr;
    FArrayCopy(RC::Unreal::FArrayProperty* property,void* source):Property(property),Target(source){
        using namespace RC::Unreal;
        if(!property||!source||property->GetArrayDim()!=1||property->GetElementSize()!=sizeof(FScriptArray))
            throw std::runtime_error("Unsupported reflected array storage");
        Data=FMemory::Malloc(property->GetElementSize(),property->GetMinAlignment());
        if(!Data)throw std::bad_alloc();
        bool initialized=false;
        try{Property->InitializeValue(Data);initialized=true;Property->CopySingleValue(Data,source);}
        catch(...){if(initialized)Property->DestroyValue(Data);FMemory::Free(Data);throw;}
    }
    ~FArrayCopy(){Property->DestroyValue(Data);RC::Unreal::FMemory::Free(Data);}
    FArrayCopy(const FArrayCopy&)=delete;
    FArrayCopy& operator=(const FArrayCopy&)=delete;
    // Swap owned array storage on the game thread; committing cannot allocate.
    void Commit()noexcept{
        if(!Target)return;
        auto* target=static_cast<unsigned char*>(Target);auto* draft=static_cast<unsigned char*>(Data);
        for(std::size_t i=0;i<sizeof(RC::Unreal::FScriptArray);++i)std::swap(target[i],draft[i]);
        Target=nullptr;
    }
};
}
