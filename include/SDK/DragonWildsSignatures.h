#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>

namespace DragonWilds {
    class SignatureManager {
    public:
        static void Initialize();
        
        static void* GetSignature(const std::string& ClassAndFunction);
    private:
        static inline std::unordered_map<std::string, void*> SignatureMap;

        static inline std::unordered_map<std::string, std::string> Signatures {

            { "FPakPlatformFile::GetPakFolders", "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 40 48 8D 4D F0 48 8B DA" },

            { "UObjectGlobals::StaticFindObject", "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 04 00 00" },

            { "FField::IsA", "48 8B 41 08 48 8B 4A 08 48 85 C9 74 08 48 85 48 10 0F 95 C0 C3" },

            { "FName::Constructor", "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 41 8B F8 33 C9 4C 8B DA 44 8B D1 4C 8B CA 48 85 D2" },

            { "FMemory::Free", "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? ?? ?? ?? ?? 48 85 C9 75 0C E8 ?? ?? ?? ?? 48 8B" },

            { "UDataTable::Serialize", "48 89 5C 24 18 57 48 81 EC E0 01 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 84 24 D8 01 00 00" },
        };
        static inline std::unordered_map<std::string, std::string> SignaturesCallResolve {

            { "FFieldClass::GetNameToFieldClassMap", "E8 ?? ?? ?? ?? 48 8B 5C 24 50 48 8B E8 8B 4C 24 50 48 8B FB 48 C1 EF 20" },

            { "FName::ToString_Wchar", "E8 ?? ?? ?? ?? BE 01 00 00 00 39 75 48 0F 8E ?? ?? ?? ?? 4C 89 B4 24 80 00 00 00" },

            { "GetObjectsOfClass", "E8 ?? ?? ?? ?? 4C 89 36 48 8D 4D D7 48 8B D3 4C 89 76 08" },
        };
    };
}
