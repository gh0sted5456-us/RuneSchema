#include <Windows.h>
#include <safetyhook.hpp>
#include "Generator/AppearanceTraceContract.h"
#include <array>
#include <cstring>
#include <iostream>
using namespace PS::AppearanceTraceContract;
void Observe(safetyhook::Context&) {}
int main() {
    // Private synthetic image, never executed. No game process is opened.
    auto* image = static_cast<unsigned char*>(VirtualAlloc(nullptr, ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!image) return 2;
    std::array<safetyhook::MidHook, std::size(Sites)> hooks;
    for (const auto& site : Sites) {
        std::memcpy(image + site.rva, site.bytes.data(), site.bytes.size());
    }
    for (size_t i = 0; i < hooks.size(); ++i) {
        auto hook = safetyhook::MidHook::create(image + Sites[i].rva, Observe, safetyhook::MidHook::StartDisabled);
        if (!hook) { std::cerr << "Cannot relocate site " << i << "\n"; return 3; }
        hooks[i] = std::move(*hook);
    }
    for (auto& hook : hooks) if (!hook.enable()) return 4;
    for (auto& hook : hooks) hook = {};
    for (const auto& site : Sites) {
        if (std::memcmp(image + site.rva, site.bytes.data(), site.bytes.size())) return 5;
    }
    VirtualFree(image, 0, MEM_RELEASE);
    std::cout << "PASS: all 5 appearance trace hook sequences relocate, enable and restore.\n";
}
