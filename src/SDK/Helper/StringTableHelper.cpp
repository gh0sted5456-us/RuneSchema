#include <Windows.h>

#include "Unreal/FString.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/StringTableHelper.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace {
    constexpr unsigned StringTableOwnerAssetOffset = 0x10;
    constexpr unsigned StringTableKeysToEntriesOffset = 0x20;
    constexpr unsigned EntryOwnerTableOffset = 0x00;
    constexpr unsigned EntrySourceStringOffset = 0x10;
    constexpr unsigned KeysToEntriesStride = 32;

    constexpr unsigned SparseArrayFlagsOffset = 0x10;
    constexpr unsigned BitArraySecondaryOffset = 0x10;
    constexpr unsigned BitArrayNumBitsOffset = 0x18;

    constexpr int32_t MaxPlausibleStringLength = 1000000;

    int g_tableOffset = -1;
    int g_entryOffset = -1;

    bool IsReadable(const void* address, size_t size)
    {
        if (!address || size == 0)
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION info{};
        if (::VirtualQuery(address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT)
        {
            return false;
        }

        constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
                                 | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((info.Protect & readable) == 0 || (info.Protect & PAGE_GUARD) != 0)
        {
            return false;
        }

        const auto regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        return reinterpret_cast<uintptr_t>(address) + size <= regionEnd;
    }

    bool LooksLikeFString(const void* candidate)
    {
        if (!IsReadable(candidate, 16))
        {
            return false;
        }

        const auto* bytes = static_cast<const uint8_t*>(candidate);
        const void* data = *reinterpret_cast<void* const*>(bytes);
        const int32_t num = *reinterpret_cast<const int32_t*>(bytes + 8);
        const int32_t max = *reinterpret_cast<const int32_t*>(bytes + 12);

        if (num < 0 || max < num || max > MaxPlausibleStringLength)
        {
            return false;
        }

        if (num == 0)
        {
            return true;
        }

        return data != nullptr && IsReadable(data, static_cast<size_t>(num) * sizeof(wchar_t));
    }

    bool ResolveTableOffset(UObject* asset)
    {
        for (int Candidate = 0x28; Candidate <= 0x60; Candidate += 8)
        {
            auto* Slot = reinterpret_cast<uint8_t*>(asset) + Candidate;
            if (!IsReadable(Slot, sizeof(void*)))
            {
                continue;
            }

            auto* Table = *reinterpret_cast<uint8_t* const*>(Slot);
            if (!IsReadable(Table, StringTableKeysToEntriesOffset + 0x50))
            {
                continue;
            }

            if (*reinterpret_cast<UObject* const*>(Table + StringTableOwnerAssetOffset) == asset)
            {
                g_tableOffset = Candidate;
                return true;
            }
        }

        return false;
    }

    bool ResolveEntryOffset(uint8_t* element, const void* table)
    {
        for (int Candidate : {8, 0})
        {
            auto* Entry = *reinterpret_cast<uint8_t* const*>(element + Candidate);
            if (!IsReadable(Entry, EntrySourceStringOffset + 16))
            {
                continue;
            }

            if (*reinterpret_cast<const void* const*>(Entry + EntryOwnerTableOffset) == table &&
                LooksLikeFString(Entry + EntrySourceStringOffset))
            {
                g_entryOffset = Candidate;
                return true;
            }
        }

        return false;
    }
}

namespace DragonWilds::StringTableHelper {
    void ForEachEntry(const std::function<void(UObject*, FString&)>& callback)
    {
        auto* StringTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, STR("/Script/Engine.StringTable"));

        if (!StringTableClass)
        {
            throw std::runtime_error("Failed to find the StringTable class. The class may have been renamed or removed.");
        }

        TArray<UObject*> Assets{};
        UECustom::UObjectGlobals::GetObjectsOfClass(StringTableClass, Assets, true);

        for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
        {
            auto* Asset = Assets[AssetIndex];
            if (!Asset)
            {
                continue;
            }

            if (g_tableOffset < 0 && !ResolveTableOffset(Asset))
            {
                continue;
            }

            auto* Slot = reinterpret_cast<uint8_t*>(Asset) + g_tableOffset;
            if (!IsReadable(Slot, sizeof(void*)))
            {
                continue;
            }

            auto* Table = *reinterpret_cast<uint8_t* const*>(Slot);
            if (!IsReadable(Table, StringTableKeysToEntriesOffset + 0x50) ||
                *reinterpret_cast<UObject* const*>(Table + StringTableOwnerAssetOffset) != Asset)
            {
                continue;
            }

            auto* Sparse = Table + StringTableKeysToEntriesOffset;
            auto* Elements = *reinterpret_cast<uint8_t* const*>(Sparse);
            const int32_t ElementCount = *reinterpret_cast<const int32_t*>(Sparse + 8);

            auto* FlagsBase = Sparse + SparseArrayFlagsOffset;
            auto* SecondaryFlags = *reinterpret_cast<uint32_t* const*>(FlagsBase + BitArraySecondaryOffset);
            const int32_t BitCount = *reinterpret_cast<const int32_t*>(FlagsBase + BitArrayNumBitsOffset);
            const uint32_t* Flags = SecondaryFlags ? SecondaryFlags : reinterpret_cast<const uint32_t*>(FlagsBase);

            if (ElementCount <= 0 || !Elements || BitCount < ElementCount)
            {
                continue;
            }

            if (!IsReadable(Elements, static_cast<size_t>(ElementCount) * KeysToEntriesStride) ||
                !IsReadable(Flags, static_cast<size_t>((ElementCount + 31) / 32) * sizeof(uint32_t)))
            {
                continue;
            }

            for (int32_t Index = 0; Index < ElementCount; ++Index)
            {
                if ((Flags[Index >> 5] & (1u << (Index & 31))) == 0)
                {
                    continue;
                }

                auto* Element = Elements + static_cast<size_t>(Index) * KeysToEntriesStride;

                if (g_entryOffset < 0 && !ResolveEntryOffset(Element, Table))
                {
                    continue;
                }

                auto* Entry = *reinterpret_cast<uint8_t* const*>(Element + g_entryOffset);
                if (!IsReadable(Entry, EntrySourceStringOffset + 16) ||
                    *reinterpret_cast<const void* const*>(Entry + EntryOwnerTableOffset) != Table)
                {
                    continue;
                }

                auto* SourceString = reinterpret_cast<FString*>(Entry + EntrySourceStringOffset);
                if (LooksLikeFString(SourceString))
                {
                    callback(Asset, *SourceString);
                }
            }
        }
    }
}
