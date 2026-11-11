#include "Runtime/MultiplayerRuntime.h"
#include "Runtime/MultiplayerPolicy.h"
#include "Core/ModDataRelay.h"
#include "Unreal/AActor.hpp"
#include "Runtime/NetworkContext.h"
#include "Runtime/PluginHost.h"
#include "SDK/WeakObjectHandle.h"
#include "Utility/Logging.h"

#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

namespace PS::MultiplayerRuntime {
namespace {
using namespace RC::Unreal;
using DragonWilds::ActorHelper::FunctionCall;
using Json = nlohmann::json;
using Rule = MultiplayerPolicy::Rule;

struct Route { std::string Owner, Class; };
struct ActiveRule { std::string Key; Rule Values; };
struct AppliedValue { float Original, Written; };
struct AppliedActor {
    WeakObjectHandle Object;
    std::string Key;
    std::optional<AppliedValue> Range, Frequency;
};
struct Progress { std::size_t Applied = 0, Skipped = 0; std::string Detail; };

std::mutex Gate;
std::map<std::string, Route> Routes;
std::set<std::pair<std::string, std::string>> DeclaredTransport;
std::map<std::string, Progress> Status;
uint64_t WorldRevision = 0;
bool Host = false, Applying = false;
std::string RuntimeError;
std::atomic_bool Refresh = false;

// Engine objects and apply work are confined to the bridge's game-thread callbacks.
WeakObjectHandle World;
SnapshotPublisher Publish;
std::unordered_map<std::string, ActiveRule> Active;
std::unordered_map<int32_t, AppliedActor> Applied;
std::vector<WeakObjectHandle> Pending;

Json Policy(const Json& request) {
    return Json::parse(PluginHost::Call("RuneSchema.Core", MultiplayerPolicy::Service, request.dump()));
}

bool Usable(UObject* actor) {
    const auto* world = World.Get();
    return world && actor && actor->GetClassPrivate() && actor->GetWorld() == world
        && actor->IsA<AActor>() && !actor->HasAnyFlags(static_cast<EObjectFlags>(
            RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad | RF_NeedPostLoad
            | RF_BeginDestroyed | RF_FinishDestroyed));
}

bool Flag(UObject* actor, const RC::CharType* name) {
    auto* field = CastField<FBoolProperty>(actor->GetPropertyByNameInChain(name));
    if (!field || field->GetArrayDim() != 1
        || !DragonWilds::ReflectedContract::Fits(field, actor->GetClassPrivate()->GetPropertiesSize()))
        throw std::runtime_error("Actor replication flags are unavailable.");
    return field->GetPropertyValueInContainer(actor);
}

struct Setting {
    const RC::CharType* Getter;
    const RC::CharType* Setter;
};
constexpr Setting Range{TEXT("/Script/Engine.Actor:GetNetCullDistanceSquared"), TEXT("/Script/Engine.Actor:SetNetCullDistanceSquared")};
constexpr Setting Frequency{TEXT("/Script/Engine.Actor:GetNetUpdateFrequency"), TEXT("/Script/Engine.Actor:SetNetUpdateFrequency")};

UFunction* Bind(const RC::CharType* path, bool setter) {
    auto* function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, path);
    using namespace DragonWilds::ReflectedContract;
    if (Count(function) != 1) throw std::runtime_error("Actor replication accessor does not match the expected contract.");
    auto* value = CastField<FFloatProperty>(function->GetReturnProperty());
    if (setter) {
        for (auto* field : TFieldRange<FProperty>(function, EFieldIterationFlags::IncludeDeprecated))
            if (Input(field, function)) value = CastField<FFloatProperty>(field);
    }
    if (!value || value->GetSize() != sizeof(float) || (setter ? !Input(value, function) : !Output(value, function)))
        throw std::runtime_error("Actor replication accessor is not a single float.");
    return function;
}

float Get(UObject* actor, const Setting& setting) {
    FunctionCall call(actor, Bind(setting.Getter, false));
    call.Invoke();
    return call.Result<float>();
}
void Set(UObject* actor, const Setting& setting, float value) {
    FunctionCall call(actor, Bind(setting.Setter, true));
    call.FirstNumericArg(value).Invoke();
}

void RestoreValue(UObject* actor, const Setting& setting, const std::optional<AppliedValue>& value) {
    if (value && Get(actor, setting) == value->Written) Set(actor, setting, value->Original);
}
void Restore(const AppliedActor& actor) {
    auto* object = actor.Object.Get();
    if (!Usable(object)) return;
    try { RestoreValue(object, Range, actor.Range); } catch (...) {}
    try { RestoreValue(object, Frequency, actor.Frequency); } catch (...) {}
}

void Record(const std::string& key, bool applied, std::string detail = {}) {
    std::lock_guard lock(Gate);
    auto& progress = Status[key];
    if (applied) ++progress.Applied;
    else { ++progress.Skipped; progress.Detail = std::move(detail); }
}

void Apply(UObject* actor, const WeakObjectHandle& handle) {
    if (!Usable(actor)) return;
    const auto it = Active.find(RC::to_string(actor->GetClassPrivate()->GetPathName()));
    if (it == Active.end()) return;
    const auto& [key, rule] = it->second;
    if (const auto old = Applied.find(handle.ObjectIndex); old != Applied.end()) {
        if (old->second.Object.Get() == actor) return;
        Applied.erase(old);
    }
    AppliedActor applied{handle, key, {}, {}};
    try {
        FunctionCall authority(actor, TEXT("/Script/Engine.Actor:HasAuthority"));
        authority.Invoke();
        if (!authority.Result<bool>() || !Flag(actor, TEXT("bReplicates")))
            throw std::runtime_error("Actor is not authoritative and replicating.");
        if (rule.DistanceSquared && (Flag(actor, TEXT("bAlwaysRelevant")) || Flag(actor, TEXT("bOnlyRelevantToOwner"))
            || Flag(actor, TEXT("bNetUseOwnerRelevancy"))))
            throw std::runtime_error("Actor uses always-relevant or owner-based relevance.");
        if (rule.DistanceSquared) {
            (void)Bind(Range.Setter, true);
            applied.Range = AppliedValue{Get(actor, Range), *rule.DistanceSquared};
        }
        if (rule.Frequency) {
            (void)Bind(Frequency.Setter, true);
            applied.Frequency = AppliedValue{Get(actor, Frequency), *rule.Frequency};
        }
        if (applied.Range) {
            Set(actor, Range, applied.Range->Written);
            const auto actual = Get(actor, Range);
            if (actual != applied.Range->Written) {
                applied.Range->Written = actual;
                throw std::runtime_error("Range readback differs.");
            }
        }
        if (applied.Frequency) {
            Set(actor, Frequency, applied.Frequency->Written);
            const auto actual = Get(actor, Frequency);
            if (actual != applied.Frequency->Written) {
                applied.Frequency->Written = actual;
                throw std::runtime_error("Update rate readback differs.");
            }
        }
        Applied.emplace(handle.ObjectIndex, applied);
        Record(key, true);
    } catch (const std::exception& error) {
        Restore(applied);
        Record(key, false, error.what());
    }
}

void Reload() {
    for (const auto& [_, actor] : Applied) Restore(actor);
    Applied.clear();
    Active.clear();
    Pending.clear();
    std::map<std::string, Route> routes;
    {
        std::lock_guard lock(Gate);
        Status.clear();
        RuntimeError.clear();
        Applying = true;
        routes = Routes;
    }
    if (routes.empty()) return;
    const auto policy = Policy({{"Operation", "Read"}});
    if (!policy.value("Ok", false)) throw std::runtime_error(policy.at("Status").get<std::string>());
    const auto& rules = policy.at("Rules");
    for (const auto& [key, route] : routes) {
        if (!rules.contains(key)) continue;
        Active.emplace(route.Class, ActiveRule{key, MultiplayerPolicy::Parse(rules.at(key))});
        const auto classPath = PS::ToWideSafe(route.Class);
        auto* type = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, classPath.c_str());
        if (!type) continue; // A later BeginPlay covers classes streamed in after this refresh.
        TArray<UObject*> actors;
        UECustom::UObjectGlobals::GetObjectsOfClass(type, actors, false);
        for (auto* actor : actors)
            if (Usable(actor) && actor->GetClassPrivate() == type) Pending.emplace_back(actor);
    }
}
}

