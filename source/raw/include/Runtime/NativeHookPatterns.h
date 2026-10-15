#pragma once
#include <string_view>
namespace PS::NativeHookPatterns {
// Signatures include calling-convention setup and meaningful body operations.
// Relocation/call displacements are masked, not structure offsets or registers.
// They are resolved uniquely at x64 unwind entries, not at fixed build RVAs.
inline constexpr std::string_view SteamWearableBuilder=
    "48 89 5C 24 08 44 88 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 81 EC 80 00 00 00 4C 8D B1 C0 00 00 00 BB 01 00 00 00 4C 8B E9 41 0F B6 F8 4C 8B E2 49 8B CE 41 39 5E 48";
inline constexpr std::string_view SteamWearableEvaluator=
    "48 85 C9 0F 84 ?? ?? ?? ?? 4C 89 44 24 18 53 56 57 41 56 48 81 EC 98 00 00 00 8B 41 08 49 8B F1 C1 E8 1E 4D 8B F0 F6 D0 0F B6 FA 48 8B D9 A8 01 0F 84 ?? ?? ?? ?? 4C 89 AC 24 88 00 00 00 E8 ?? ?? ?? ?? 4C 8B E8 48 85 C0";
inline constexpr std::string_view GamePassWearableBuilder=
    "44 88 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC 88 00 00 00 4C 8D B9 C0 00 00 00 BF 01 00 00 00 48 8B D9 41 0F B6 F0 4C 8B EA 49 8B CF 41 39 7F 48";
inline constexpr std::string_view GamePassWearableEvaluator=
    "48 85 C9 0F 84 ?? ?? ?? ?? 4C 89 4C 24 20 4C 89 44 24 18 56 41 56 48 81 EC B8 00 00 00 8B 41 08 49 8B F1 C1 E8 1E 4D 8B F0 F6 D0 44 0F B6 DA 4C 8B D1 A8 01 0F 84 ?? ?? ?? ?? 48 89 BC 24 A8 00 00 00 80 FA 01 75 09 48 8B B9 98 02 00 00 EB 07 48 8B B9 90 02 00 00";
inline constexpr std::string_view GamePassSpudLoad=
    "44 88 44 24 18 48 89 54 24 10 55 53 56 41 54 41 57 48 8D 6C 24 C0 48 81 EC 40 01 00 00 48 8B 59 20 45 33 FF 44 89 7D 70 48 8B F2 4C 8B E1 48 85 DB";
inline constexpr std::string_view GamePassSpudDeserialize=
    "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 A0 48 81 EC 60 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 50 48 8B BD C0 00 00 00 45 0F B6 F0 48 89 7C 24 48 48 8B F2 4C 89 4D B0 4C 8B F9 44 88 44 24 40 48 89 4D A0 E8 ?? ?? ?? ?? 84 C0";
}
