#include "SDK/Classes/Custom/UDataTableStore.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Helpers/String.hpp"
#include <algorithm>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    RC::Unreal::UDataTable* UDataTableRegistry::GetDatatableByName(const std::string& name)
    {
        std::lock_guard guard(m_mutex);
        auto datatable = m_datatableMap.find(name);
        if (datatable != m_datatableMap.end())
        {
            return datatable->second;
        }

        return nullptr;
    }

    RC::Unreal::UDataTable* UDataTableRegistry::GetDatatableByPath(const std::string& path)
    {
        std::lock_guard guard(m_mutex);
        const auto found = m_datatablePathMap.find(path);
        return found == m_datatablePathMap.end() ? nullptr : found->second;
    }

    std::vector<RC::Unreal::UDataTable*> UDataTableRegistry::GetDatatablesByName(const std::string& name)
    {
        std::lock_guard guard(m_mutex);
        const auto found = m_datatablesByName.find(name);
        return found == m_datatablesByName.end() ? std::vector<RC::Unreal::UDataTable*>{} : found->second;
    }

    DatatableSerializeCallbackId UDataTableRegistry::RegisterDatatableSerializeCallback(const DatatableSerializeCallback& callback)
    {
        return m_callbacks.Add(callback);
    }

    void UDataTableRegistry::UnregisterDatatableSerializeCallback(const DatatableSerializeCallbackId& callbackId)
    {
        m_callbacks.Remove(callbackId);
    }

    void UDataTableRegistry::Add(const std::string& name, RC::Unreal::UDataTable* datatable)
    {
        // Allow recursive table loads on the same thread.
        std::lock_guard dispatch(m_dispatchMutex);
        {
            std::lock_guard guard(m_mutex);
            m_datatableMap.insert_or_assign(name, datatable);
            const auto path = RC::to_string(datatable->GetPathName());
            m_datatablePathMap.insert_or_assign(path, datatable);
            auto& candidates = m_datatablesByName[name];
            if (std::ranges::find(candidates, datatable) == candidates.end()) candidates.push_back(datatable);
        }
        for (const auto& callback : m_callbacks.Snapshot()) callback(datatable);
    }

    void UDataTableRegistry::Add(RC::Unreal::UDataTable* datatable)
    {
        auto name = datatable->GetNamePrivate().ToString();
        Add(RC::to_string(name), datatable);
    }

}