void Declare(const Json& entries) {
    std::map<std::string, Route> next;
    std::map<std::string, unsigned> declarations;
    std::set<std::pair<std::string, std::string>> transports;
    for (const auto& entry : entries) {
        const auto kind = entry.value("kind", std::string{});
        if (kind == "DataRelay") {
            const auto channel = ModDataRelay::Declaration(entry.at("owner").get<std::string>(), entry.at("relay"));
            transports.emplace(kind, channel.Key);
        } else if (kind == "CharacterAppearance") transports.emplace(kind, entry.at("key").get<std::string>());
        if (kind != "MultiplayerActor") continue;
        auto path = entry.at("actorClass").get<std::string>();
        ++declarations[path];
        next.emplace(entry.at("key").get<std::string>(), Route{entry.at("owner").get<std::string>(), std::move(path)});
    }
    std::erase_if(next, [&](const auto& entry) { return declarations.at(entry.second.Class) != 1; });
    {
        std::lock_guard lock(Gate);
        Routes = std::move(next);
        DeclaredTransport = std::move(transports);
    }
    Refresh = true;
}

void Attach(UObject* gameState, SnapshotPublisher publisher) {
    const auto network = Network::Detect(gameState);
    const bool host = network.Mode == Network::Role::DedicatedServer || network.Mode == Network::Role::ListenServer;
    if (!host || !network.World) return;
    if (World.Get() == network.World) return;
    Reset();
    World.Assign(network.World);
    Publish = std::move(publisher);
    {
        std::lock_guard lock(Gate);
        Host = World.Get() != nullptr;
    }
    Refresh = true;
}

