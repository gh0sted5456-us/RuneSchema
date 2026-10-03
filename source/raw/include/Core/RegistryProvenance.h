#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace PS::RegistryProvenance {
    inline std::mutex& ItemMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline std::unordered_set<std::string>& RuneSchemaItemIds()
    {
        static std::unordered_set<std::string> ids;
        return ids;
    }

    inline void AnnounceRuneSchemaItem(std::string id)
    {
        if (id.empty()) return;
        std::scoped_lock lock(ItemMutex());
        RuneSchemaItemIds().insert(std::move(id));
    }

    inline bool IsRuneSchemaItem(std::string_view id)
    {
        std::scoped_lock lock(ItemMutex());
        return RuneSchemaItemIds().contains(std::string(id));
    }

    inline std::vector<std::string> RuneSchemaItems()
    {
        std::scoped_lock lock(ItemMutex());
        return {RuneSchemaItemIds().begin(), RuneSchemaItemIds().end()};
    }
}
