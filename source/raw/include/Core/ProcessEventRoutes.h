#pragma once
// Name-keyed routing for ProcessEvent handlers.
//
// Each UE4SS ProcessEvent callback runs on every game function call. Handlers
// that only care about a fixed set of function names register here instead,
// and one shared callback per phase looks the name up once.
//
// Hot path for a name nobody routes: one relaxed atomic load and a bit test.
// For a routed name: a reader count, a table pointer load and a hash lookup.
//
// Handlers for the same name run in the order they were added. Add and Remove
// may be called from any thread, including from inside a handler: a dispatch
// already in progress keeps reading the table it started with, and a removed
// handler is skipped from then on. Old tables are freed once no dispatch is
// reading them.
//
// Portable (no Unreal types) so it can be tested without the game.
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PS {
template<class Key, class Handler, class Hash = std::hash<Key>>
class ProcessEventRoutes {
public:
    using Id = std::uint64_t;
    static constexpr std::size_t MaximumNames = 512;
    static constexpr std::size_t MaximumHandlers = 256;

    ProcessEventRoutes() = default;
    ProcessEventRoutes(const ProcessEventRoutes&) = delete;
    ProcessEventRoutes& operator=(const ProcessEventRoutes&) = delete;
    ~ProcessEventRoutes() { delete m_table.load(); }

    // Returns 0 when there are no names, too many names or handlers, or no handler.
    Id Add(std::vector<Key> names, std::string label, Handler handler) {
        if (names.empty() || names.size() > MaximumNames || !handler) return 0;
        std::lock_guard lock(m_gate);
        if (m_registrations.size() >= MaximumHandlers) return 0;
        Registration registration;
        registration.Names = std::move(names);
        registration.Entry.RouteId = m_next++;
        registration.Entry.Call = std::make_shared<const Handler>(std::move(handler));
        registration.Entry.Label = std::make_shared<const std::string>(std::move(label));
        registration.Entry.Live = std::make_shared<std::atomic<bool>>(true);
        const auto id = registration.Entry.RouteId;
        m_registrations.push_back(std::move(registration));
        Publish();
        return id;
    }

    // Safe to call with 0 or an id that was already removed.
    bool Remove(Id id) {
        if (!id) return false;
        std::lock_guard lock(m_gate);
        for (auto it = m_registrations.begin(); it != m_registrations.end(); ++it) {
            if (it->Entry.RouteId != id) continue;
            it->Entry.Live->store(false, std::memory_order_release);
            m_registrations.erase(it);
            Publish();
            return true;
        }
        return false;
    }

    // Calls invoke(handler, label) for each live handler routed to `name`.
    template<class Invoke>
    void Dispatch(const Key& name, Invoke&& invoke) const {
        const auto hash = Hash{}(name);
        if (!(m_mask.load(std::memory_order_relaxed) & Bit(hash))) return;
        Reader reader(m_readers);
        const Table* table = m_table.load(std::memory_order_seq_cst);
        if (!table) return;
        const auto found = table->Routes.find(name);
        if (found == table->Routes.end()) return;
        for (const auto& route : found->second)
            if (route.Live->load(std::memory_order_acquire)) invoke(*route.Call, *route.Label);
    }

    bool Empty() const { std::lock_guard lock(m_gate); return m_registrations.empty(); }
    std::size_t Handlers() const { std::lock_guard lock(m_gate); return m_registrations.size(); }
    std::size_t RetainedTables() const { std::lock_guard lock(m_gate); return m_retired.size(); }

private:
    struct Route {
        Id RouteId = 0;
        std::shared_ptr<const Handler> Call;
        std::shared_ptr<const std::string> Label;
        std::shared_ptr<std::atomic<bool>> Live;
    };
    struct Registration { std::vector<Key> Names; Route Entry; };
    struct Table { std::unordered_map<Key, std::vector<Route>, Hash> Routes; std::uint64_t Mask = 0; };
    struct Reader {
        std::atomic<std::uint64_t>& Count;
        explicit Reader(std::atomic<std::uint64_t>& count) : Count(count) { Count.fetch_add(1, std::memory_order_seq_cst); }
        ~Reader() { Count.fetch_sub(1, std::memory_order_seq_cst); }
    };

    static std::uint64_t Bit(std::size_t hash) {
        const auto mixed = static_cast<std::uint64_t>(hash) ^ (static_cast<std::uint64_t>(hash) >> 17) ^ (static_cast<std::uint64_t>(hash) >> 31);
        return std::uint64_t{1} << (mixed & 63);
    }

    // Caller holds m_gate.
    void Publish() {
        auto next = std::make_unique<Table>();
        for (const auto& registration : m_registrations)
            for (const auto& name : registration.Names) {
                auto& routes = next->Routes[name];
                // A handler listed under the same name twice still runs once.
                bool present = false;
                for (const auto& route : routes) if (route.RouteId == registration.Entry.RouteId) { present = true; break; }
                if (!present) routes.push_back(registration.Entry);
                next->Mask |= Bit(Hash{}(name));
            }
        const Table* previous = nullptr;
        if (next->Routes.empty()) {
            previous = m_table.exchange(nullptr, std::memory_order_seq_cst);
            m_mask.store(0, std::memory_order_relaxed);
        } else {
            const auto mask = next->Mask;
            // Widen the mask before the new table appears so a new name is not skipped,
            // then narrow it to the table's own names.
            m_mask.fetch_or(mask, std::memory_order_relaxed);
            previous = m_table.exchange(next.release(), std::memory_order_seq_cst);
            m_mask.store(mask, std::memory_order_relaxed);
        }
        if (previous) m_retired.emplace_back(previous);
        // A reader that registers after this check loads the table published above.
        if (m_readers.load(std::memory_order_seq_cst) == 0) m_retired.clear();
    }

    mutable std::mutex m_gate;
    std::vector<Registration> m_registrations;
    Id m_next = 1;
    std::atomic<std::uint64_t> m_mask{0};
    std::atomic<const Table*> m_table{nullptr};
    mutable std::atomic<std::uint64_t> m_readers{0};
    std::vector<std::unique_ptr<const Table>> m_retired;
};
}
