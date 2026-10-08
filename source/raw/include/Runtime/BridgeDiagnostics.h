#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "nlohmann/json.hpp"

namespace PS::Network {

// Process-local, observational bridge telemetry. It never participates in a
// gameplay decision and is cleared at every world boundary.
class BridgeDiagnostics {
public:
    static constexpr uint32_t SchemaVersion = 1;
    static constexpr size_t GlobalCapacity = 256;
    static constexpr size_t CorrelationCapacity = 32;
    static constexpr size_t CounterCapacity = 256;
    static constexpr size_t SuppressionCapacity = 128;
    static constexpr size_t PendingCapacity = 128;

    struct Event {
        uint32_t schemaVersion = SchemaVersion;
        uint64_t monotonicMs = 0;
        uint64_t worldEpoch = 0;
        std::string networkRole, side, channel, owner, entity, action;
        int64_t requestRevision = 0;
        std::string correlationId, stage, outcome, reasonCode;
        uint32_t retryCount = 0, suppressedCount = 0;
        uint64_t elapsedMs = 0;
    };

#if !defined(NDEBUG)
    enum class Fault {None,Delay,Drop,Reorder,Reject};
    void SetDevelopmentFault(Fault fault) {std::scoped_lock lock(m_mutex);m_fault=fault;}
    Fault DevelopmentFault() const {std::scoped_lock lock(m_mutex);return m_fault;}
#endif

    void BeginWorld(std::string role, std::string build, int protocol,
        std::string registryFingerprint, std::string mappingState, bool verbose) {
        std::scoped_lock lock(m_mutex);
        ++m_epoch; m_role=Bound(role,24);m_build=Bound(build,32);m_protocol=protocol;
        m_registry=Bound(registryFingerprint,96);m_mapping=Bound(mappingState,32);m_verbose=verbose;
        m_events.clear();m_counters.clear();m_suppressed.clear();m_started=Clock::now();
        m_latencyTotal=0;m_latencyMax=0;m_terminalCount=0;m_counterOverflow=0;m_pending.clear();
    }

    void Reset() {
        std::scoped_lock lock(m_mutex);m_events.clear();m_counters.clear();m_suppressed.clear();
        m_pending.clear();m_latencyTotal=0;m_latencyMax=0;m_terminalCount=0;m_counterOverflow=0;
    }

    std::string Correlation(uint64_t bridgeInstance,int64_t revision) const {
        std::scoped_lock lock(m_mutex);
        return CorrelationFrom(m_epoch,bridgeInstance,revision);
    }
    uint64_t WorldEpoch() const {std::scoped_lock lock(m_mutex);return m_epoch;}
    static std::string CorrelationFrom(uint64_t epoch,uint64_t bridgeInstance,int64_t revision) {
        return std::to_string(epoch)+"-"+std::to_string(bridgeInstance)+"-"+std::to_string(std::max<int64_t>(0,revision));
    }

