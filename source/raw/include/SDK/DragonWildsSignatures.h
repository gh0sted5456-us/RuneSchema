#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>
#include <initializer_list>
#include "Helpers/String.hpp"

namespace DragonWilds {
    class SignatureManager {
    public:
        static void Initialize();
        static void InitializeOnly(std::initializer_list<const char*> names);

        static void* GetSignature(const std::string& ClassAndFunction);
        // Resolve a virtual UObject member from UE4SS's versioned vtable map.
        // Existing providers retain priority; validated runtime addresses are cached.
        static void* ResolveUObjectVirtual(const std::string& binding,
            const RC::StringType& classPath, std::initializer_list<const RC::CharType*> members);
        static std::string GetSource(const std::string& binding);
    private:
        static inline std::unordered_map<std::string, void*> SignatureMap;
        static inline std::unordered_map<std::string, std::string> SourceMap;

        static inline std::unordered_map<std::string, std::string> SteamSignatures {

            { "FPakPlatformFile::GetPakFolders", "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 40 48 8D 4D F0 48 8B DA" },

            { "UObjectGlobals::StaticFindObject", "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 04 00 00" },

            { "FName::Constructor", "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 41 8B F8 33 C9 4C 8B DA 44 8B D1 4C 8B CA 48 85 D2" },

            { "FMemory::Free", "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? ?? ?? ?? ?? 48 85 C9 75 0C E8 ?? ?? ?? ?? 48 8B" },

            { "UDataTable::Serialize", "48 89 5C 24 18 57 48 81 EC E0 01 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 84 24 D8 01 00 00" },
        };
        static inline std::unordered_map<std::string, std::string> SteamSignaturesCallResolve {

            { "GetObjectsOfClass", "E8 ?? ?? ?? ?? 4C 89 36 48 8D 4D D7 48 8B D3 4C 89 76 08" },
            { "FName::ToString_Wchar", "E8 ?? ?? ?? ?? BE 01 00 00 00 39 75 48 0F 8E ?? ?? ?? ?? 4C 89 B4 24 80 00 00 00" },
        };

        // RuneScape: Dragonwilds 100.4.0.0 WinGDK/UE 5.6.1. These patterns
        // were verified against the decrypted executable image mapped by
        // Gaming Services. Keep them isolated from the Win64 lane.
        static inline std::unordered_map<std::string, std::string> GamePassSignatures {
            { "FPakPlatformFile::GetPakFolders", "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 40 48 8D 4D F0 48 8B DA" },
            { "UObjectGlobals::StaticFindObject", "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 04 00 00" },
            { "FName::Constructor", "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 41 8B F8 33 C9 4C 8B DA 44 8B D1 4C 8B CA 48 85 D2" },
            { "FMemory::Free", "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? ?? ?? ?? ?? 48 85 C9 75 0C E8 F2 FD FF FF 48 8B" },
            { "UDataTable::Serialize", "48 89 5C 24 10 57 48 81 EC D0 01 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 84 24 C8 01 00 00 48 8D 05" },
            { "FName::ToString_Wchar", "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 30 48 8B F1 48 8B FA 8B 09 E8 ?? ?? ?? ?? 48 8B D8 48 8B CF" },
            // WinGDK's similarly shaped object iterator accepts a callback
            // object, not a TArray output parameter. Calling it as
            // GetObjectsOfClass executes the array's null data pointer. Use
            // UE4SS's initialized hash tables/object array fallback instead.
        };
        static inline std::unordered_map<std::string, std::string> GamePassSignaturesCallResolve {
            { "FFieldClass::GetNameToFieldClassMap", "E8 ?? ?? ?? ?? 4C 8B D0 8B 45 38 44 0F B7 C8 8B C8 48 8B 45 38 45 8B D9 C1 E9 10" },
        };
    };
}
