#include "Generator/ToolRequest.h"

namespace PS {
std::atomic<bool> ToolRequest<SpawnToolTag>::Waiting{false};
std::atomic<bool> ToolRequest<SpawnToolTag>::Available{false};
std::atomic<bool> ToolRequest<SpawnToolTag>::CancelRequested{false};
std::atomic<uint64_t> ToolRequest<SpawnToolTag>::Generation{1};
namespace {
struct ItemIconMailboxState {
    std::mutex Mutex;
    std::deque<nlohmann::json> Pending;
    std::atomic<bool> Waiting{false};
    std::atomic<bool> Available{false};
    nlohmann::json Result={{"Status","Load a world before using runtime tools."}};
};
ItemIconMailboxState& IconMailbox() {
    static ItemIconMailboxState state;
    return state;
}
}
bool ToolRequest<ItemIconTag>::TrySubmit(nlohmann::json request) {
    auto& s=IconMailbox();
    std::lock_guard lock(s.Mutex);
    if(s.Pending.size()>=12)return false;
    const auto icon=request.value("Icon",std::string{});
    if(icon.empty())return false;
    for(const auto& queued:s.Pending)
        if(queued.value("Icon",std::string{})==icon)return true;
    s.Pending.push_back(std::move(request));
    s.Waiting.store(true);
    s.Result["Status"]="Queued for the game thread.";
    return true;
}
void ToolRequest<ItemIconTag>::Submit(nlohmann::json request) {
    (void)TrySubmit(std::move(request));
}
bool ToolRequest<ItemIconTag>::HasPending() {
    return IconMailbox().Waiting.load();
}
void ToolRequest<ItemIconTag>::SetAvailable(bool available) {
    IconMailbox().Available.store(available);
}
std::optional<nlohmann::json> ToolRequest<ItemIconTag>::Take() {
    auto& s=IconMailbox();
    if(!s.Waiting.load())return {};
    std::lock_guard lock(s.Mutex);
    if(s.Pending.empty()) { s.Waiting.store(false);return {}; }
    auto value=std::move(s.Pending.front());
    s.Pending.pop_front();
    s.Waiting.store(!s.Pending.empty());
    return value;
}
void ToolRequest<ItemIconTag>::Publish(nlohmann::json result) {
    auto& s=IconMailbox();
    std::lock_guard lock(s.Mutex);
    s.Result=std::move(result);
}
nlohmann::json ToolRequest<ItemIconTag>::Read() {
    auto& s=IconMailbox();
    std::lock_guard lock(s.Mutex);
    return s.Result;
}
void ToolRequest<ItemIconTag>::Clear() {
    auto& s=IconMailbox();
    std::lock_guard lock(s.Mutex);
    s.Pending.clear();
    s.Waiting.store(false);
    s.Result={{"Status","World changed; refresh tools."}};
}
}
