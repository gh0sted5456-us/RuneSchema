#include "Runtime/NetworkIdentityBootstrap.h"
#include "Core/NetworkIdentityWelcome.h"
#include "Core/NativeWelcomeLayout.h"
#include "Core/NetworkIdentityState.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Utility/Logging.h"
#include <Unreal/AGameModeBase.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FWorldContext.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/World.hpp>
#include <Helpers/Casting.hpp>

using namespace RC;
using namespace RC::Unreal;
namespace PS::NetworkIdentityBootstrap {
namespace {
    Snapshot Read;
    Apply Align;
    Hook::GlobalCallbackId WorldHook = Hook::ERROR_ID, MapHook = Hook::ERROR_ID;
    using WelcomeFunction = void(*)(AGameModeBase*, void*, FString&);
    WelcomeFunction Original = nullptr;
    void** PatchedSlot = nullptr;
    std::size_t WelcomeOffset = 0;
    WeakObjectHandle Mode;
    constexpr std::string_view FailurePrefix = "runeschema-ids-error-v1:";
    void PublishAuthority(const nlohmann::json& document) {
        const auto& lanes=document.at("lanes");
        const auto count=lanes.contains("BUILDING")?lanes.at("BUILDING").at("prefixCount").get<std::size_t>()+lanes.at("BUILDING").at("entries").size():0;
        NetworkIdentityState::SetAuthorityOrderProof(NetworkIdentityWelcome::OrderProof(document),count);
    }