void ObserveActor(UObject* actor, bool entering) {
    if (Active.empty() || !actor) return;
    if (!entering) {
        const auto found = Applied.find(actor->GetInternalIndex());
        if (found != Applied.end() && found->second.Object.Get() == actor) {
            std::lock_guard lock(Gate);
            auto& count = Status[found->second.Key].Applied;
            if (count) --count;
            Applied.erase(found);
        }
        return;
    }
    if (Usable(actor) && Active.contains(RC::to_string(actor->GetClassPrivate()->GetPathName())))
        Pending.emplace_back(actor);
}

void Tick() {
    if (!World.Get()) {
        if (Host) Reset();
        return;
    }
    if (Refresh.exchange(false)) {
        try { Reload(); }
        catch (const std::exception& error) {
            Active.clear();
            Pending.clear();
            std::lock_guard lock(Gate);
            RuntimeError = error.what();
        }
    }
    if (Pending.empty() && !Applying) return;
    // Amortize reflected calls when a host changes settings on a populated world.
    for (int budget = 32; budget > 0 && !Pending.empty(); --budget) {
        auto actor = Pending.back();
        Pending.pop_back();
        Apply(actor.Get(), actor);
    }
    std::lock_guard lock(Gate);
    Applying = !Pending.empty();
}

void Reset() {
    std::lock_guard lock(Gate);
    World.Reset();
    Publish = {};
    Applied.clear();
    Active.clear();
    Pending.clear();
    Refresh = false;
    ++WorldRevision;
    Host = Applying = false;
    Status.clear();
    RuntimeError.clear();
}

bool PublishSnapshot(const std::string& owner, const std::string& channel, const std::string& payload) {
    if (!World.Get() || !Publish) return false;
    const auto key = owner + ":" + channel;
    {
        std::lock_guard lock(Gate);
        if (!DeclaredTransport.contains({"DataRelay", key})) return false;
    }
    Publish(key, payload);
    return true;
}

Json Probe(const Json& request) {
    const auto kind = request.at("Kind").get<std::string>();
    const auto key = request.at("Key").get<std::string>();
    std::lock_guard lock(Gate);
    return {{"Protocol", 1}, {"Declared", kind == "MultiplayerActor" ? Routes.contains(key)
        : DeclaredTransport.contains({kind, key})}};
}

Json Control(const Json& request) {
    try {
        std::lock_guard lock(Gate);
        const auto operation = request.value("Operation", std::string("Read"));
        if (operation == "Read") {
            auto policy = Policy({{"Operation", "Read"}});
            Json rows = Json::array();
            for (const auto& [key, route] : Routes) {
                const auto found = Status.find(key);
                const auto progress = found == Status.end() ? Progress{} : found->second;
                rows.push_back({{"Key", key}, {"Owner", route.Owner}, {"Class", route.Class},
                    {"Rule", policy.at("Rules").value(key, Json::object())}, {"Applied", progress.Applied},
                    {"Skipped", progress.Skipped}, {"Detail", progress.Detail}});
            }
            return {{"Available", true}, {"CanManage", Host && policy.value("Ok", false)},
                {"World", WorldRevision}, {"Revision", policy.at("Revision")}, {"Rows", std::move(rows)},
                {"Pending", Applying || Refresh.load()}, {"Status", RuntimeError.empty() ? policy.at("Status").get<std::string>() : RuntimeError}};
        }
        if (operation != "Set" && operation != "Reset") throw std::runtime_error("Unknown multiplayer operation.");
        if (!Host || request.at("World").get<uint64_t>() != WorldRevision)
            throw std::runtime_error("Use Helpy on the current multiplayer host.");
        if (!Routes.contains(request.at("Key").get<std::string>()))
            throw std::runtime_error("The class is no longer declared by an enabled mod.");
        auto response = Policy(request);
        if (response.value("Ok", false)) Refresh = true;
        return response;
    } catch (const std::exception& error) {
        return {{"Available", false}, {"Ok", false}, {"Status", error.what()}};
    }
}
}
