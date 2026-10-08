#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "Core/NetworkIdentityPlan.h"

namespace PS::NetworkIdentityState {
    inline std::mutex Gate;
    inline std::map<std::string, std::string> Lanes;
    inline std::map<std::string, std::map<std::string, std::string>> OwnerLanes;

    inline void Publish(const std::string& lane, const std::string& fingerprint)
    {
        std::scoped_lock lock(Gate);
        Lanes.insert_or_assign(lane, fingerprint);
    }

    inline std::string Fingerprint()
    {
        std::scoped_lock lock(Gate);
        std::vector<std::string> rows;
        rows.reserve(Lanes.size());
        for (const auto& [lane, fingerprint] : Lanes)
            rows.push_back(lane + "=" + fingerprint);
        return NetworkIdentityPlan::Fingerprint(rows);
    }

    inline std::map<std::string, std::string> Snapshot()
    {
        std::scoped_lock lock(Gate);
        return Lanes;
    }

    inline void PublishOwner(const std::string& lane, const std::string& owner,
        const std::string& fingerprint)
    {
        std::scoped_lock lock(Gate);
        OwnerLanes[owner].insert_or_assign(lane, fingerprint);
    }

    inline std::map<std::string, std::string> OwnerSnapshot()
    {
        std::scoped_lock lock(Gate);
        std::map<std::string, std::string> result;
        for (const auto& [owner, lanes] : OwnerLanes)
        {
            std::vector<std::string> rows;
            rows.reserve(lanes.size());
            for (const auto& [lane, fingerprint] : lanes)
                rows.push_back(lane + "=" + fingerprint);
            result.emplace(owner, NetworkIdentityPlan::Fingerprint(rows));
        }
        return result;
    }

    inline void Reset()
    {
        std::scoped_lock lock(Gate);
        Lanes.clear();
        OwnerLanes.clear();
    }
}
