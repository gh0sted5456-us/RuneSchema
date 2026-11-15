#pragma once
// Helpy's own thread for talking to RuneSchema. The UI thread posts requests and later drains replies; it
// never waits on the core, the game thread or the desktop pipe. Requests that share a key coalesce, so a
// burst of visibility changes sends only the newest. Replies run on the thread that calls Drain().
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace PS::HelpyCoreLink {
using Json = nlohmann::json;
using Remote = std::function<bool(const Json&, Json&)>;
using Done = std::function<void(bool, const Json&)>;
using Clock = std::chrono::steady_clock;

struct Stats { uint64_t Posted = 0, Coalesced = 0, Executed = 0, Failed = 0; double SlowestMs = 0; size_t Queued = 0; };

class Link {
    struct Job { std::string Key; Json Request; Remote Call; Done Callback; };
    struct Reply { std::string Key; bool Ok = false; Json Value; Done Callback; };
    mutable std::mutex m_gate;
    std::condition_variable m_wake;
    std::deque<Job> m_queue;
    std::deque<Reply> m_done;
    std::string m_running;
    std::thread m_worker;
    bool m_stop = false;
    Stats m_stats;
    static constexpr size_t MaximumQueue = 64;

    void Run() {
#if defined(_WIN32)
        SetThreadDescription(GetCurrentThread(), L"Helpy core link");
#endif
        for (;;) {
            Job job;
            {
                std::unique_lock lock(m_gate);
                m_wake.wait(lock, [&] { return m_stop || !m_queue.empty(); });
                if (m_stop) return;
                job = std::move(m_queue.front()); m_queue.pop_front(); m_running = job.Key;
            }
            Json value; bool ok = false;
            const auto started = Clock::now();
            try { ok = job.Call && job.Call(job.Request, value); } catch (...) { ok = false; }
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
            std::lock_guard lock(m_gate);
            m_running.clear(); ++m_stats.Executed; if (!ok) ++m_stats.Failed; m_stats.SlowestMs = std::max(m_stats.SlowestMs, ms);
            if (job.Callback) { m_done.push_back({std::move(job.Key), ok, std::move(value), std::move(job.Callback)}); if (m_done.size() > MaximumQueue) m_done.pop_front(); }
        }
    }
public:
    Link() = default;
    Link(const Link&) = delete; Link& operator=(const Link&) = delete;
    ~Link() { Stop(); }
    void Start() {
        std::lock_guard lock(m_gate);
        if (m_worker.joinable()) return;
        m_stop = false;
        m_worker = std::thread([this] { Run(); });
    }
    // Stops after the request in flight; queued requests and undelivered replies are dropped.
    void Stop() {
        { std::lock_guard lock(m_gate); m_stop = true; m_queue.clear(); }
        m_wake.notify_all();
        if (m_worker.joinable() && m_worker.get_id() != std::this_thread::get_id()) m_worker.join();
        std::lock_guard lock(m_gate); m_done.clear(); m_running.clear();
    }
    // Queues `request` on the link thread. A pending request with the same non-empty key is replaced.
    bool Post(std::string key, Json request, Remote call, Done callback = {}) {
        if (!call) return false;
        {
            std::lock_guard lock(m_gate);
            if (m_stop || !m_worker.joinable()) return false;
            ++m_stats.Posted;
            if (!key.empty()) {
                const auto found = std::find_if(m_queue.begin(), m_queue.end(), [&](const Job& job) { return job.Key == key; });
                if (found != m_queue.end()) { found->Request = std::move(request); found->Call = std::move(call); found->Callback = std::move(callback); ++m_stats.Coalesced; return true; }
            }
            if (m_queue.size() >= MaximumQueue) return false;
            m_queue.push_back({std::move(key), std::move(request), std::move(call), std::move(callback)});
        }
        m_wake.notify_one();
        return true;
    }
    // True while a request with this key is queued or running (use it to avoid piling up polls).
    bool Busy(std::string_view key) const {
        std::lock_guard lock(m_gate);
        return m_running == key || std::any_of(m_queue.begin(), m_queue.end(), [&](const Job& job) { return job.Key == key; });
    }
    // Runs completed callbacks on the calling (UI) thread. Bounded so one frame never does unbounded work.
    size_t Drain(size_t maximum = 32) {
        std::deque<Reply> ready;
        { std::lock_guard lock(m_gate); while (!m_done.empty() && ready.size() < maximum) { ready.push_back(std::move(m_done.front())); m_done.pop_front(); } }
        for (auto& reply : ready) try { reply.Callback(reply.Ok, reply.Value); } catch (...) {}
        return ready.size();
    }
    Stats Snapshot() const { std::lock_guard lock(m_gate); auto stats = m_stats; stats.Queued = m_queue.size(); return stats; }
};
}
