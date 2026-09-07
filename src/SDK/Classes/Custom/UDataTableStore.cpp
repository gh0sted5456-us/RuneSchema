#include "SDK/Classes/Custom/UDataTableStore.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Helpers/String.hpp"

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
        }
        for (const auto& callback : m_callbacks.Snapshot()) callback(datatable);
    }

    void UDataTableRegistry::Add(RC::Unreal::UDataTable* datatable)
    {
        auto name = datatable->GetNamePrivate().ToString();
        Add(RC::to_string(name), datatable);
    }

}
