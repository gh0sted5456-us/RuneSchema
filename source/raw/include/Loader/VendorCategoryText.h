#pragma once

#include <string>

namespace RC::Unreal {
    class FArrayProperty;
    class FTextProperty;
}

namespace DragonWilds::VendorCategoryText {
// All pointers below address initialized, reflected storage. Call only on the
// game thread, as with the existing merchant/recipe placement code.
std::string Read(const void* container, RC::Unreal::FTextProperty* property);
void Write(void* container, RC::Unreal::FTextProperty* property,
           const std::string& label);

}
