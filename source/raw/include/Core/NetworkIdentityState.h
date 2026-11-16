#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include "Core/NetworkIdentityPlan.h"

namespace PS::NetworkIdentityState {
    inline std::mutex Gate;
    inline std::map<std::string, std::string> Lanes;
    inline std::map<std::string, std::map<std::string, std::string>> OwnerLanes;
    inline std::atomic<bool> StartupBatchFinished{false};
    inline std::string AuthorityOrderProof;
    inline std::size_t AuthorityBuildingCount=0;
    inline std::map<std::pair<std::uintptr_t,std::uintptr_t>,std::string> NativeEnrollments;
    inline bool RecordNativeEnrollment(std::uintptr_t subsystem,std::uintptr_t asset,const std::string& identity) noexcept {
        try {
            if(!subsystem || !asset || identity.empty())return false;
            std::scoped_lock lock(Gate);NativeEnrollments.insert_or_assign({subsystem,asset},identity);return true;
        }catch(...){return false;}
    }
    inline bool HasNativeEnrollment(std::uintptr_t subsystem,std::uintptr_t asset,const std::string& identity) {
        std::scoped_lock lock(Gate);const auto found=NativeEnrollments.find({subsystem,asset});
        return found!=NativeEnrollments.end() && found->second==identity;
    }
    inline void SetAuthorityOrderProof(std::string value,std::size_t buildingCount=0) { std::scoped_lock lock(Gate); AuthorityBuildingCount=value.empty()?0:buildingCount; AuthorityOrderProof=std::move(value); }
    inline std::size_t GetAuthorityBuildingCount() { std::scoped_lock lock(Gate); return AuthorityBuildingCount; }
    inline std::string GetAuthorityOrderProof() { std::scoped_lock lock(Gate); return AuthorityOrderProof; }
    inline bool MatchesAuthorityOrderProof(const std::string& remote) { std::scoped_lock lock(Gate); return remote.size()==64 && remote==AuthorityOrderProof; }
    inline bool DeferItemEnrollment() noexcept {return !StartupBatchFinished.load(std::memory_order_acquire);}
    inline void FinishStartupBatch() noexcept {StartupBatchFinished.store(true,std::memory_order_release);}

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
        AuthorityOrderProof.clear();
        AuthorityBuildingCount=0;
        NativeEnrollments.clear();
        StartupBatchFinished.store(false,std::memory_order_release);
    }
}
