#pragma once
// Game-side source for Core/GameplayEventFeed: observes a fixed set of native player callbacks plus mod
// Blueprint functions that Helpy asks to watch, keeps only the local player's calls, and pushes normalized
// events. Confirmed kills come from NativeKillCredit; a host relays a remote player's kill to that client.
//
// Cost on the hot ProcessEvent path is one FName comparison per watched name. Parameter capture and path
// strings are only built after a name matches.
#include "Core/GameplayEventFeed.h"
#include "Generator/EventParameters.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/NameTypes.hpp"
#include "Unreal/UObject.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <functional>
#include <string>
#include <vector>

namespace DragonWilds {
class GameplayEventObserver {
    std::vector<RC::Unreal::FName> m_names;
    std::set<std::string, std::less<>> m_watched;
    uint64_t m_watchRevision = ~uint64_t{};

    void Refresh() {
        using namespace RC::Unreal;
        const auto revision = PS::GameplayEventFeed::Current.WatchRevision();
        if (revision == m_watchRevision && !m_names.empty()) return;
        m_watchRevision = revision;
        m_watched = PS::GameplayEventFeed::Current.Watched();
        m_names.clear();
        const auto add = [&](std::string_view path) {
            const auto colon = path.rfind(':');
            if (colon == path.npos) return;
            const auto name = RC::to_generic_string(std::string(path.substr(colon + 1)));
            const FName value(name.c_str(), FNAME_Add);
            if (std::find(m_names.begin(), m_names.end(), value) == m_names.end()) m_names.push_back(value);
        };
        for (const auto path : PS::GameplayEventFeed::NativeFunctions) add(path);
        for (const auto& path : m_watched) add(path);
    }
    enum class Owner { None, Local, Remote };
    // Finds the player that owns `object` (component -> actor -> pawn/controller/player state).
    static Owner Ownership(RC::Unreal::UObject* object) {
        using namespace RC::Unreal;
        auto* controllerType = ActorHelper::ResolveClass(TEXT("/Script/Engine.PlayerController"));
        auto* pawnType = ActorHelper::ResolveClass(TEXT("/Script/Engine.Pawn"));
        auto* stateType = ActorHelper::ResolveClass(TEXT("/Script/Engine.PlayerState"));
        UObject* controller = nullptr;bool player = false;
        for (int depth = 0; object && depth < 6 && !controller; ++depth) {
            if (controllerType && object->IsA(controllerType)) { controller = object; player = true; break; }
            if (pawnType && object->IsA(pawnType)) {
                auto* playerCharacter = ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
                if (playerCharacter && object->IsA(playerCharacter)) player = true;
                controller = ActorHelper::GetObjectRef(object, TEXT("Controller"));
                break;
            }
            if (stateType && object->IsA(stateType)) { player = true; controller = ActorHelper::GetObjectRef(object, TEXT("Owner")); break; }
            object = object->GetOuterPrivate();
        }
        if (!controller) return player ? Owner::Remote : Owner::None; // A remote player's pawn has no controller on a client.
        if (!controllerType || !controller->IsA(controllerType)) return Owner::None; // AI pawn.
        ActorHelper::FunctionCall local(controller, TEXT("/Script/Engine.Controller:IsLocalController"));
        local.Invoke();
        return local.Result<bool>() ? Owner::Local : Owner::Remote;
    }
public:
    // Publishes a kill to a remote client (host side). Returns false when the client cannot be reached.
    std::function<bool(RC::Unreal::UObject* controller, const std::string& payload)> PublishRemote;

    void Observe(RC::Unreal::Hook::TCallbackIterationData<void>& dispatch, RC::Unreal::UObject* source,
        RC::Unreal::UFunction* function, void* parameters) noexcept {
        try {
            if (!source || !function || dispatch.OriginalFunctionCallPrevented()) return;
            Refresh();
            const auto name = function->GetFName();
            if (std::find(m_names.begin(), m_names.end(), name) == m_names.end()) return;
            const auto path = RC::to_string(function->GetPathName());
            const bool native = std::find(PS::GameplayEventFeed::NativeFunctions.begin(), PS::GameplayEventFeed::NativeFunctions.end(), path) != PS::GameplayEventFeed::NativeFunctions.end();
            if (!native && !m_watched.contains(path)) return;
            const auto owner = Ownership(source);
            // Native signals must belong to the local player. Mod events may come from world actors (a boss,
            // an altar); they count on the machine where they run, but never for another player's pawn.
            if (owner == Owner::Remote || (native && owner != Owner::Local)) return;
            auto event = PS::GameplayEventFeed::Normalize(path, PS::InspectionTools::CaptureEventParameters(function, parameters), m_watched);
            if (event) PS::GameplayEventFeed::Current.Push(std::move(*event));
        } catch (...) {}
    }
    // Called by NativeKillCredit after the game confirmed and dispatched a kill (authority only).
    void Kill(RC::Unreal::UObject* controller, RC::Unreal::UObject* victim) noexcept {
        try {
            using namespace RC::Unreal;
            if (!controller || !victim || !victim->GetClassPrivate()) return;
            auto event = PS::GameplayEventFeed::Kill(RC::to_string(victim->GetClassPrivate()->GetPathName()));
            ActorHelper::FunctionCall local(controller, TEXT("/Script/Engine.Controller:IsLocalController"));
            local.Invoke();
            if (local.Result<bool>()) { PS::GameplayEventFeed::Current.Push(std::move(event)); return; }
            if (PublishRemote) (void)PublishRemote(controller, nlohmann::json{{"Kind", "kill"}, {"Subject", event.Subject}}.dump());
        } catch (...) {}
    }
    // Client side: a kill the host credited to this player.
    static bool ReceiveRemote(const std::string& payload) noexcept {
        try {
            if (payload.size() > 1024) return false;
            const auto packet = nlohmann::json::parse(payload);
            if (!packet.is_object() || packet.size() != 2 || packet.value("Kind", std::string{}) != "kill" || !packet.at("Subject").is_string()) return false;
            PS::GameplayEventFeed::Current.Push(PS::GameplayEventFeed::Kill(packet["Subject"].get<std::string>()));
            return true;
        } catch (...) { return false; }
    }
};
}
