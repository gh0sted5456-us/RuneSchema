#include "Loader/NativeHookContract.h"
#include "Loader/SurgeNativeContract.h"
#include "Loader/ShadowveilNativeContract.h"
#include <cassert>
#include <limits>
using namespace DragonWilds;
int main() {
    for (const auto& p : SurgeNative::Profiles) assert(NativeHookContract::Select(p.timestamp,p.imageSize,SurgeNative::Profiles) == &p);
    for (const auto& p : ShadowveilNative::Profiles) assert(NativeHookContract::Select(p.timestamp,p.imageSize,ShadowveilNative::Profiles) == &p);
    assert(!NativeHookContract::Select(0,SurgeNative::ImageSize,SurgeNative::Profiles));
    assert(!NativeHookContract::Select(SurgeNative::ServerTimestamp,SurgeNative::ImageSize,SurgeNative::Profiles));
    assert(SurgeNative::Profiles[0].sites.size() == 8 && SurgeNative::Profiles[1].sites.size() == 8);
    assert(ShadowveilNative::Profiles[0].sites.size() == 5 && ShadowveilNative::Profiles[1].sites.size() == 5);
    std::array<unsigned char,128> bytes{};
    SurgeNative::Site site{4,64,{},{}};
    auto sites=std::span<const SurgeNative::Site>(&site,1);
    assert(NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites));
    bytes[4]=1; assert(!NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites)); bytes[4]=0;
    bytes[64]=1; assert(!NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites)); bytes[64]=0;
    site.rva=std::numeric_limits<uintptr_t>::max(); assert(!NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites));
    site.rva=110; assert(!NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites));
    site.rva=4;site.resume=110;assert(!NativeHookContract::Validate(std::span<const unsigned char>(bytes),sites));
}
