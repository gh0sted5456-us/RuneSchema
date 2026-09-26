#include "Generator/HelpyStatKey.h"
#include "Generator/ItemCloneRequest.h"
#include "Runtime/F2FavoriteStore.h"
#include "Runtime/HelpySettings.h"
#include "Runtime/HostServices.h"
#include <chrono>
#include "Generator/InGameQuickMenu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <cwchar>
#include <stdexcept>
#include <initializer_list>
#include <string>
#include <nlohmann/json.hpp>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/UGameViewportClient.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/World.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Generator/ToolRequest.h"
#include "Generator/QuickMenuSpawnRequest.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/Memory.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/InlineHook.h"
#include "Utility/Logging.h"
using namespace RC;
using namespace RC::Unreal;
using namespace DragonWilds;
namespace PS {
namespace {
void SyncFavorites(QuickUI::Model& model) {
    if(model.favoritesLoaded&&!model.favoritesDirty)return;
    const auto file=HostServices::ReferencesDirectory()/"Helpy-favorites.json";
    if(!model.favoritesLoaded) {
        model.favoritesLoaded=true;
        try {
            bool migrated=false;
            model.SetFavorites(F2FavoriteStore::LoadWithLegacy(file,HostServices::SettingsDirectory()/"F2-favorites.json",migrated));
            if(migrated)model.favoriteStatus="Favorites imported to runtime/live/saved/references/Helpy-favorites.json.";
        }
        catch(const std::exception& e) {
            model.favoritesWritable=false;model.favoriteStatus=std::string("Favorites are session-only: ")+e.what();
            model.status=model.favoriteStatus;PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy: {}\n"),RC::to_generic_string(model.favoriteStatus));
        }
    }
    if(!model.favoritesDirty)return;
    model.favoritesDirty=false;
    if(!model.favoritesWritable)return;
    try {F2FavoriteStore::Save(file,model.favorites);model.favoriteStatus="Favorites saved locally.";}
    catch(const std::exception& e) {
        model.favoritesWritable=false;model.favoriteStatus=std::string("Favorites are session-only: ")+e.what();model.status=model.favoriteStatus;
        PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy: {}\n"),RC::to_generic_string(model.favoriteStatus));
    }
}
void PrioritizeVisibleSearch(const QuickUI::Model& model) {
    struct SearchState {std::string query;uint64_t generation=0;std::chrono::steady_clock::time_point changed{};bool sent=false,indexing=false;};
    static SearchState state;
    const auto now=std::chrono::steady_clock::now();const auto generation=SpawnToolRequests::CurrentGeneration();
    const auto query=model.ActiveSearch();
    if(state.query!=query||state.generation!=generation||(!state.indexing&&model.indexing)) {
        state.query=query;state.generation=generation;state.changed=now;state.sent=false;
        SpawnToolRequests::PrioritizeSearch(""); // discard an obsolete load hint, never a mutation request
    }
    state.indexing=model.indexing;
    if(!state.sent&&query.size()>=2&&now-state.changed>=std::chrono::milliseconds(300)) {
        SpawnToolRequests::PrioritizeSearch(query);state.sent=true;
    }
}
nlohmann::json Vec2(double x, double y) {
    return {{"X", x}, {"Y", y}};
}

nlohmann::json Color(double r, double g, double b, double a = 1.0) {
    return {{"R", r}, {"G", g}, {"B", b}, {"A", a}};
}

double ReadNumeric(UObject* object, const TCHAR* name) {
    if (!object) return 0.0;
    auto* raw = PropertyHelper::GetPropertyByName(object->GetClassPrivate(), name);
    auto* numeric = CastField<FNumericProperty>(raw);
    if (!numeric || numeric->GetArrayDim()!=1 || numeric->GetOffset_Internal()<0
        || numeric->GetElementSize()<=0 || numeric->GetOffset_Internal()+numeric->GetElementSize()>object->GetClassPrivate()->GetPropertiesSize())return 0.0;
    auto* address = numeric->ContainerPtrToValuePtr<void>(object);
    if (!address) return 0.0;
    if (numeric->IsFloatingPoint()) return numeric->GetFloatingPointPropertyValue(address);
    if (numeric->IsInteger()) return static_cast<double>(numeric->GetSignedIntPropertyValue(address));
    return 0.0;
}

void* ResolvePostRender() {
    auto vtable = DragonWilds::GetVTablePtrByClassPath(TEXT("/Script/Engine.GameViewportClient"));
    const auto slot = UGameViewportClient::VTableLayoutMap.find(TEXT("PostRender"));
    if (!vtable || slot == UGameViewportClient::VTableLayoutMap.end()
        || slot->second % sizeof(void*) || slot->second > 8192) {
        throw std::runtime_error("GameViewportClient PostRender vtable metadata is unavailable");
    }

    auto* target = GetVirtualFunctionFromVTable(vtable, slot->second / sizeof(void*));
    MEMORY_BASIC_INFORMATION memory{};
    if (!target || !VirtualQuery(target, &memory, sizeof(memory)) || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        || !(memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        throw std::runtime_error("GameViewportClient PostRender target is not executable");
    }
    return target;
}

UFunction* CanvasFunction(const TCHAR* path) {
    // These engine UFunctions live for the process lifetime. Looking them up
    // for every text and rectangle made reflection discovery part of the hot
    // render loop (hundreds of searches per frame on dense pages).
    static std::unordered_map<std::wstring,UFunction*> functions;
    const std::wstring key(path);
    const auto found=functions.find(key);
    if(found!=functions.end())return found->second;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    if(function)functions.emplace(key,function);
    return function;
}

UObject* MenuAsset(const char* path) {
    static std::unordered_map<std::string,PS::WeakObjectHandle> assets;
    auto& weak=assets[path];auto* value=weak.Get();
    if(value && !value->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return value;
    weak.Reset();value=ActorHelper::ResolveObject(RC::to_generic_string(path));
    if(value)weak.Assign(value);return value;
}
UObject* MenuWhiteTexture() {return MenuAsset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture");}
UObject* MenuFont() {return MenuAsset("/Engine/EngineFonts/Roboto.Roboto");}

void DrawTexture(UObject* canvas, UObject* texture, float x, float y, float w, float h,
                 float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) {
    UFunction* function = CanvasFunction(TEXT("/Script/Engine.Canvas:K2_DrawTexture"));
    if (!canvas || !function || !texture) throw std::runtime_error("Canvas K2_DrawTexture texture contract is unavailable");
    ActorHelper::FunctionCall call(canvas, function);
    call.Arg(STR("RenderTexture"), texture)
        .JsonArg(STR("ScreenPosition"), Vec2(x, y))
        .JsonArg(STR("ScreenSize"), Vec2(w, h))
        .JsonArg(STR("CoordinatePosition"), Vec2(0.0, 0.0))
        .JsonArg(STR("CoordinateSize"), Vec2(1.0, 1.0))
        .JsonArg(STR("RenderColor"), Color(r, g, b, a))
        .JsonArg(STR("BlendMode"), "BLEND_Translucent")
        .JsonArg(STR("Rotation"), 0.0)
        .JsonArg(STR("PivotPoint"), Vec2(0.0, 0.0))
        .Invoke();
}

void DrawRect(UObject* canvas, float x, float y, float w, float h,
              float r, float g, float b, float a) {
    UObject* texture = MenuWhiteTexture();
    if (!texture) throw std::runtime_error("Engine white texture is unavailable");
    DrawTexture(canvas, texture, x, y, w, h, r, g, b, a);
}

void DrawText(UObject* canvas, const std::string& text,
              float x, float y, float scale, float r, float g, float b, float a = 1.0f,
              bool centreX = false) {
    UFunction* function = CanvasFunction(TEXT("/Script/Engine.Canvas:K2_DrawText"));
    auto* font = MenuFont();
    // Never call K2_DrawText with a null font. Dragonwilds does not provide a
    // reliable null-font fallback in this path.
    if (!canvas || !function || !font) throw std::runtime_error("Canvas text renderer or engine font is unavailable");
    ActorHelper::FunctionCall call(canvas, function);
    call.Arg(STR("RenderFont"), font)
        .JsonArg(STR("RenderText"), text)
        .JsonArg(STR("ScreenPosition"), Vec2(x, y))
        .JsonArg(STR("Scale"), Vec2(scale, scale))
        .JsonArg(STR("RenderColor"), Color(r, g, b, a))
        .JsonArg(STR("Kerning"), 0.0)
        .JsonArg(STR("ShadowColor"), Color(0.0, 0.0, 0.0, 0.75))
        .JsonArg(STR("ShadowOffset"), Vec2(1.0, 1.0))
        .JsonArg(STR("bCentreX"), centreX)
        .JsonArg(STR("bCentreY"), false)
        .JsonArg(STR("bOutlined"), false)
        .JsonArg(STR("OutlineColor"), Color(0.0, 0.0, 0.0, 1.0))
        .Invoke();
}

bool ValidExtent(double w,double h) {
    return std::isfinite(w)&&std::isfinite(h)&&w>=320&&h>=240&&w<=32768&&h<=32768;
}
std::pair<float,float> ViewportExtent(UObject* canvas,UObject* controller) {
    // SizeX/SizeY are not reflected on every supported runtime. Query the
    // native player viewport through validated *out parameters*, not raw offsets.
    auto* fn=CanvasFunction(TEXT("/Script/Engine.PlayerController:GetViewportSize"));
    if(controller&&fn&&fn->GetParmsSize()>0&&fn->GetParmsSize()<=64) {
        auto* x=CastField<FNumericProperty>(fn->FindProperty(FName(TEXT("SizeX"),FNAME_Find)));
        auto* y=CastField<FNumericProperty>(fn->FindProperty(FName(TEXT("SizeY"),FNAME_Find)));
        const auto valid=[&](FNumericProperty* p){return p&&p->IsInteger()&&p->GetArrayDim()==1
            && p->GetElementSize()==4&&p->HasAnyPropertyFlags(CPF_OutParm)&&p->HasAnyPropertyFlags(CPF_Parm)
            &&p->GetOffset_Internal()>=0&&p->GetOffset_Internal()+4<=fn->GetParmsSize();};
        size_t params=0;for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++params;
        if(params==2&&valid(x)&&valid(y)&&x->GetOffset_Internal()!=y->GetOffset_Internal()) {
            alignas(16) std::array<uint8_t,64> storage{};controller->ProcessEvent(fn,storage.data());
            const auto w=x->GetSignedIntPropertyValue(storage.data()+x->GetOffset_Internal());
            const auto h=y->GetSignedIntPropertyValue(storage.data()+y->GetOffset_Internal());
            if(ValidExtent(static_cast<double>(w),static_cast<double>(h)))return {static_cast<float>(w),static_cast<float>(h)};
        }
    }
    for(const auto& pair:std::array<std::pair<const TCHAR*,const TCHAR*>,2>{{{TEXT("SizeX"),TEXT("SizeY")},{TEXT("ClipX"),TEXT("ClipY")}}}) {
        const auto w=ReadNumeric(canvas,pair.first),h=ReadNumeric(canvas,pair.second);
        if(ValidExtent(w,h))return {static_cast<float>(w),static_cast<float>(h)};
    }
    throw std::runtime_error("The viewport has no validated size yet; reopen Helpy after the world finishes loading");
}
BOOL CALLBACK FindGameWindow(HWND window,LPARAM parameter) {
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId()||!IsWindowVisible(window)||GetWindow(window,GW_OWNER))return TRUE;
    wchar_t name[128]{};GetClassNameW(window,name,128);
    if(std::wcscmp(name,L"UnrealWindow")!=0)return TRUE;
    auto* selected=reinterpret_cast<HWND*>(parameter);
    RECT r{},previous{};GetClientRect(window,&r);if(*selected)GetClientRect(*selected,&previous);
    if((r.right-r.left)*(r.bottom-r.top)>(previous.right-previous.left)*(previous.bottom-previous.top))*selected=window;
    return TRUE;
}
#include "QuickMenuInputAdapter.inl"
} // namespace

void InGameQuickMenu::Close() noexcept {
    CloseWithReason(CloseReason::Requested, "external close request");
}
const char* InGameQuickMenu::CloseReasonText(CloseReason reason) noexcept {
    switch(reason) {
    case CloseReason::Requested: return "requested";
    case CloseReason::F2Toggle: return "Helpy-hotkey";
    case CloseReason::Escape: return "Escape";
    case CloseReason::CloseButton: return "close-button";
    case CloseReason::FocusLost: return "foreground-lost";
    case CloseReason::WindowUnavailable: return "window-unavailable";
    case CloseReason::WorldReset: return "world-reset";
    case CloseReason::OwnerExpired: return "input-owner-expired";
    case CloseReason::OwnerWorldChanged: return "owner-world-changed";
    case CloseReason::NativeModeChange: return "native-input-mode-change";
    case CloseReason::InputQueueOverflow: return "input-queue-overflow";
    case CloseReason::InputRelayError: return "input-relay-error";
    case CloseReason::InputObserverError: return "input-observer-error";
    case CloseReason::FirstFrameTimeout: return "first-frame-timeout";
    case CloseReason::FrameTimeout: return "frame-watchdog";
    case CloseReason::RenderFailure: return "open-or-render-error";
    case CloseReason::Shutdown: return "shutdown";
    }
    return "unknown";
}
void InGameQuickMenu::CloseWithReason(CloseReason reason,const char* detail) noexcept {
    try {
        // Serialize requests with render/cleanup callbacks, but do NOT dispatch
        std::lock_guard lock(s_lifetimeMutex);
        const bool wasOpen=m_open.exchange(false,std::memory_order_acq_rel);
        m_inputReady.store(false,std::memory_order_release);
        m_frameSubmitted.store(false,std::memory_order_release);
        m_inputDiagnostic.store(false,std::memory_order_release);
        m_generation.fetch_add(1,std::memory_order_acq_rel);
        if(!wasOpen)return;
        const auto now=GetTickCount64();
        const auto opened=m_openRequestedAt.load(std::memory_order_acquire);
        const auto last=m_lastFrameAt.load(std::memory_order_acquire);
        const auto count=m_completedFrames.load(std::memory_order_acquire);
        const auto elapsed=now>=opened?now-opened:0;
        const auto frameAge=count&&now>=last?now-last:0;
        PS::Log<LogLevel::Verbose>(STR("Helpy closed [{}] after {} ms; frames={}; last-frame-age={} ms; {}.\n"),
            RC::to_generic_string(CloseReasonText(reason)),elapsed,count,frameAge,
            RC::to_generic_string(detail&&*detail?detail:"no additional detail"));
    }catch(...) {
        m_inputReady.store(false,std::memory_order_release);
        m_open.store(false,std::memory_order_release);
        m_frameSubmitted.store(false,std::memory_order_release);
        m_inputDiagnostic.store(false,std::memory_order_release);
        m_generation.fetch_add(1,std::memory_order_acq_rel);
    }
}
void InGameQuickMenu::Toggle() noexcept {
    try {
        HelpySettings::EnsureLoaded();
        std::lock_guard lock(s_lifetimeMutex);
        if(IsOpen()) {CloseWithReason(CloseReason::F2Toggle, "host toggle callback");return;}
        m_generation.fetch_add(1,std::memory_order_acq_rel);
        m_inputReady.store(false,std::memory_order_release);
        m_frameSubmitted.store(false,std::memory_order_release);
        m_inputDiagnostic.store(false,std::memory_order_release);
        m_openRequestedAt.store(GetTickCount64(),std::memory_order_release);
        m_lastFrameAt.store(0,std::memory_order_release);
        m_completedFrames.store(0,std::memory_order_release);
        m_renderErrorLogged.store(false);
        // Opening Helpy performs only the lightweight target/authority lookup.
        // The embedded plugin catalogue is already displayable; no catalogue
        // refresh, UObject census, cache rebuild, or network fetch is implied.
        if(!m_ui.busy) {
            QuickUI::Command players;players.kind=QuickUI::Command::Kind::Players;
            m_ui.Queue(std::move(players));
        }
        m_open.store(true,std::memory_order_release);
    }catch(...) {CloseWithReason(CloseReason::InputRelayError, "toggle callback failed");}
}
void InGameQuickMenu::SelectPrevious() noexcept {
    if(IsOpen())m_requestedTab.store((m_currentTab.load()+2)%3);
}
void InGameQuickMenu::SelectNext() noexcept {
    if(IsOpen())m_requestedTab.store((m_currentTab.load()+1)%3);
}
void InGameQuickMenu::ResetWorld() {
    CloseWithReason(CloseReason::WorldReset, "world-reset callback");
    ReleaseViewportInput();m_ui.Reset();
    m_catalogRevisions.fill(0);m_lastCatalogReadAt=0;m_lastCatalogCategory=-1;m_catalogReadRequested.store(true,std::memory_order_release);
    m_pendingRequest=0;m_highSurrogate=0;m_wheelRemainder=0;
    SpawnToolRequests::Cancel();
    m_world.Reset();m_seenWorld=false;
    m_canvasIcons.clear();m_failedCanvasIcons.clear();m_queuedCanvasIcons.clear();
    m_canvasIconQueue.clear();m_canvasIconOrder.clear();
    std::lock_guard lock(m_eventMutex);m_events.clear();
}
bool InGameQuickMenu::Initialize(bool reportFailure) {
    if(s_postRenderHook)return true;
    try {
        auto* target=ResolvePostRender();
        Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");
        options.HookName=TEXT("RuneSchemaF2InputSafety");
        m_inputSafetyHook=Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&,UEngine*,float,bool) {
                std::lock_guard lock(s_lifetimeMutex);
                if(s_activeMenu.load(std::memory_order_acquire)==this)TickInputSafety();
            },options);
        if(m_inputSafetyHook==Hook::ERROR_ID)throw std::runtime_error("Input cleanup tick registration failed");
        options.HookName=TEXT("RuneSchemaF2QuickMenuWorldReset");
        m_worldResetHook=Hook::RegisterInitGameStatePreCallback(
            [this](Hook::TCallbackIterationData<void>&,AGameModeBase*) {
                std::lock_guard lock(s_lifetimeMutex);ResetWorld();
            },options);
        if(m_worldResetHook==Hook::ERROR_ID)throw std::runtime_error("World-reset callback registration failed");
        options.HookName=TEXT("RuneSchemaF2NativeInputModeOwner");
        m_inputModeHook=Hook::RegisterProcessEventPreCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject*,UFunction* function,void* parameters) {
                if(s_nativeMenuModeDepth || !m_modeObserverActive.load(std::memory_order_acquire))return;
                std::lock_guard lock(s_lifetimeMutex);
                if(s_activeMenu.load(std::memory_order_acquire)==this)ObserveNativeInputMode(function,parameters);
            },options);
        if(m_inputModeHook==Hook::ERROR_ID)throw std::runtime_error("Native input-mode owner callback registration failed");
        s_activeMenu.store(this,std::memory_order_release);
        if(!PS::InstallInlineHook(s_postRenderHook,target,reinterpret_cast<void*>(&PostRenderThunk)))
            throw std::runtime_error("PostRender detour installation failed");
        PS::Log<LogLevel::Verbose>(TEXT("Helpy Canvas and input hooks installed.\n"));
        return true;
    }catch(const std::exception& e){
        s_activeMenu.store(nullptr);s_postRenderHook={};
        if(m_inputModeHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_inputModeHook);m_inputModeHook=Hook::ERROR_ID;}
        if(m_worldResetHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_worldResetHook);m_worldResetHook=Hook::ERROR_ID;}
        if(m_inputSafetyHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_inputSafetyHook);m_inputSafetyHook=Hook::ERROR_ID;}
        if(reportFailure)PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy unavailable: {}\n"),RC::to_generic_string(e.what()));
        return false;
    }
}
void InGameQuickMenu::Shutdown() {
    {
        std::lock_guard lock(s_lifetimeMutex);
        CloseWithReason(CloseReason::Shutdown, "menu shutdown");ReleaseViewportInput();s_activeMenu.store(nullptr,std::memory_order_release);
    }
    RemoveInputHook();s_postRenderHook={};
    if(m_inputModeHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_inputModeHook);m_inputModeHook=Hook::ERROR_ID;}
    if(m_worldResetHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_worldResetHook);m_worldResetHook=Hook::ERROR_ID;}
    if(m_inputSafetyHook!=Hook::ERROR_ID){Hook::UnregisterCallback(m_inputSafetyHook);m_inputSafetyHook=Hook::ERROR_ID;}
}
bool InGameQuickMenu::HasInputLease() const noexcept {
    return m_nativeModeOwned || m_cursorFlag.owned || m_controllerLocks.Any();
}
void InGameQuickMenu::AbortOpen(const char* stage,const char* reason,CloseReason cause) noexcept {
    const bool hadFrame=m_frameSubmitted.load(std::memory_order_acquire);
    CloseWithReason(cause,stage);
    const bool restored=ReleaseViewportInput();
    try {
        if(!m_renderErrorLogged.exchange(true))
            PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy {} [{}]: {}; input {}.\n"),
                hadFrame?TEXT("closed"):TEXT("open aborted"),RC::to_generic_string(stage),
                RC::to_generic_string(reason),restored?TEXT("restored"):TEXT("cleanup pending on game tick"));
    }catch(...){}
}
void InGameQuickMenu::TickInputSafety() {
    if(const int key=HelpyHotkeys::SuppressedUntilRelease.load();key)
        HelpyHotkeys::ObserveRelease(key,(GetAsyncKeyState(key)&0x8000)!=0);
    // Runs on the game thread, independently of the viewport detour. No UObject
    // enumeration or asset resolution is performed while the menu is idle.
    if(!IsOpen()){if(HasInputLease())ReleaseViewportInput();return;}
    try {
        const HWND window=m_window.load(std::memory_order_acquire);
        if(window&&!IsWindow(window)){CloseWithReason(CloseReason::WindowUnavailable,"input safety tick");ReleaseViewportInput();return;}
        if(window&&GetForegroundWindow()!=window) {
            // Alt-tab suspends Helpy input but preserves the open model, active
            // Item Lab draft and details page. Input is reacquired on return.
            m_inputReady.store(false,std::memory_order_release);if(HasInputLease())ReleaseViewportInput();return;
        }
        const auto state=QuickInput::CheckWatchdog(IsOpen(),window!=nullptr,
            window&&IsWindow(window)&&GetForegroundWindow()==window,
            m_frameSubmitted.load(std::memory_order_acquire),m_openRequestedAt.load(),
            m_lastFrameAt.load(),GetTickCount64());
        if(state==QuickInput::WatchdogResult::LostFocus)return;
        if(state==QuickInput::WatchdogResult::FirstFrameTimeout){
            AbortOpen("first frame","No complete Canvas frame arrived within five seconds",CloseReason::FirstFrameTimeout);return;
        }
        if(state==QuickInput::WatchdogResult::FrameTimeout){
            AbortOpen("frame watchdog","Viewport frames stopped for five seconds",CloseReason::FrameTimeout);return;
        }
        if(HasInputLease()) {
            auto* viewport=m_lockedViewport.Get();
            auto* controller=m_lockedController.Get();
            if(!LiveMenuObject(viewport)||!LiveMenuObject(controller)) {
                CloseWithReason(CloseReason::OwnerExpired, "viewport/controller weak owner expired");
                ReleaseViewportInput();return;
            }
            if(m_seenWorld&&(!m_world.Get()||viewport->GetWorld()!=m_world.Get()
                                          ||controller->GetWorld()!=m_world.Get())) {
                CloseWithReason(CloseReason::OwnerWorldChanged, "input owner no longer belongs to the captured world");
                ReleaseViewportInput();return;
            }
        }
    }catch(const std::exception& e){AbortOpen("input watchdog",e.what());}
    catch(...){AbortOpen("input watchdog","Unknown input/world error");}
}
void InGameQuickMenu::EnsureInputHook() {
    HWND window=m_window.load();
    if(window&&IsWindow(window)&&m_inputHook)return;
    RemoveInputHook();window=nullptr;EnumWindows(&FindGameWindow,reinterpret_cast<LPARAM>(&window));
    if(!window)throw std::runtime_error("No visible Dragonwilds Unreal game window is available");
    DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(window,&pid);
    if(!thread||pid!=GetCurrentProcessId())throw std::runtime_error("Game window ownership changed");
    // Thread-scoped, same-process hook. Never a desktop/global keyboard hook.
    m_window.store(window,std::memory_order_release);m_windowThread=thread;
    m_inputHook=SetWindowsHookExW(WH_GETMESSAGE,&InputThunk,nullptr,thread);
    if(!m_inputHook){m_window.store(nullptr);throw std::runtime_error("Cannot attach input to the game message thread");}
}
void InGameQuickMenu::RemoveInputHook() noexcept {
    if(m_inputHook){UnhookWindowsHookEx(m_inputHook);m_inputHook=nullptr;}
    m_window.store(nullptr);m_windowThread=0;
}
LRESULT CALLBACK InGameQuickMenu::InputThunk(int code,WPARAM removed,LPARAM parameter) {
    if(code<0||removed!=PM_REMOVE)return CallNextHookEx(nullptr,code,removed,parameter);
    try {
        std::lock_guard lifetime(s_lifetimeMutex);
        auto* menu=s_activeMenu.load(std::memory_order_acquire);auto* message=reinterpret_cast<MSG*>(parameter);
        if(!menu||!message||!menu->IsOpen()||message->hwnd!=menu->m_window.load())return CallNextHookEx(nullptr,code,removed,parameter);
        if(message->message==WM_KILLFOCUS||(message->message==WM_ACTIVATEAPP&&!message->wParam)) {
            // Keep the menu model open across alt-tab. The game-thread safety
            // tick releases the input lease while unfocused and restores it on return.
            menu->m_inputReady.store(false,std::memory_order_release);
            return CallNextHookEx(nullptr,code,removed,parameter);
        }
        if(!menu->m_inputReady.load(std::memory_order_acquire)) {
            if(message->message==WM_KEYDOWN&&message->wParam==VK_ESCAPE)
                menu->CloseWithReason(CloseReason::Escape, "Escape before interactive readiness");
            return CallNextHookEx(nullptr,code,removed,parameter);
        }
        Event e{};e.generation=menu->m_generation.load();bool queue=false,consume=false;
        if(message->message==WM_KEYDOWN||message->message==WM_KEYUP) {
            if(message->wParam==static_cast<WPARAM>(HelpyHotkeys::Active.load()))return CallNextHookEx(nullptr,code,removed,parameter);
            consume=true;
            if(message->message==WM_KEYDOWN){
                e.kind=Event::Kind::Key;e.value=static_cast<int>(message->wParam);e.control=(GetKeyState(VK_CONTROL)&0x8000)!=0;queue=true;
                // Translate before replacing the key message, so ordinary typing
                // still arrives as WM_CHAR after the game input has been suppressed.
                if(!e.control)TranslateMessage(message);
            }
        } else if(message->message==WM_CHAR) {
            e.kind=Event::Kind::Character;e.value=static_cast<int>(message->wParam);queue=true;consume=true;
        } else if(message->message==WM_MOUSEWHEEL) {
            e.kind=Event::Kind::Wheel;e.value=static_cast<short>(HIWORD(message->wParam));queue=true;consume=true;
        } else if(message->message==WM_LBUTTONUP||message->message==WM_RBUTTONUP) {
            RECT r{};if(GetClientRect(message->hwnd,&r)&&r.right>0&&r.bottom>0) {
                e.kind=message->message==WM_RBUTTONUP?Event::Kind::RightClick:Event::Kind::Click;
                e.x=static_cast<float>(static_cast<short>(LOWORD(message->lParam)))/static_cast<float>(r.right);
                e.y=static_cast<float>(static_cast<short>(HIWORD(message->lParam)))/static_cast<float>(r.bottom);queue=true;
            }consume=true;
        } else if(message->message==WM_LBUTTONDOWN||message->message==WM_LBUTTONDBLCLK
            ||message->message==WM_RBUTTONDOWN
            ||message->message==WM_MBUTTONDOWN||message->message==WM_MBUTTONUP)consume=true;
        if(queue) {
            std::lock_guard lock(menu->m_eventMutex);
            if(menu->m_events.size()<256)menu->m_events.push_back(e);
            else menu->CloseWithReason(CloseReason::InputQueueOverflow, "256 pending input events"); // Keep the bounded queue safety limit.
        }
        if(consume){message->message=WM_NULL;message->wParam=0;message->lParam=0;}
    }catch(const std::exception& e){if(auto* menu=s_activeMenu.load())menu->CloseWithReason(CloseReason::InputRelayError,e.what());}
    catch(...){if(auto* menu=s_activeMenu.load())menu->CloseWithReason(CloseReason::InputRelayError,"unknown message-relay exception");}
    return CallNextHookEx(nullptr,code,removed,parameter);
}
void InGameQuickMenu::PostRenderThunk(UObject* viewport,UObject* canvas) {
    s_postRenderHook.call(viewport,canvas);
    std::lock_guard lock(s_lifetimeMutex);
    auto* menu=s_activeMenu.load(std::memory_order_acquire);if(!menu)return;
    const char* stage="world/viewport";
    try {
        // The detour can observe other viewport instances. Once an input owner
        // is captured, another live viewport must not reset that owner's menu.
        // The independent world/owner watchdog and reset hook remain active.
        if(menu->IsOpen()&&menu->HasInputLease()) {
            auto* owned=menu->m_lockedViewport.Get();
            if(LiveMenuObject(owned)&&viewport!=owned)return;
        }
        auto* world=viewport?viewport->GetWorld():nullptr;
        if(menu->m_seenWorld&&menu->m_world.Get()!=world) {
            menu->CloseWithReason(CloseReason::OwnerWorldChanged,"captured viewport changed worlds");
            menu->ResetWorld();
        }
        if(world&&menu->m_world.Get()!=world){menu->m_world.Assign(world);menu->m_seenWorld=true;}
        if(!menu->IsOpen()){menu->ReleaseViewportInput();return;}
        if(!LiveMenuObject(canvas)||!world)throw std::runtime_error("No live Canvas and gameplay world yet");
        const auto generation=menu->m_generation.load(std::memory_order_acquire);
        stage="window input";menu->EnsureInputHook();
        if(GetForegroundWindow()!=menu->m_window.load()){
            menu->m_inputReady.store(false,std::memory_order_release);menu->ReleaseViewportInput();return;
        }
        stage="input capture";
        if(!menu->m_inputDiagnostic.load(std::memory_order_acquire)) {
            try { menu->SyncViewportInput(viewport,canvas); }
            catch(const MissingNativeMenuInput& e) {
                if(!menu->ReleaseViewportInput())throw; // No diagnostic while cleanup is pending.
                menu->m_inputReady.store(false,std::memory_order_release);
                menu->m_inputDiagnostic.store(true,std::memory_order_release);
                PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy: input unavailable; attempting read-only Canvas diagnostic. {}.\n"),
                    RC::to_generic_string(e.what()));
            }
        }
        if(menu->m_inputDiagnostic.load(std::memory_order_acquire)) {
            // Deliberately no normal RenderCanvas: no catalogue jobs, commands,
            // click targets, cursor changes or controller/viewport input writes.
            stage="Canvas diagnostic";menu->RenderInputDiagnostic(viewport,canvas);
            if(menu->IsOpen()&&menu->m_generation.load(std::memory_order_acquire)==generation) {
                menu->m_completedFrames.fetch_add(1,std::memory_order_acq_rel);
                menu->m_lastFrameAt.store(GetTickCount64(),std::memory_order_release);
                if(!menu->m_frameSubmitted.exchange(true,std::memory_order_acq_rel))
                    PS::Log<LogLevel::Verbose>(TEXT("Helpy diagnostic Canvas frame submitted.\n"));
            }
            return;
        }
        stage="Canvas drawing";menu->RenderCanvas(canvas);
        if(menu->IsOpen()&&menu->m_generation.load(std::memory_order_acquire)==generation) {
            menu->m_completedFrames.fetch_add(1,std::memory_order_acq_rel);
            menu->m_lastFrameAt.store(GetTickCount64(),std::memory_order_release);
            menu->m_frameSubmitted.store(true,std::memory_order_release);
            if(!menu->m_inputReady.exchange(true,std::memory_order_acq_rel))
                PS::Log<LogLevel::Verbose>(TEXT("Helpy first Canvas frame submitted.\n"));
        }else menu->ReleaseViewportInput();
    }catch(const std::exception& e){menu->AbortOpen(stage,e.what());}
    catch(...){menu->AbortOpen(stage,"Unknown render/input error");}
}
void InGameQuickMenu::RenderInputDiagnostic(UObject* viewport,UObject* canvas) {
    auto* controller=FindLocalPlayerController(viewport);
    const auto [screenW,screenH]=ViewportExtent(canvas,controller);
    const float factor=std::min({1.0f,screenW/900.f,screenH/440.f});
    const float w=820.f*factor,h=350.f*factor;
    const float x=(screenW-w)*.5f,y=(screenH-h)*.5f;
    DrawRect(canvas,x,y,w,h,.035f,.045f,.055f,.97f);
    DrawText(canvas,"RuneSchema Helpy - render diagnostic",x+24*factor,y+22*factor,
             .95f*factor,.95f,.97f,1.f);
    DrawText(canvas,"ITEMS                ENEMIES                RESOURCES",x+24*factor,y+70*factor,
             .8f*factor,.55f,.6f,.65f);
    DrawText(canvas,"INPUT UNAVAILABLE - THIS PANEL IS NOT INTERACTIVE",x+24*factor,y+130*factor,
             .78f*factor,1.f,.8f,.45f);
    DrawText(canvas,"The viewport drawing path reached this diagnostic.",x+24*factor,y+179*factor,
             .72f*factor,.9f,.93f,.96f);
    DrawText(canvas,"No input lock, item grant or spawn has been enabled.",x+24*factor,y+214*factor,
             .72f*factor,.9f,.93f,.96f);
    DrawText(canvas,"Gameplay still receives input. Your Helpy hotkey or Escape closes this panel.",x+24*factor,y+265*factor,
             .68f*factor,.75f,.8f,.85f);
    DrawText(canvas,"The UE4SS log contains the exact input-backend failure.",x+24*factor,y+299*factor,
             .65f*factor,.75f,.8f,.85f);
}
UObject* InGameQuickMenu::FindLocalPlayerController(UObject* viewportClient) {
    if (!viewportClient) return nullptr;
    auto* world = viewportClient->GetWorld();
    if (!world) return nullptr;
    auto* controllerClass = ActorHelper::ResolveClass(TEXT("/Script/Engine.PlayerController"));
    auto* localPlayerClass = ActorHelper::ResolveClass(TEXT("/Script/Engine.LocalPlayer"));
    if (!controllerClass || !localPlayerClass) return nullptr;

    TArray<UObject*> controllers;
    UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
    if (controllers.Num() > 256) return nullptr;
    for (auto* controller : controllers) {
        if (!controller || controller->GetWorld() != world
            || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
            continue;
        auto* player = ActorHelper::GetObjectRef(controller, TEXT("Player"));
        if (player && player->IsA(localPlayerClass)) return controller;
    }
    return nullptr;
}

void InGameQuickMenu::CaptureControllerInput(UObject* controller) {
    if(!LiveMenuObject(controller))throw std::runtime_error("No live local controller for input capture");
    if(m_controllerLocks.Any()||m_cursorFlag.owned)
        throw std::runtime_error("Previous controller input cleanup is incomplete");

    // Preflight both stacked functions and secure a weak owner BEFORE any
    // engine state changes. Missing optional cursor reflection is harmless:
    // RenderCanvas already draws its own pointer.
    const auto look=ValidateMenuInputCall(TEXT("/Script/Engine.Controller:SetIgnoreLookInput"),TEXT("bNewLookInput"));
    const auto move=ValidateMenuInputCall(TEXT("/Script/Engine.Controller:SetIgnoreMoveInput"),TEXT("bNewMoveInput"));
    QuickInput::BoolSlot cursor{};
    bool hasCursor=false;
    try{hasCursor=FindReflectedMenuBool(controller,TEXT("bShowMouseCursor"),cursor);}catch(...){hasCursor=false;}
    m_lockedController.Assign(controller);
    if(m_lockedController.Get()!=controller)throw std::runtime_error("Cannot retain a safe controller input owner");
    try {
        m_controllerLocks.Acquire([&](bool value){InvokeMenuInputCall(controller,look,value);},
                                  [&](bool value){InvokeMenuInputCall(controller,move,value);});
        if(hasCursor) {
            m_cursorSlot=cursor;
            m_cursorFlag.Acquire([&]{return ReadMenuBool(controller,TEXT("bShowMouseCursor"),m_cursorSlot);},
                                 [&](bool value){WriteMenuBool(controller,TEXT("bShowMouseCursor"),m_cursorSlot,value);});
        }
    }catch(...){ReleaseControllerInput();throw;}
}

bool InGameQuickMenu::ReleaseControllerInput() noexcept {
    auto* controller=m_lockedController.Get();
    if(!LiveMenuObject(controller)) {
        // An expired weak handle must never be replaced with a new controller.
        m_controllerLocks.ForgetExpiredObject();m_cursorFlag.ForgetExpiredObject();
    }else {
        m_cursorFlag.Release([&](bool value){WriteMenuBool(controller,TEXT("bShowMouseCursor"),m_cursorSlot,value);});
        m_controllerLocks.Release([&](bool value){
            InvokeMenuInputCall(controller,ValidateMenuInputCall(TEXT("/Script/Engine.Controller:SetIgnoreLookInput"),TEXT("bNewLookInput")),value);
        },[&](bool value){
            InvokeMenuInputCall(controller,ValidateMenuInputCall(TEXT("/Script/Engine.Controller:SetIgnoreMoveInput"),TEXT("bNewMoveInput")),value);
        });
    }
    const bool released=!m_controllerLocks.Any()&&!m_cursorFlag.owned;
    if(released && !m_nativeModeOwned){m_lockedController.Reset();m_cursorSlot={};}
    return released;
}

void InGameQuickMenu::ObserveNativeInputMode(UFunction* function,void* parameters) {
    if(!m_nativeModeOwned || s_nativeMenuModeDepth || GetCurrentThreadId()!=m_modeThread || !parameters
        || (function!=m_uiOnlyFunction && function!=m_gameOnlyFunction && function!=m_gameAndUIFunction))return;
    try {
        auto* field=CastField<FObjectPropertyBase>(function->FindProperty(FName(TEXT("PlayerController"),FNAME_Find)));
        if(!field || field->GetArrayDim()!=1 || field->GetElementSize()!=sizeof(UObject*)
            || !QuickInput::InBounds(field->GetOffset_Internal(),field->GetElementSize(),function->GetParmsSize()))return;
        auto* controller=ReadMenuObjectParameter(function,field,parameters,
            static_cast<std::size_t>(function->GetParmsSize()));
        if(controller!=m_lockedController.Get())return;
        // A later reflected mode request owns the UI now. Do not overwrite it
        // with GameOnly or restore our saved cursor over the new menu's cursor.
        m_nativeModeOwned=false;m_modeObserverActive.store(false,std::memory_order_release);
        m_cursorFlag.ForgetExpiredObject();
        const char* requested=function==m_uiOnlyFunction?"SetInputMode_UIOnlyEx":
            function==m_gameOnlyFunction?"SetInputMode_GameOnly":"SetInputMode_GameAndUIEx";
        CloseWithReason(CloseReason::NativeModeChange,requested);
    } catch(const std::exception& e) { CloseWithReason(CloseReason::InputObserverError,e.what()); }
    catch(...) { CloseWithReason(CloseReason::InputObserverError,"unknown native input-mode observation error"); }
}
void InGameQuickMenu::SyncViewportInput(UObject* viewportClient,UObject* canvas) {
    if(!IsOpen()){ReleaseViewportInput();return;}
    if(!LiveMenuObject(viewportClient))throw std::runtime_error("The local game viewport is not live");
    if(HasInputLease()) {
        if(m_nativeModeOwned && m_lockedViewport.Get()==viewportClient
            && m_controllerLocks.look && m_controllerLocks.move && LiveMenuObject(m_lockedController.Get()))return;
        if(!ReleaseViewportInput())throw std::runtime_error("Previous native input-mode cleanup is incomplete");
    }
    auto* controller=FindLocalPlayerController(viewportClient);
    if(!LiveMenuObject(controller))throw std::runtime_error("No live local player controller is available in this viewport");
    auto* pawn=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
    if(!LiveMenuObject(pawn) || pawn->GetWorld()!=viewportClient->GetWorld())
        throw std::runtime_error("RuneSchema Helpy is available while controlling a character in a loaded world");

    const auto enter=PrepareNativeMenuMode(true);
    const auto exit=PrepareNativeMenuMode(false);
    PreflightNativeMenuMode(controller,enter);
    PreflightNativeMenuMode(controller,exit);
    QuickInput::BoolSlot cursor{};
    if(!FindReflectedMenuBool(controller,TEXT("bShowMouseCursor"),cursor))
        throw MissingNativeMenuInput("Cannot inspect the controller cursor state before opening");
    if(ReadMenuBool(controller,TEXT("bShowMouseCursor"),cursor)
        || QueryControllerInputLock(controller,TEXT("/Script/Engine.Controller:IsLookInputIgnored"))
        || QueryControllerInputLock(controller,TEXT("/Script/Engine.Controller:IsMoveInputIgnored")))
        throw std::runtime_error("Close the active game menu or HUD edit mode before opening Helpy");
    (void)ViewportExtent(canvas,controller);
    if(!CanvasFunction(TEXT("/Script/Engine.Canvas:K2_DrawTexture"))
        ||!CanvasFunction(TEXT("/Script/Engine.Canvas:K2_DrawText"))||!MenuWhiteTexture()||!MenuFont())
        throw std::runtime_error("Required Canvas drawing assets/functions are unavailable");
    m_lockedViewport.Assign(viewportClient);
    if(m_lockedViewport.Get()!=viewportClient)throw std::runtime_error("Cannot retain a safe viewport input owner");
    m_modeThread=GetCurrentThreadId();
    m_uiOnlyFunction=enter.function;
    m_gameOnlyFunction=CanvasFunction(TEXT("/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameOnly"));
    m_gameAndUIFunction=CanvasFunction(TEXT("/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameAndUIEx"));
    try {
        CaptureControllerInput(controller);
        m_modeObserverActive.store(true,std::memory_order_release);
        InvokeNativeMenuMode(controller,enter,&m_nativeModeOwned);
    }catch(...){ReleaseViewportInput();throw;}
}

bool InGameQuickMenu::ReleaseViewportInput() noexcept {
    if(m_nativeModeOwned) {
        auto* controller=m_lockedController.Get();auto* viewport=m_lockedViewport.Get();
        if(!LiveMenuObject(controller) || !LiveMenuObject(viewport)) {
            // Never call an input mode on a replacement controller/world.
            m_nativeModeOwned=false;
        }else if(GetForegroundWindow()==m_window.load()) {
            try {
                InvokeNativeMenuMode(controller,PrepareNativeMenuMode(false));
                m_nativeModeOwned=false;
            }catch(...){} // Retain ownership for the independent cleanup tick.
        }
        // With the game unfocused, defer the engine's mouse recapture request
        // until focus returns. Movement/look and our cursor changes still unwind.
    }
    if(!m_nativeModeOwned)m_modeObserverActive.store(false,std::memory_order_release);
    ReleaseControllerInput();
    if(!m_nativeModeOwned){m_lockedViewport.Reset();m_modeThread=0;
        m_uiOnlyFunction=nullptr;m_gameOnlyFunction=nullptr;m_gameAndUIFunction=nullptr;}
    const bool restored=!HasInputLease();
    // A temporarily unfocused viewport is a normal cleanup state. The safety
    // tick continues restoration silently and clears the marker when complete.
    if(!restored)m_cleanupErrorLogged=true;
    else m_cleanupErrorLogged=false;
    return restored;
}

void InGameQuickMenu::ReadCatalog() {
    const auto category=std::clamp(static_cast<int>(m_ui.tab),0,2);
    const auto now=GetTickCount64();
    const bool categoryChanged=category!=m_lastCatalogCategory;
    const bool requested=m_catalogReadRequested.exchange(false,std::memory_order_acq_rel);
    if(!categoryChanged&&!requested)return;
    // RuneSchema pushes revisions and receipts into the plugin cache. Rendering
    // consumes that cache only after a push or a category navigation event.
    m_lastCatalogCategory=category;m_lastCatalogReadAt=now;
    nlohmann::json result;
    const bool changed=SpawnToolRequests::ReadQuickIfChanged(m_catalogRevisions[category],category,result);
    nlohmann::json receipt;
    const bool completed=m_pendingRequest&&SpawnToolRequests::TakeCompleted(m_pendingRequest,receipt);
    if(!changed&&!completed)return;
    m_presentedFrameValid=false;
    m_ui.status=(completed?receipt:result).value("Status",std::string("Ready."));
    if(changed){
        m_ui.indexing=result.value("_CatalogIndexing",false);
        m_ui.catalogStatus=result.value("CatalogStatus",std::string{});
        m_ui.indexStage=result.value("IndexStage",std::string("Not indexed"));
        m_ui.indexDetail=result.value("IndexDetail",std::string{});
        m_ui.indexDone=result.value("IndexDone",std::size_t{});
        m_ui.indexTotal=result.value("IndexTotal",std::size_t{});
        m_ui.indexHasTotal=result.value("IndexHasTotal",false);
        m_ui.indexFinished=result.value("IndexFinished",false);
    }
    if(completed) {
        m_ui.busy=false;m_pendingRequest=0;
        const auto readChoices=[](const nlohmann::json& rows) {
            std::vector<QuickUI::Choice> out;
            for(const auto& row:rows)out.push_back({row.value("Id",std::string{}),row.value("Name",std::string{}),row.value("Path",std::string{}),
                row.value("Row",std::string{}),row.value("Array",std::string{}),row.value("Category",std::string{}),row.value("Grouped",false),row.value("Icon",std::string{})});
            return out;
        };
        // Consume a receipt once, only for this menu's acknowledged request.
        if(receipt.contains("GrantResults")&&receipt["GrantResults"].is_array()) {
            m_ui.grantReport.clear();
            for(const auto& row:receipt["GrantResults"]) {
                const auto item=row.value("Item",std::string{}),state=row.value("State",std::string{});
                if(state=="Confirmed")m_ui.selection.erase(item);
                const auto* entry=m_ui.FindItem(item);
                m_ui.grantReport.push_back({entry?entry->name:item,state,row.value("Message",std::string{})});
            }
            m_ui.reportScroll=0;m_ui.reportOpen=true;
        }
        if(receipt.contains("ItemDetails")&&receipt["ItemDetails"].is_object()) {
            const auto& details=receipt["ItemDetails"];
            m_ui.itemDetailFields.clear();m_ui.itemDetailRecipes.clear();m_ui.itemDetailJournals.clear();
            for(const auto& row:details.value("Fields",nlohmann::json::array()))m_ui.itemDetailFields.push_back({row.value("Name",std::string{}),
                row.value("Type",std::string{}),row.contains("Value")?row["Value"].dump():std::string{},row.value("Reason",std::string{}),
                row.value("Editable",false),row.value("HasValue",false),row.value("VisualType",std::string{}),row.value("Kind",std::string{}),row.value("Scope",std::string{}),row.value("NativeName",std::string{}),row.value("Table",std::string{}),row.value("Row",std::string{})});
            m_ui.stationChoices=readChoices(details.value("StationChoices",nlohmann::json::array()));
            const auto& relations=details.value("Relations",nlohmann::json::object());
            for(const auto& recipe:relations.value("Recipes",nlohmann::json::array())) {
                QuickUI::Model::DetailRecipe value;value.name=recipe.value("Name",std::string{});value.path=recipe.value("Path",std::string{});
                for(const auto& ingredient:recipe.value("Ingredients",nlohmann::json::array())) {
                    const auto path=ingredient.value("ItemData",std::string{});const auto count=ingredient.value("Count",1);const auto* item=m_ui.FindItem(path);
                    const auto name=item?item->name:QuickDecorations::ReadableAssetName(path);value.ingredientLabels.push_back(std::to_string(count)+" x "+name);
                    if(!path.empty())value.ingredients.push_back({path,name,item?item->icon:std::string{},std::to_string(count)});
                }
                for(const auto& output:recipe.value("Outputs",nlohmann::json::array()))if(output.value("ItemData",std::string{})==m_ui.itemDetailsPath)value.output=std::to_string(output.value("Count",1));
                for(const auto& station:recipe.value("Stations",nlohmann::json::array())) {
                    value.stationLabels.push_back(station.value("Name",std::string{})+" / "+station.value("Table",std::string{})+" / "+station.value("Row",std::string{}));
                    if(value.station.path.empty())value.station={station.value("Path",std::string{})+":"+station.value("Row",std::string{})+":"+station.value("Array",std::string{}),station.value("Name",std::string{}),station.value("Path",std::string{}),station.value("Row",std::string{}),station.value("Array",std::string{}),station.value("Category",std::string{}),false,station.value("Icon",std::string{})};
                }
                m_ui.itemDetailRecipes.push_back(std::move(value));
            }
            for(const auto& journal:relations.value("JournalEntries",nlohmann::json::array()))m_ui.itemDetailJournals.push_back(journal.value("Name",std::string{})+" / "+journal.value("Path",std::string{}));
            m_ui.itemDetailsLoaded=true;m_ui.itemDetailsPage=0;
        }
        if(receipt.contains("CloneSource") && receipt["CloneSource"].is_object()) {
            const auto& source=receipt["CloneSource"];std::vector<QuickUI::Model::CloneField> fields;
            for(const auto& row:source.at("Fields"))fields.push_back({row.at("Name").get<std::string>(),
                row.at("Type").get<std::string>(),row.contains("Value")?row["Value"].dump():std::string{},
                row.value("Reason",std::string{}),row.value("Editable",false),row.value("HasValue",false),row.value("VisualType",std::string{}),row.value("Kind",std::string{}),row.value("Scope",std::string{}),row.value("NativeName",std::string{}),row.value("Table",std::string{}),row.value("Row",std::string{})});
            m_ui.journalChoices=readChoices(source.value("JournalChoices",nlohmann::json::array()));
            m_ui.stationChoices=readChoices(source.value("StationChoices",nlohmann::json::array()));
            m_ui.AcceptCloneSource(source.at("Source").get<std::string>(),std::move(fields),source.value("SoftDeleteField",std::string{}));
            auto& card=m_ui.cloneSourceInfo;card.path=source.at("Source").get<std::string>();card.id=card.path;
            card.name=source.value("Name",m_ui.cloneTitle);card.icon=source.value("Icon",std::string{});
            card.cooked=source.value("Cooked",false);card.assetClass=source.value("Class",std::string{});card.appearanceGroup=source.value("AppearanceGroup",std::string{});
            card.power=source.contains("PowerLevel")&&source["PowerLevel"].is_number()?source["PowerLevel"].get<double>():-1;
            m_ui.cloneTitle=card.name;
            m_ui.cloneName=card.name.size()<=250?card.name+" Clone":std::string{};
        }
        if(receipt.contains("CloneAppearance")&&receipt["CloneAppearance"].is_object()) {
            const auto& source=receipt["CloneAppearance"];auto& card=m_ui.cloneAppearanceInfo;
            card.path=source.at("Source").get<std::string>();card.id=card.path;
            card.name=source.value("Name",card.path);card.icon=source.value("Icon",std::string{});
            card.cooked=source.value("Cooked",false);card.assetClass=source.value("Class",std::string{});card.appearanceGroup=source.value("AppearanceGroup",std::string{});
            card.power=source.contains("PowerLevel")&&source["PowerLevel"].is_number()?source["PowerLevel"].get<double>():-1;
            m_ui.cloneAppearanceReady=card.path==m_ui.cloneAppearance&&card.cooked&&ClonePresentation::CompatibleAppearance(m_ui.cloneSourceInfo.assetClass,card.assetClass,m_ui.cloneSourceInfo.appearanceGroup,card.appearanceGroup);
            if(!m_ui.cloneAppearanceReady)m_ui.status="Appearance donor changed or has an incompatible item class.";
        }
        if(receipt.contains("CloneResult")) {
            const auto& clone=receipt["CloneResult"];
            const bool ok=clone.value("State",std::string{})=="Completed";
            m_ui.cloneAcknowledged=false;
            if(clone.contains("Path"))m_ui.cloneCreated=true;
            m_ui.ShowActionResult(ok?"Item created":clone.value("State",std::string{})=="CreatedFilesIncomplete"?"Item files incomplete; grant blocked":clone.contains("Path")?"Item created; check inventory":"Clone stopped",m_ui.status,ok);
        }
        if(receipt.contains("RecipeExportResult"))m_ui.ShowActionResult("Recipe JSON exported",m_ui.status,true);
        if(receipt.contains("ItemOverrideExportResult"))m_ui.ShowActionResult("Item overrides exported",m_ui.status,true);
        if(receipt.contains("SpawnResult")) {
            const bool ok=receipt["SpawnResult"].value("State",std::string{})=="Completed";
            m_ui.ShowActionResult(ok?"Spawn completed":receipt["SpawnResult"].value("Permanent",false)?"JSON saved; check activation":"Spawn stopped",m_ui.status,ok);
        }
    }
    if(!changed)return;
    if(!result.contains("Items")&&!result.contains("Definitions")&&!result.contains("Players")&&!result.contains("CookedVisuals")&&!result.contains("CatalogIssues"))return;
    QuickUI::Catalog data=m_ui.catalog;
    if(result.contains("Items")&&result["Items"].is_array()) {
        data.entries[0].clear();std::set<std::string> seen;
        for(const auto& row:result["Items"]) {
            const auto path=row.value("Path",std::string{});if(path.empty()||!seen.insert(path).second)continue;
            data.entries[0].push_back({path,row.value("Name",path),path,row.value("Icon",std::string{}),
                row.value("InternalName",std::string{})+" "+row.value("PersistenceID",std::string{})+" "+row.value("Tags",std::string{}),
                row.contains("PowerLevel")&&row["PowerLevel"].is_number()?row["PowerLevel"].get<double>():-1,
                row.value("Cooked",false),row.value("Class",std::string{}),row.value("Available",true),row.value("Reason",std::string{}),row.value("AppearanceGroup",std::string{}),
                row.value("RuntimeClone",false),row.value("Masterwork",false),row.value("Consumable",false),row.value("QuestItem",false),row.value("CategoryIcon",std::string{}),row.value("Tags",std::string{}),row.value("RuneSchemaManaged",false),
                row.value("DeclaredModded",false),row.value("DeclaredCooked",false),row.value("CloneEligible",false),row.value("CloneReason",std::string{})});
        }
    }
    if(result.contains("Definitions")&&result["Definitions"].is_array()) {
        const auto responseCategory=result.value("_Category",-1);
        if(responseCategory==1)data.entries[1].clear();
        else if(responseCategory==2)data.entries[2].clear();
        else {data.entries[1].clear();data.entries[2].clear();}
        std::set<std::string> seen;
        for(const auto& row:result["Definitions"]) {
            const auto type=row.value("Type",std::string{});if(type!="AI"&&type!="Resource"&&type!="NPC")continue;
            const auto id=row.value("Key",std::string{}),path=row.value("Class",std::string{});
            if(id.empty()||path.empty()||!seen.insert(id).second)continue;
            auto name=row.value("Name",std::string{});if(name.empty())name=path.substr(path.find_last_of("./")+1);
            const int slot=type=="Resource"?2:1;
            data.entries[slot].push_back({id,name,path,row.value("Icon",std::string{}),id,
                row.contains("PowerLevel")&&row["PowerLevel"].is_number()?row["PowerLevel"].get<double>():-1,
                row.value("Packaged",false),{},row.value("Available",true),row.value("Reason",std::string{})});
            auto& e=data.entries[slot].back();e.nodeKind=type;e.resourceFamily=row.value("ResourceFamily",std::string{});
            e.runeSchemaManaged=row.value("RuneSchemaManaged",false);e.declaredModded=row.value("DeclaredModded",false);e.declaredCooked=row.value("DeclaredCooked",false);
            e.temporaryAllowed=row.value("TemporaryAllowed",false);e.permanentAllowed=row.value("PermanentAllowed",false);
            e.temporaryReason=row.value("TemporaryReason",std::string{});e.permanentReason=row.value("PermanentReason",std::string{});
            e.npcVendor=row.value("Vendor",false);e.npcQuestGiver=row.value("QuestGiver",false);
            e.npcLore=row.value("Lore",false);e.npcDialogue=row.value("Dialogue",false);
        }
    }
    if(result.contains("CookedVisuals")&&result["CookedVisuals"].is_array()) {
        data.visuals.clear();
        for(const auto& row:result["CookedVisuals"]) {
            const auto path=row.value("Path",std::string{});if(path.empty())continue;
            data.visuals.push_back({path,row.value("Name",path),path,{},{},-1,true,row.value("Class",std::string{})});
        }
    }
    if(result.contains("CatalogIssues")&&result["CatalogIssues"].is_array()) {
        data.issues.clear();
        for(const auto& row:result["CatalogIssues"])data.issues.push_back({row.value("Path",std::string{}),row.value("Reason",std::string{})});
    }
    m_ui.coverageSummary=result.value("CatalogCoverage",std::string{})+" | NPC/AI: "+std::to_string(data.entries[1].size())+
        " | Resources: "+std::to_string(data.entries[2].size())+" | Unresolved: "+std::to_string(result.value("UnresolvedAssets",std::size_t{}))+
        " | Unclassified: "+std::to_string(result.value("UnclassifiedRecords",std::size_t{}));
    if(result.contains("Players")&&result["Players"].is_array()) {
        data.players.clear();const auto self=m_lockedController.Get()?RC::to_string(m_lockedController.Get()->GetPathName()):std::string{};
        for(const auto& row:result["Players"]) {
            const auto path=row.value("Path",std::string{});if(!path.empty())data.players.push_back({path,row.value("Name",path),path==self});
        }
        data.authority=!data.players.empty();
    }
    m_ui.Update(std::move(data));
}
void InGameQuickMenu::Submit(const QuickUI::Command& command) {
    if(command.kind==QuickUI::Command::Kind::SetHotkey) {
        HelpySettings::SetKey(command.name);m_ui.helpyKey=HelpyHotkeys::Name();m_ui.status=HelpySettings::Message();m_ui.busy=false;return;
    }
    if(command.kind==QuickUI::Command::Kind::Cancel){SpawnToolRequests::Cancel();return;}
    const bool spawning=command.kind==QuickUI::Command::Kind::Spawn;
    const bool cloning=command.kind==QuickUI::Command::Kind::CreateClone;
    try {
        const auto id=++m_requestSequence;nlohmann::json request={{"_RequestId",id},{"_Source","Helpy"}};
        if(auto* world=m_world.Get())request["World"]=RC::to_string(world->GetPathName());
        if(command.kind==QuickUI::Command::Kind::UpdateReference)request["Action"]="UpdateHelpyReference";
        else if(command.kind==QuickUI::Command::Kind::Index) {
            if(!PSConfig::Get()->GetSettings().advancedRuntime)
                throw std::runtime_error("Full catalog scans require Advanced Runtime");
            request["Action"]="IndexQuickCatalog";
        }
        else if(command.kind==QuickUI::Command::Kind::Refresh){request["Action"]="QuickCatalog";m_failedCanvasIcons.clear();}
        else if(command.kind==QuickUI::Command::Kind::Players)request["Action"]="QuickPlayers";
        else if(command.kind==QuickUI::Command::Kind::DismissNpcs){request["Action"]="DismissHelpyNpcs";request["Player"]=command.player;}
        else if(command.kind==QuickUI::Command::Kind::Give){
            request["Action"]="GiveItems";request["Player"]=command.player;request["Items"]=nlohmann::json::array();
            for(const auto& g:command.grants)request["Items"].push_back({{"Item",g.item},{"Count",g.count}});
        }else if(command.kind==QuickUI::Command::Kind::InspectClone||command.kind==QuickUI::Command::Kind::InspectDetails) {
            Authoring::ValidateObjectPath(command.definition);
            request["Action"]="InspectClone";request["Source"]=command.definition;request["Player"]=command.player;request["InspectAppearance"]=command.inspectAppearance;
            request["InspectDetails"]=command.kind==QuickUI::Command::Kind::InspectDetails;
        }else if(command.kind==QuickUI::Command::Kind::ExportRecipe) {
            Authoring::ValidateObjectPath(command.definition);request["Action"]="ExportRecipe";request["Source"]=command.definition;
            const auto encodeChoice=[](const QuickUI::Choice& c) {return nlohmann::json{{"Path",c.path},{"Row",c.row},{"Array",c.array},{"Category",c.category},{"Grouped",c.grouped}};};
            request["Recipe"]={{"Enabled",true},{"Unlock",command.unlockRecipe},{"Station",encodeChoice(command.recipeStation)},
                {"Category",command.recipeCategory},{"Count",QuickUI::Integer(command.recipeOutput,1,10000,"Recipe output count")},{"Ingredients",nlohmann::json::array()}};
            for(const auto& i:command.ingredients)request["Recipe"]["Ingredients"].push_back({{"ItemData",i.path},{"Count",QuickUI::Integer(i.count,1,10000,"Ingredient count")}});
        }else if(command.kind==QuickUI::Command::Kind::ExportItemOverrides) {
            Authoring::ValidateObjectPath(command.definition);request["Action"]="ExportItemOverrides";request["Source"]=command.definition;request["Overrides"]=nlohmann::json::object();
            for(const auto& [key,text]:command.overrides){if(text.size()>Authoring::MaxCloneValueBytes)throw std::runtime_error("Item override exceeds 16 KiB.");request["Overrides"][key]=Authoring::ParseCloneJson(text);}
        }else if(cloning) {
            auto overrides=nlohmann::json::object(),statOverrides=nlohmann::json::object();
            for(const auto& [key,text]:command.overrides) {
                if(text.size()>Authoring::MaxCloneValueBytes)throw std::runtime_error("Clone JSON value exceeds 16 KiB.");
                if(const auto stat=HelpyStatKey::Parse(key))statOverrides[stat->handle][stat->field]=Authoring::ParseCloneJson(text);
                else overrides[key]=Authoring::ParseCloneJson(text);
            }
            const auto fields=Authoring::CloneRequest(command.definition,command.name,command.iconPath,std::move(overrides),
                command.permanent,command.giveClone,command.count,command.acknowledgeExperimental,command.player,
                command.modTag,command.persistenceId,command.appearanceSource,command.meshField,command.meshPath);
            request.update(fields);request["StatOverrides"]=std::move(statOverrides);request["IconMode"]=command.iconMode;
            const auto encodeChoice=[](const QuickUI::Choice& c) {return nlohmann::json{{"Path",c.path},{"Row",c.row},{"Array",c.array},{"Category",c.category},{"Grouped",c.grouped}};};
            request["Journal"]={{"Enabled",command.makeJournal},{"Text",command.journalText},{"Title",command.journalTitle},
                {"Target",encodeChoice(command.journalTarget)},{"Group",command.journalGroup},{"GroupName",command.journalGroupName}};
            request["Recipe"]={{"Enabled",command.makeRecipe},{"Unlock",command.unlockRecipe},{"Station",encodeChoice(command.recipeStation)},
                {"Category",command.recipeCategory},{"Count",command.makeRecipe?QuickUI::Integer(command.recipeOutput,1,10000,"Recipe output count"):1},{"Ingredients",nlohmann::json::array()}};
            if(command.makeRecipe)for(const auto& i:command.ingredients)request["Recipe"]["Ingredients"].push_back({{"ItemData",i.path},{"Count",QuickUI::Integer(i.count,1,10000,"Ingredient count")}});
        }else{
            request["AdditionalDrops"]=nlohmann::json::array();
            QuickUI::EncodeSpawn(command,[&](const char* key,const auto& value){request[key]=value;},
                [&](const QuickUI::Loot& d){request["AdditionalDrops"].push_back({{"Item",d.item},{"Min",d.min},{"Max",d.max},{"ChancePercent",d.chance}});});
        }
        if(SpawnToolRequests::TrySubmit(std::move(request))){
            m_pendingRequest=id;m_ui.busy=true;
            m_ui.status=spawning?"Spawn request queued for the shared game-thread backend.":"Command queued for the game thread.";
            if(spawning)PS::Log<LogLevel::Normal>(STR("RuneSchema Helpy spawn #{} queued: {} x {}.\n"),id,command.count,RC::to_generic_string(command.classPath));
        }else{
            m_ui.busy=false;m_ui.status="The runtime is unavailable or another command is pending. Nothing was submitted.";
            if(spawning||cloning)m_ui.ShowActionResult("Action not submitted",m_ui.status,false);
        }
    }catch(const std::exception& e){
        m_ui.busy=false;m_ui.status=e.what();
        if(spawning||cloning)m_ui.ShowActionResult("Action not submitted",m_ui.status,false);
        PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy command not submitted: {}\n"),RC::to_generic_string(e.what()));
    }
}
void InGameQuickMenu::RenderCanvas(UObject* canvas) {
    const auto [screenW,screenH]=ViewportExtent(canvas,m_lockedController.Get());
    // Fit against the real logical extent and permit modest growth on high-DPI
    // viewports. The margin remains resolution independent and ultrawide
    // screens are constrained by height rather than stretched horizontally.
    const float factor=std::min({(screenW-32)/QuickUI::Width,(screenH-32)/QuickUI::Height,QuickUI::MaxViewportScale});
    if(factor<=0)throw std::runtime_error("The viewport is too small");
    const float x=(screenW-QuickUI::Width*factor)*.5f,y=(screenH-QuickUI::Height*factor)*.5f;
    const auto generation=m_generation.load();
    if(generation!=m_seenGeneration){m_seenGeneration=generation;m_highSurrogate=0;m_wheelRemainder=0;m_ui.closeRequested=false;m_ui.focus.clear();m_presentedFrameValid=false;}
    const auto requested=m_requestedTab.exchange(-1);
    if(requested>=0&&!m_ui.node&&!m_ui.lootPicker&&!m_ui.playerPicker&&!m_ui.ItemPickerOpen()&&!m_ui.cloneFieldOpen&&!m_ui.cloneAdvancedOpen&&!m_ui.cloneMeshPicker&&!m_ui.cloneMeshFieldPicker&&!m_ui.cloneModeOpen&&!m_ui.coverageOpen){m_ui.tab=static_cast<QuickUI::Tab>(std::clamp(requested,0,2));m_presentedFrameValid=false;}
    HelpySettings::EnsureLoaded();m_ui.helpyKey=HelpyHotkeys::Name();
    m_ui.advancedRuntime=PSConfig::Get()->GetSettings().advancedRuntime;
    SyncFavorites(m_ui);
    ReadCatalog();
    POINT mouse{};RECT client{};const HWND window=m_window.load();GetCursorPos(&mouse);ScreenToClient(window,&mouse);GetClientRect(window,&client);
    const float mx=client.right>0?(static_cast<float>(mouse.x)/static_cast<float>(client.right)*screenW-x)/factor:-1;
    const float my=client.bottom>0?(static_cast<float>(mouse.y)/static_cast<float>(client.bottom)*screenH-y)/factor:-1;
    std::vector<Event> events;{std::lock_guard lock(m_eventMutex);events.swap(m_events);}
    for(const auto& event:events) {
        if(event.generation!=generation)continue;
        if(event.kind==Event::Kind::Click||event.kind==Event::Kind::RightClick){
            // Input belongs to the scene the player actually saw. Reuse that
            // scene for hit testing instead of rebuilding the complete UI just
            // to discover the same rectangles.
            QuickUI::Frame fallback;
            const QuickUI::Frame* hitFrame=nullptr;
            if(m_presentedFrameValid&&m_presentedFrameGeneration==generation)hitFrame=&m_presentedFrame;
            else {fallback=m_ui.Render(mx,my);hitFrame=&fallback;}
            const float hitX=(event.x*screenW-x)/factor,hitY=(event.y*screenH-y)/factor;
            if(event.kind==Event::Kind::Click)m_ui.Click(*hitFrame,hitX,hitY);
            else m_ui.RightClick(*hitFrame,hitX,hitY);
            m_presentedFrameValid=false;
        }
        else if(event.kind==Event::Kind::Wheel){m_wheelRemainder+=event.value;const int steps=m_wheelRemainder/WHEEL_DELTA;m_wheelRemainder%=WHEEL_DELTA;if(steps){m_ui.Wheel(steps);m_presentedFrameValid=false;}}
        else if(event.kind==Event::Kind::Key){m_ui.Key(event.value,event.control);m_presentedFrameValid=false;}
        else{
            const auto unit=static_cast<uint16_t>(event.value);
            if(unit>=0xd800&&unit<=0xdbff){m_highSurrogate=unit;continue;}
            uint32_t cp=unit;
            if(unit>=0xdc00&&unit<=0xdfff){if(!m_highSurrogate)continue;cp=0x10000+((m_highSurrogate-0xd800)<<10)+(unit-0xdc00);}
            m_highSurrogate=0;m_ui.Character(cp);m_presentedFrameValid=false;
        }
        if(m_ui.closeRequested){
            CloseWithReason(event.kind==Event::Kind::Key&&event.value==VK_ESCAPE?
                CloseReason::Escape:CloseReason::CloseButton,"menu control");break;
        }
    }
    SyncFavorites(m_ui); // one verified preference write after processing this frame's clicks
    if(!IsOpen())return;
    PrioritizeVisibleSearch(m_ui);
    if(auto command=m_ui.TakeCommand())Submit(*command);
    m_currentTab.store(static_cast<int>(m_ui.tab));
    const bool pointerMoved=!m_presentedFrameValid||std::abs(mx-m_lastRenderMouseX)>.35f||std::abs(my-m_lastRenderMouseY)>.35f;
    if(!m_presentedFrameValid||m_presentedFrameGeneration!=generation||pointerMoved||m_ui.indexing){
        m_presentedFrame=m_ui.Render(mx,my);
        m_presentedFrameGeneration=generation;
        m_lastRenderMouseX=mx;m_lastRenderMouseY=my;
        m_presentedFrameValid=true;
    }
    const auto& frame=m_presentedFrame;
    // Capture only icon paths requested by the current painted frame. The
    // cache is shared by every Helpy tab for this game session, bounded, and
    // populated on the game thread without a catalogue-wide startup scan.
    for(const auto& draw:frame.draws)if(draw.kind==QuickUI::Draw::Kind::Icon&&!draw.text.empty()
        &&!m_canvasIcons.contains(draw.text)&&!m_failedCanvasIcons.contains(draw.text)
        &&m_queuedCanvasIcons.insert(draw.text).second)m_canvasIconQueue.push_back(draw.text);
    // Asset resolution can hitch the game thread. Visible icons are still
    // prioritised, but fill in over a few frames instead of front-loading a
    // dozen reflected loads into the first interactive frame.
    constexpr std::size_t IconCacheLimit=256,FirstFrameIconBudget=6,SteadyIconBudget=2;
    std::size_t iconBudget=m_canvasIcons.empty()?FirstFrameIconBudget:SteadyIconBudget;
    UClass* textureType=nullptr;
    while(iconBudget--&&!m_canvasIconQueue.empty()) {
        auto path=std::move(m_canvasIconQueue.front());m_canvasIconQueue.pop_front();m_queuedCanvasIcons.erase(path);
        UObject* texture=nullptr;
        try {texture=ActorHelper::ResolveObject(RC::to_generic_string(path));}catch(...) {texture=nullptr;}
        if(!texture)for(const auto* fallback:QuickDecorations::FallbackTextures(path)) {
            if(!fallback||!*fallback)continue;
            try {texture=ActorHelper::ResolveObject(RC::to_generic_string(fallback));}catch(...) {texture=nullptr;}
            if(texture)break;
        }
        if(!textureType)try {textureType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Texture"));}catch(...) {textureType=nullptr;}
        if(!texture||!textureType||!texture->IsA(textureType)) {
            if(m_failedCanvasIcons.insert(path).second)
                PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy item icon unavailable: {}\n"),RC::to_generic_string(path));
            continue;
        }
        auto& weak=m_canvasIcons[path];weak.Assign(texture);m_canvasIconOrder.push_back(path);
        while(m_canvasIcons.size()>IconCacheLimit&&!m_canvasIconOrder.empty()) {
            const auto oldest=std::move(m_canvasIconOrder.front());m_canvasIconOrder.pop_front();m_canvasIcons.erase(oldest);
        }
    }
    for(const auto& draw:frame.draws) {
        const auto& r=draw.box;const auto& c=draw.color;
        if(draw.kind==QuickUI::Draw::Kind::Rectangle)DrawRect(canvas,x+r.x*factor,y+r.y*factor,r.w*factor,r.h*factor,c[0],c[1],c[2],c[3]);
        else if(draw.kind==QuickUI::Draw::Kind::Text)DrawText(canvas,draw.text,x+r.x*factor,y+r.y*factor,(draw.font/24.f)*factor*QuickUI::FontScale,c[0],c[1],c[2],c[3],draw.centreX);
        else{
            bool drawn=false;
            try {
                // Badge and navigation chrome are always drawn from Canvas
                // primitives. Only live catalogue artwork performs an object
                // lookup, so Helpy has no cooked menu-asset dependency.
                if(draw.kind==QuickUI::Draw::Kind::Icon&&!draw.text.empty()) {
                    auto found=m_canvasIcons.find(draw.text);
                    auto* texture=found==m_canvasIcons.end()?nullptr:found->second.Get();
                    if(texture&&!texture->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))) {
                        DrawTexture(canvas,texture,x+r.x*factor,y+r.y*factor,r.w*factor,r.h*factor,c[0],c[1],c[2],c[3]);drawn=true;
                    }
                }
            }catch(...){if(draw.kind==QuickUI::Draw::Kind::Icon&&!draw.text.empty())m_failedCanvasIcons.insert(draw.text);}
            if(!drawn) {
                const auto label=draw.kind==QuickUI::Draw::Kind::Badge?draw.fallback:std::string("?");
                DrawRect(canvas,x+r.x*factor,y+r.y*factor,r.w*factor,r.h*factor,.10f,.09f,.07f,.85f*c[3]);
                DrawText(canvas,label,x+(r.x+r.w/2)*factor,y+(r.y+4)*factor,
                    (label.size()>2?.43f:.62f)*factor*QuickUI::FontScale,.95f,.87f,.67f,c[3],true);
            }
        }
    }

}
} // namespace PS