    void Record(Event event, bool warning=false) {
        std::scoped_lock lock(m_mutex);
        event.schemaVersion=SchemaVersion;event.worldEpoch=m_epoch;
        event.monotonicMs=Ms(Clock::now()-m_started);
        event.networkRole=Bound(event.networkRole.empty()?m_role:event.networkRole,24);
        event.side=Token(event.side,24);event.channel=Token(event.channel,64);
        event.owner=Token(event.owner,64);event.entity=Token(event.entity,128);
        event.action=Token(event.action,48);event.correlationId=Token(event.correlationId,96);
        event.stage=Token(event.stage,64);event.outcome=Token(event.outcome,32);
        event.reasonCode=Token(event.reasonCode,64);
        const auto counter=event.channel+"|"+event.stage+"|"+event.outcome;
        if(m_counters.contains(counter)||m_counters.size()<CounterCapacity)++m_counters[counter];else ++m_counterOverflow;
        const auto pendingKey=event.correlationId.empty()?event.channel+"|"+event.owner+"|"+event.entity:event.correlationId;
        if(event.outcome=="pending"||event.outcome=="deferred") {if(m_pending.contains(pendingKey)||m_pending.size()<PendingCapacity)m_pending.insert(pendingKey);}
        if(event.outcome=="executed"||event.outcome=="rejected"||event.outcome=="expired"||event.outcome=="cancelled") {
            m_pending.erase(pendingKey);m_latencyTotal+=event.elapsedMs;m_latencyMax=std::max(m_latencyMax,event.elapsedMs);++m_terminalCount;
        }
        if(!m_verbose&&!warning)return;
        const auto duplicate=event.channel+"|"+event.owner+"|"+event.entity+"|"+event.stage+"|"+event.outcome+"|"+event.reasonCode;
        if(!m_suppressed.contains(duplicate)&&m_suppressed.size()>=SuppressionCapacity)m_suppressed.erase(m_suppressed.begin());
        auto& suppression=m_suppressed[duplicate];
        if(!m_verbose&&suppression.Last.time_since_epoch().count()!=0&&Clock::now()-suppression.Last<std::chrono::seconds(5)) {
            ++suppression.Count;return;
        }
        event.suppressedCount=suppression.Count;suppression={Clock::now(),0};
        if(!event.correlationId.empty()) {
            size_t same=0;
            for(auto at=m_events.rbegin();at!=m_events.rend();++at)if(at->correlationId==event.correlationId&&++same>=CorrelationCapacity) {
                const auto index=static_cast<size_t>(std::distance(at,m_events.rend())-1);m_events.erase(m_events.begin()+index);break;
            }
        }
        if(m_events.size()>=GlobalCapacity)m_events.pop_front();
        m_events.push_back(std::move(event));
    }

    nlohmann::json Snapshot() const {
        std::scoped_lock lock(m_mutex);nlohmann::json events=nlohmann::json::array();
        for(const auto& e:m_events)events.push_back({{"schemaVersion",e.schemaVersion},{"monotonicMs",e.monotonicMs},
            {"worldEpoch",e.worldEpoch},{"networkRole",e.networkRole},{"side",e.side},{"channel",e.channel},
            {"owner",e.owner},{"entity",e.entity},{"action",e.action},{"requestRevision",e.requestRevision},
            {"correlationId",e.correlationId},{"stage",e.stage},{"outcome",e.outcome},{"reasonCode",e.reasonCode},
            {"retryCount",e.retryCount},{"elapsedMs",e.elapsedMs},{"suppressedCount",e.suppressedCount}});
        return {{"schemaVersion",SchemaVersion},{"worldEpoch",m_epoch},{"worldRole",m_role},{"build",m_build},
            {"protocolVersion",m_protocol},{"registryFingerprint",m_registry},{"mappingState",m_mapping},
            {"verbose",m_verbose},{"events",std::move(events)},{"counters",m_counters},{"counterOverflow",m_counterOverflow},{"activePending",m_pending.size()},
            {"averageEndToEndMs",m_terminalCount?m_latencyTotal/m_terminalCount:0},{"maxEndToEndMs",m_latencyMax}};
    }

private:
    using Clock=std::chrono::steady_clock;
    struct Suppression {Clock::time_point Last{};uint32_t Count=0;};
    static uint64_t Ms(Clock::duration value) {return static_cast<uint64_t>(std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(value).count()));}
    static std::string Bound(std::string_view value,size_t limit) {return std::string(value.substr(0,limit));}
    static std::string Token(std::string_view value,size_t limit) {
        std::string out;out.reserve(std::min(limit,value.size()));
        for(char c:value) {if(out.size()>=limit)break;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c==':'||c=='_'||c=='-')out.push_back(c);else out.push_back('_');}
        return out;
    }
    mutable std::mutex m_mutex;
    uint64_t m_epoch=0,m_latencyTotal=0,m_latencyMax=0,m_terminalCount=0,m_counterOverflow=0;
    std::string m_role="unknown",m_build,m_registry,m_mapping;int m_protocol=0;bool m_verbose=false;
    Clock::time_point m_started=Clock::now();std::deque<Event> m_events;
    std::unordered_map<std::string,uint64_t> m_counters;std::unordered_map<std::string,Suppression> m_suppressed;
    std::unordered_set<std::string> m_pending;
#if !defined(NDEBUG)
    Fault m_fault=Fault::None;
#endif
};
}
