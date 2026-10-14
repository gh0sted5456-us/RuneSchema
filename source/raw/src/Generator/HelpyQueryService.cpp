#include "Generator/HelpyQueryService.h"
#include "Generator/ToolRequest.h"
#include <algorithm>
#include <array>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace PS::HelpyQueryService {
namespace {
std::mutex Gate;
std::deque<nlohmann::json> Pending;
std::array<std::uint64_t,3> LatestCancel{};
std::uint64_t Revision=1;
constexpr std::size_t MaxPending=24;
int CategoryOf(const nlohmann::json& request){return request.value("Category",-1);}
int PriorityOf(const nlohmann::json& request){return std::clamp(request.value("Priority",2),0,3);}
std::uint64_t CancelOf(const nlohmann::json& request){return request.value("CancelGeneration",std::uint64_t{});}
bool StaleLocked(const nlohmann::json& request) {
    const int category=CategoryOf(request);if(category<0||category>2)return true;
    return CancelOf(request)<LatestCancel[static_cast<std::size_t>(category)];
}
}

bool Submit(nlohmann::json request) {
    if(!request.is_object()||request.value("Operation",std::string{})!="CatalogWindow")return false;
    const int category=CategoryOf(request);if(category<0||category>2)return false;
    if(request.value("RequestId",std::uint64_t{})==0)return false;
    const auto cancel=CancelOf(request);const auto query=request.value("Query",std::string{});
    if(query.size()>256)return false;
    const int limit=request.value("Limit",24);if(limit<1||limit>128)return false;
    std::lock_guard lock(Gate);
    auto& newest=LatestCancel[static_cast<std::size_t>(category)];
    if(cancel<newest)return false;
    if(cancel>newest)newest=cancel;
    std::erase_if(Pending,[&](const auto& queued){return CategoryOf(queued)==category&&CancelOf(queued)<cancel;});
    if(Pending.size()>=MaxPending) {
        auto worst=std::max_element(Pending.begin(),Pending.end(),[](const auto& a,const auto& b){return PriorityOf(a)<PriorityOf(b);});
        if(worst!=Pending.end()&&PriorityOf(*worst)>PriorityOf(request))Pending.erase(worst);else return false;
    }
    Pending.push_back(std::move(request));return true;
}

void Cancel(int category,std::uint64_t cancelGeneration) {
    if(category<0||category>2)return;
    std::lock_guard lock(Gate);
    auto& newest=LatestCancel[static_cast<std::size_t>(category)];newest=std::max(newest,cancelGeneration);
    std::erase_if(Pending,[&](const auto& request){return CategoryOf(request)==category&&CancelOf(request)<newest;});
}

std::optional<nlohmann::json> Take() {
    std::lock_guard lock(Gate);
    std::erase_if(Pending,[](const auto& request){return StaleLocked(request);});
    if(Pending.empty())return {};
    auto best=std::min_element(Pending.begin(),Pending.end(),[](const auto& a,const auto& b){return PriorityOf(a)<PriorityOf(b);});
    auto value=std::move(*best);Pending.erase(best);return value;
}

bool IsStale(const nlohmann::json& request){std::lock_guard lock(Gate);return StaleLocked(request);}
bool HasPending(){std::lock_guard lock(Gate);return !Pending.empty();}
std::size_t PendingCount(){std::lock_guard lock(Gate);return Pending.size();}

void Publish(nlohmann::json response) {
    const auto revision=++Revision;
    if(SpawnToolRequests::PushUpdate)SpawnToolRequests::PushUpdate("query",response,revision);
}
void Clear(){std::lock_guard lock(Gate);Pending.clear();LatestCancel={};++Revision;}
}
