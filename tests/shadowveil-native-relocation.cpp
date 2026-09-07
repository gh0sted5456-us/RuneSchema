#include <Windows.h>
#include <safetyhook.hpp>
#include "Loader/ShadowveilNativeContract.h"
#include <array>
#include <cstring>
#include <iostream>
using namespace DragonWilds::ShadowveilNative;
void Observe(safetyhook::Context&) {}
int main() {
    // Private synthetic image, never executed. No game process is opened.
    auto* image = static_cast<unsigned char*>(VirtualAlloc(nullptr, ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!image) return 2;
    std::array<safetyhook::MidHook, std::size(Sites)> hooks;
    for (const auto& site : Sites) {
        std::memcpy(image + site.rva, site.bytes.data(), site.bytes.size());
        if (site.resume) std::memcpy(image + site.resume, site.resumeBytes.data(), site.resumeBytes.size());
    }
    for (const unsigned mask : {0u, 7u, 8u, 16u, 24u, 31u}) {
    for (size_t i = 0; i < hooks.size(); ++i) {
        if (!(mask & (1u << i))) continue;
        auto hook = safetyhook::MidHook::create(image + Sites[i].rva, Observe, safetyhook::MidHook::StartDisabled);
        if (!hook) { std::cerr << "Cannot relocate site " << i << "\n"; return 3; }
        hooks[i] = std::move(*hook);
    }
    for (auto& hook : hooks) if (hook && !hook.enable()) return 4;
    for (auto& hook : hooks) hook = {};
    for (const auto& site : Sites) {
        if (std::memcmp(image + site.rva, site.bytes.data(), site.bytes.size())) return 5;
        if (site.resume && std::memcmp(image + site.resume, site.resumeBytes.data(), site.resumeBytes.size())) return 6;
    }
    }
    VirtualFree(image, 0, MEM_RELEASE);
    std::cout << "PASS: five binding sites relocate, enable and restore for six action subsets; resume bytes preserved.\n";
}