    void Welcome(AGameModeBase* mode, void* connection, FString& redirect) {
        Original(mode, connection, redirect);
        if (Mode.Get() != mode || !Read || redirect.GetCharArray().Num() > 1) return;
        try {
            const auto document = Read(); const auto token = NetworkIdentityWelcome::Encode(document);
            PublishAuthority(document);
            redirect = FString(ToWideSafe(token.c_str()));
            Log<LogLevel::Normal>(STR("[NETWORK-ID-SYNC][OFFER] Native pre-join welcome: {} lanes, {} characters.\n"), document.at("lanes").size(), token.size());
        } catch (const std::exception& error) {
            redirect = FString(ToWideSafe((std::string(FailurePrefix) + "Registry synchronization is unavailable; see the server log.").c_str()));
            Log<LogLevel::Warning>(STR("[RS-NET-002] [NETWORK-ID-SYNC][OFFER-REJECTED] {}\n"), ToWideSafe(error.what()));
        }
    }
    bool Unpatch() noexcept {
        if (!PatchedSlot) return true;
        DWORD protect = 0;
        bool restored = false;
        if (VirtualProtect(PatchedSlot, sizeof(void*), PAGE_READWRITE, &protect)) {
            const auto previous = InterlockedCompareExchangePointer(PatchedSlot, reinterpret_cast<void*>(Original), reinterpret_cast<void*>(&Welcome));
            restored = previous == reinterpret_cast<void*>(&Welcome) || previous == reinterpret_cast<void*>(Original);
            DWORD ignored; VirtualProtect(PatchedSlot, sizeof(void*), protect, &ignored);
        }
        if (restored) { PatchedSlot = nullptr; Mode.Reset(); }
        return restored;
    }
    std::size_t VerifiedWelcomeOffset() {
        const auto* image = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
        const auto& site = NativeWelcomeLayout::Find(pe->FileHeader.TimeDateStamp, pe->OptionalHeader.SizeOfImage);
        return NativeWelcomeLayout::Verify({image + site.Address, site.Code.size()}, site.Code);
    }
    void Install(AGameModeBase* mode) {
        if (!mode || mode->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return;
        const auto offset = WelcomeOffset;
        auto** table = *reinterpret_cast<void***>(mode); auto** slot = table + offset / sizeof(void*);
        if (PatchedSlot == slot) { Mode.Assign(mode); return; }
        if (!Unpatch()) throw std::runtime_error("Another plugin retains the previous welcome hook; synchronization was not rebound");
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(slot, &page, sizeof(page)) || page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            throw std::runtime_error("Native welcome virtual slot is unreadable");
        void* target = *slot;
        if (!VirtualQuery(target, &page, sizeof(page)) || page.State != MEM_COMMIT
            || !(page.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            throw std::runtime_error("Native welcome target is not executable");
        DWORD protect = 0;
        // A later plugin can retain our callback in its own chain after shutdown.
        HMODULE resident = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&Welcome), &resident)) throw std::runtime_error("Cannot retain the native welcome callback module");
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protect)) throw std::runtime_error("Native welcome virtual slot is protected");
        Original = reinterpret_cast<WelcomeFunction>(target);
        const auto previous = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(&Welcome), target);
        DWORD ignored; VirtualProtect(slot, sizeof(void*), protect, &ignored);
        if (previous != target) throw std::runtime_error("Another plugin changed the native welcome slot");
        PatchedSlot = slot; Mode.Assign(mode);
        Log<LogLevel::Normal>(STR("[NETWORK-ID-SYNC][HOOK] Native welcome slot 0x{:X} installed after executable call-site verification.\n"), offset);
    }
    bool NetworkIdle(FWorldContext& context) {
        auto* world = context.GetThisCurrentWorld().Get(); if (!world) return true;
        auto* field = CastField<FObjectPropertyBase>(DragonWilds::PropertyHelper::GetPropertyByName(world->GetClassPrivate(), TEXT("NetDriver")));
        NetworkIdentitySync::Require(field != nullptr, "Cannot verify the current world's network boundary");
        return field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(world)) == nullptr;
    }
}
void Start(Snapshot snapshot, Apply apply) {
    Stop();
    wchar_t enabled[4]{};
    // Disabled by default until full native multiplayer checks pass.
    if (GetEnvironmentVariableW(L"RUNESCHEMA_NETWORK_ID_SYNC", enabled, 4) != 1 || enabled[0] != L'1') return;
    WelcomeOffset = VerifiedWelcomeOffset();
    Read = std::move(snapshot); Align = std::move(apply);
    PublishAuthority(Read());
    Hook::FCallbackOptions options{}; options.OwnerModName = TEXT("RuneSchema"); options.HookName = TEXT("RegistryNativeWelcome");
    WorldHook = Hook::RegisterInitGameStatePostCallback([](auto&, AGameModeBase* mode) {
        try { Install(mode); } catch (const std::exception& error) { Log<LogLevel::Warning>(STR("[NETWORK-ID-SYNC][HOOK-UNAVAILABLE] {}\n"), ToWideSafe(error.what())); }
    }, options);
    options.HookName = TEXT("RegistryPreJoinIds");
    MapHook = Hook::RegisterLoadMapPreCallback([](auto& info, UEngine*, FWorldContext& context, FURL url, UPendingNetGame* pending, FString& error) {
        if (!pending || !Read || !Align) return;
        NetworkIdentityState::SetAuthorityOrderProof({});
        const auto& chars = url.RedirectURL.GetCharArray();
        if (chars.Num() <= 1) return;
        try {
            NetworkIdentitySync::Require(chars.GetData() && chars.Num() <= static_cast<int32>(NetworkIdentitySync::MaxWelcome + 1)
                && chars.GetData()[chars.Num() - 1] == 0, "Invalid native welcome string");
            const auto token = RC::to_string(RC::StringType(chars.GetData(), chars.Num() - 1));
            if (token.starts_with(FailurePrefix)) throw NetworkIdentitySync::Incompatible("Authority mapping is unavailable; native handling retained");
            if (!token.starts_with(NetworkIdentitySync::WelcomePrefix)) return;
            NetworkIdentitySync::Compatible(NetworkIdle(context), "Existing network world retained; live registry IDs were not changed");
            nlohmann::json local;
            try { local = Read(); } catch(const std::exception& problem) { throw NetworkIdentitySync::Incompatible(problem.what()); }
            const auto authority = NetworkIdentityWelcome::Decode(token, local);
            if(authority.value("telemetryOmitted",false))
                Log<LogLevel::Warning>(STR("[RS-NET-002] [NETWORK-ID-SYNC][TELEMETRY-REDUCED] Optional source metadata was reduced to fit the native welcome; complete shared-ID verification remains required.\n"));
            if(authority.contains("sourceVersions")&&local.contains("sourceVersions"))
                for(const auto& [owner,version]:authority.at("sourceVersions").items())
                    if(local.at("sourceVersions").contains(owner)&&local.at("sourceVersions").at(owner)!=version)
                        Log<LogLevel::Warning>(STR("[RS-NET-002] [NETWORK-ID-SYNC][CONTENT-WARNING] Mod '{}' version differs from the authority; shared IDs are negotiated independently.\n"),ToWideSafe(owner));
            if(!authority.at("registryOwners").empty()&&local.at("registryOwners") != authority.at("registryOwners"))
                Log<LogLevel::Warning>(STR("[RS-NET-002] [NETWORK-ID-SYNC][CONTENT-WARNING] Registry definitions or versions differ; shared IDs are negotiated independently.\n"));
            auto order = NetworkIdentitySync::Plan(local, authority);
            bool changed = false;
            for (const auto& [key, rows] : order) for (std::size_t i = 0; i < rows.size(); ++i) changed = changed || rows[i] != i;
            NetworkIdentitySync::Permutations undo;
            for (const auto& [key, rows] : order) { auto& inverse = undo[key]; inverse.resize(rows.size()); for (std::size_t i = 0; i < rows.size(); ++i) inverse[rows[i]] = i; }
            try { if (changed) Align(order); }
            catch(const std::exception& problem) { throw NetworkIdentitySync::Incompatible(problem.what()); }
            try {
                const auto checked = NetworkIdentitySync::Plan(Read(), authority);
                for(const auto& [key,rows]:checked) for(std::size_t i=0;i<rows.size();++i)
                    NetworkIdentitySync::Require(rows[i]==i,"Shared registry IDs did not match the authority");
            }
            catch (...) { if (changed) Align(undo); throw; }
            PublishAuthority(authority);
            std::size_t clientCount=0,authorityCount=0;
            for(const auto& [key,lane]:local.at("lanes").items())clientCount+=lane.at("entries").size();
            for(const auto& [key,lane]:authority.at("lanes").items())authorityCount+=lane.at("entries").size();
            Log<LogLevel::Normal>(STR("[NETWORK-ID-SYNC][{}] {} native registry lanes verified before LoadMap/SendJoin; protected IDs retained; {} client-only entries retained locally.\n"), changed ? STR("SYNCHRONIZED") : STR("MATCH"), order.size(),clientCount-authorityCount);
        } catch (const NetworkIdentitySync::Incompatible& problem) {
            Log<LogLevel::Warning>(STR("[RS-NET-002] [NETWORK-ID-SYNC][CONTENT-WARNING] {}; alignment skipped, native entry remains available.\n"), ToWideSafe(problem.what()));
        } catch (const std::exception& problem) {
            error = FString(ToWideSafe((std::string("RuneSchema registry synchronization failed: ") + problem.what()).c_str()));
            info.TrySetReturnValue(false); info.PreventOriginalFunctionCall();
            Log<LogLevel::Warning>(STR("[NETWORK-ID-SYNC][JOIN-REJECTED] {}\n"), ToWideSafe(problem.what()));
        }
    }, options);
    if (WorldHook == Hook::ERROR_ID || MapHook == Hook::ERROR_ID) { Stop(); throw std::runtime_error("Native registry synchronization hooks are unavailable"); }
    TArray<UObject*> modes;
    UECustom::UObjectGlobals::GetObjectsOfClass(AGameModeBase::StaticClass(), modes, true);
    for (auto* object : modes) if (object && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed))) {
        auto* mode = static_cast<AGameModeBase*>(object);
        if (mode->GetWorld()) Install(mode);
    }
}
void Stop() noexcept {
    NetworkIdentityState::SetAuthorityOrderProof({});
    Unpatch();
    for (auto* hook : {&WorldHook, &MapHook}) if (*hook != Hook::ERROR_ID) { Hook::UnregisterCallback(*hook); *hook = Hook::ERROR_ID; }
    Read = {}; Align = {};
}
}
