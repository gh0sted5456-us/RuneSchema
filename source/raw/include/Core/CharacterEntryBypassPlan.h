#pragma once
#include <cstdint>
#include <array>
#include <algorithm>
#include <optional>
#include <span>
#include <string_view>
namespace PS::CharacterEntryBypassPlan {
// Verified current Win64 and WinGDK native entry shapes. Displacements and
// stack sizes are masked; exact unwind entries, uniqueness and return control
// flow are independently checked. These bindings are used only when opted in.
inline constexpr std::array<std::string_view,2> PlayerStatePatterns={
    "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 4D 8B E1 8B DA 45 33 F6 4C 8D 4D ?? 49 8B D4 44 89 75 ?? 4D 8B F8 48 8B F1 E8 ?? ?? ?? ?? 84 C0",
    "4C 89 44 24 ?? 55 53 56 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 4D 8B F1 48 89 45 ?? 8B DA 8B 05 ?? ?? ?? ?? 4C 8B E9 85 C0 74 04 FF C8 EB 02 33 C0 49 8B 09"
};
enum class Kind { Other, Restore, StackRestore, Return, Branch, Invalid };
struct Instruction {std::uint32_t Offset,Size;Kind Type;std::int64_t Target=0;};
// Input is produced by a full x64 decoder, never a raw search for RET bytes.
// A single normal exit must be reached through an AL-preserving restore tail.
inline std::optional<std::uint32_t> FindReturn(std::span<const Instruction> code,std::uint32_t bodySize) {
    if(code.empty()||!bodySize||bodySize>65536)return {};
    std::uint32_t next=0;std::optional<std::size_t> exit;
    for(std::size_t i=0;i<code.size();++i) {
        const auto& op=code[i];
        if(op.Offset!=next||!op.Size||op.Size>15||op.Size>bodySize-next||op.Type==Kind::Invalid)return {};
        next+=op.Size;
        if(op.Type==Kind::Return){if(exit)return {};exit=i;}
    }
    if(next!=bodySize||!exit)return {};
    auto gate=*exit;bool stack=false;
    while(gate&&(code[gate-1].Type==Kind::Restore||code[gate-1].Type==Kind::StackRestore)) {
        --gate;stack|=code[gate].Type==Kind::StackRestore;
    }
    if(!stack||code[*exit].Offset-code[gate].Offset<5)return {};
    for(const auto& op:code)if(op.Type==Kind::Branch) {
        if(op.Target<0||op.Target>=bodySize)return {}; // no unknown tail exits
        const auto target=std::lower_bound(code.begin(),code.end(),op.Target,
            [](const Instruction& item,std::int64_t offset){return item.Offset<offset;});
        if(target==code.end()||target->Offset!=op.Target)return {};
        if(op.Target>code[gate].Offset&&op.Target<=code[*exit].Offset)return {};
    }
    return code[gate].Offset;
}
inline std::uintptr_t AcceptRejected(std::uintptr_t result) noexcept {return (result&0xffu)?result:(result|std::uintptr_t{1});}
}
