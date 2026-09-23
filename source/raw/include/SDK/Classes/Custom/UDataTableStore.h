#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>
#include "Utility/OrderedCallbacks.h"
#include "Unreal/Core/HAL/Platform.hpp"

namespace RC::Unreal {
    class UDataTable;
}

namespace UECustom {
    using DatatableSerializeCallback = std::function<void(RC::Unreal::UDataTable*)>;
    using DatatableSerializeCallbackId = RC::Unreal::uint64;

    class UDataTableRegistry {
    public:
        RC::Unreal::UDataTable* GetDatatableByName(const std::string& name);
        RC::Unreal::UDataTable* GetDatatableByPath(const std::string& path);
        std::vector<RC::Unreal::UDataTable*> GetDatatablesByName(const std::string& name);

        DatatableSerializeCallbackId RegisterDatatableSerializeCallback(const DatatableSerializeCallback& callback);
        void UnregisterDatatableSerializeCallback(const DatatableSerializeCallbackId& callbackId);

        void Add(const std::string& name, RC::Unreal::UDataTable* datatable);

        void Add(RC::Unreal::UDataTable* datatable);

    private:
        std::mutex m_mutex;
        std::recursive_mutex m_dispatchMutex;
        std::unordered_map<std::string, RC::Unreal::UDataTable*> m_datatableMap;
        std::unordered_map<std::string, RC::Unreal::UDataTable*> m_datatablePathMap;
        std::unordered_map<std::string, std::vector<RC::Unreal::UDataTable*>> m_datatablesByName;
        PS::OrderedCallbacks<DatatableSerializeCallback> m_callbacks;
    };
}
