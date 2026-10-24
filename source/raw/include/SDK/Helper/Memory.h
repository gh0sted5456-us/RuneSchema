#pragma once

#include "Helpers/String.hpp"

namespace DragonWilds {
    uintptr_t** GetVTablePtrByClassPath(const RC::StringType& classPath);
    void* GetVirtualFunctionFromVTable(uintptr_t** vtable, size_t offset);
}
