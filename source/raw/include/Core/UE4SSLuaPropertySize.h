#pragma once
#include <cstddef>

namespace PS::UE4SSLuaPropertySize {
inline bool RepairObjectPtr(int& objectPtrSize, int objectSize) {
    if ((objectPtrSize != 0 && objectPtrSize != -1) || objectSize != sizeof(void*)) return false;
    // UE5 object-pointer values have the same storage width as object references, not FProperty descriptors.
    objectPtrSize = objectSize;
    return true;
}
}
