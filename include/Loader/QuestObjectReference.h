#pragma once
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace DragonWilds::QuestRegistry {
inline void ValidateObjectReferenceLayout(std::string_view type,int size,int dim,size_t pointerSize) {
    if((type!="ObjectProperty" && type!="ObjectPtrProperty") || size!=pointerSize || dim!=1)
        throw std::runtime_error("Quest registry requires a scalar strong object reference");
}
template<class Pointer,class Copy,class Read>
void CopyResolvedReference(Pointer object,void* destination,Copy copy,Read read) {
    if(!object || !destination)throw std::runtime_error("Quest object reference storage is missing");
    copy(destination,&object);
    if(read(destination)!=object)throw std::runtime_error("Quest object reference copy did not round-trip");
}
}
