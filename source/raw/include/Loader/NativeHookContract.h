#pragma once
#include <cstdint>
#include <span>
#include <cstring>

namespace DragonWilds::NativeHookContract {
template<class Site> struct Profile {
    uint32_t timestamp, imageSize;
    std::span<const Site> sites;
};
template<class Site, size_t Count>
const Profile<Site>* Select(uint32_t timestamp, uint32_t imageSize, const Profile<Site> (&profiles)[Count]) {
    for (const auto& profile : profiles)
        if (profile.timestamp == timestamp && profile.imageSize == imageSize) return &profile;
    return nullptr;
}
template<class Site>
bool Validate(std::span<const unsigned char> image, std::span<const Site> sites) {
    for (const auto& site : sites) {
        if (site.rva > image.size() || site.bytes.size() > image.size() - site.rva ||
            std::memcmp(image.data() + site.rva, site.bytes.data(), site.bytes.size())) return false;
        if (site.resume && (site.resume > image.size() || site.resumeBytes.size() > image.size() - site.resume ||
            std::memcmp(image.data() + site.resume, site.resumeBytes.data(), site.resumeBytes.size()))) return false;
    }
    return true;
}
}
