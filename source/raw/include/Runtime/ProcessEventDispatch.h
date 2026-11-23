#pragma once
// Shared ProcessEvent callbacks for handlers that only react to fixed function names.
//
// Every UE4SS ProcessEvent callback runs on every game function call, so each
// hook that only needs a few function names still costs something per call.
// Those hooks register a route here instead. One UE4SS callback per phase
// (pre and post) looks the function name up and calls only the routed
// handlers; a name nobody routes costs one atomic load and a bit test.
//
// Behaviour matches a separate callback per handler:
// - Handlers receive the same iteration data, so PreventOriginalFunctionCall
//   and OriginalFunctionCallPrevented work as before. They must not call
//   RemoveSelf (that would remove the shared callback); use Remove instead.
// - Handlers for one name run in the order they were added.
// - A handler keeps its own thread and state checks; the dispatcher does not
//   filter by thread.
// - An exception from one handler is reported once and does not stop the others.
//
// Only hooks whose names are not also handled by another UE4SS callback
// belong here, unless both are read-only post observers: moving a hook into
// the dispatcher changes its order relative to callbacks that stay outside.
#include "Core/ProcessEventRoutes.h"
#include "Utility/Logging.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/NameTypes.hpp"
#include "Unreal/UFunction.hpp"
#include "Unreal/UObject.hpp"
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace PS::ProcessEventDispatch {
using Handler = std::function<void(RC::Unreal::Hook::TCallbackIterationData<void>&, RC::Unreal::UObject*,
    RC::Unreal::UFunction*, void*)>;
using Routes = ProcessEventRoutes<RC::Unreal::FName, Handler>;
enum class Phase : std::uint8_t { Pre, Post };

// Empty (false) when registration failed or after Remove.
struct Route {
    Phase When = Phase::Pre;
    Routes::Id Id = 0;
    explicit operator bool() const { return Id != 0; }
};

namespace Detail {
inline Routes& Table(Phase phase) {
    static Routes pre, post;
    return phase == Phase::Pre ? pre : post;
}
inline std::mutex InstallGate;
inline RC::Unreal::Hook::GlobalCallbackId Installed[2]{RC::Unreal::Hook::ERROR_ID, RC::Unreal::Hook::ERROR_ID};
inline std::mutex ReportGate;
inline std::set<std::string> Reported;

inline void Report(const std::string& label, const char* what) noexcept {
    try {
        {
            std::lock_guard lock(ReportGate);
            if (Reported.size() >= 256 || !Reported.insert(label).second) return;
        }
        PS::Log<RC::LogLevel::Warning>(STR("ProcessEvent handler '{}' failed: {}. Later failures from this handler are not repeated.\n"),
            PS::ToWideSafe(label), PS::ToWideSafe(what ? what : "unknown error"));
    } catch (...) {}
}

template<Phase When>
void Dispatch(RC::Unreal::Hook::TCallbackIterationData<void>& iteration, RC::Unreal::UObject* source,
    RC::Unreal::UFunction* function, void* parameters) {
    if (!function) return;
    Table(When).Dispatch(function->GetFName(), [&](const Handler& handler, const std::string& label) {
        try { handler(iteration, source, function, parameters); }
        catch (const std::exception& error) { Report(label, error.what()); }
        catch (...) { Report(label, nullptr); }
    });
}

inline bool Install(Phase phase) {
    using namespace RC::Unreal;
    std::lock_guard lock(InstallGate);
    auto& id = Installed[phase == Phase::Pre ? 0 : 1];
    if (id != Hook::ERROR_ID) return true;
    Hook::FCallbackOptions options{};
    options.OwnerModName = STR("RuneSchema");
    if (phase == Phase::Pre) {
        options.HookName = STR("ProcessEventDispatchPre");
        id = Hook::RegisterProcessEventPreCallback(
            [](Hook::TCallbackIterationData<void>& iteration, UObject* source, UFunction* function, void* parameters) {
                Dispatch<Phase::Pre>(iteration, source, function, parameters);
            }, options);
    } else {
        options.HookName = STR("ProcessEventDispatchPost");
        id = Hook::RegisterProcessEventPostCallback(
            [](Hook::TCallbackIterationData<void>& iteration, UObject* source, UFunction* function, void* parameters) {
                Dispatch<Phase::Post>(iteration, source, function, parameters);
            }, options);
    }
    return id != Hook::ERROR_ID;
}
}

// `names` are bare function names as UFunction::GetFName reports them (no class
// path). `label` names the handler in failure reports. Returns an empty Route when
// the shared callback could not be installed or the route was refused.
inline Route Add(Phase phase, std::vector<RC::Unreal::FName> names, std::string label, Handler handler) {
    if (!Detail::Install(phase)) return {};
    return Route{phase, Detail::Table(phase).Add(std::move(names), std::move(label), std::move(handler))};
}
inline Route Add(Phase phase, std::initializer_list<const RC::CharType*> names, std::string label, Handler handler) {
    std::vector<RC::Unreal::FName> keys;
    keys.reserve(names.size());
    for (const auto* name : names) keys.emplace_back(name, RC::Unreal::FNAME_Add);
    return Add(phase, std::move(keys), std::move(label), std::move(handler));
}

// Safe on an empty Route; leaves it empty.
inline void Remove(Route& route) {
    if (route) Detail::Table(route.When).Remove(route.Id);
    route = {};
}

// Unregisters the shared callbacks at module shutdown. Routes left behind stop
// being called; owners still remove their own routes.
inline void Shutdown() {
    using namespace RC::Unreal;
    std::lock_guard lock(Detail::InstallGate);
    for (auto& id : Detail::Installed)
        if (id != Hook::ERROR_ID) { Hook::UnregisterCallback(id); id = Hook::ERROR_ID; }
}
}
