#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>
#include <safetyhook.hpp>
#include "Unreal/Hooks.hpp"
#include "SDK/WeakObjectHandle.h"
#include "Generator/QuickMenuUI.h"
#include "Generator/QuickMenuInputSafety.h"

namespace PS {
class InGameQuickMenu {
public:
    enum class Tab:int { Items=0, Enemies=1, Resources=2 };
    bool Initialize(bool reportFailure=true);
    void Shutdown();
    void Toggle() noexcept;
    void Close() noexcept;
    bool IsOpen() const noexcept {return m_open.load(std::memory_order_acquire);}
    void SelectPrevious() noexcept;
    void SelectNext() noexcept;
    void NotifyToolsChanged() noexcept {m_catalogReadRequested.store(true,std::memory_order_release);}
    void Select(Tab tab) noexcept {m_requestedTab.store(static_cast<int>(tab));}
    Tab SelectedTab() const noexcept {return static_cast<Tab>(m_currentTab.load());}
private:
    enum class CloseReason {
        Requested, F2Toggle, Escape, CloseButton, FocusLost, WindowUnavailable,
        WorldReset, OwnerExpired, OwnerWorldChanged, NativeModeChange,
        InputQueueOverflow, InputRelayError, InputObserverError,
        FirstFrameTimeout, FrameTimeout, RenderFailure, Shutdown
    };
    void CloseWithReason(CloseReason, const char* detail = "") noexcept;
    static const char* CloseReasonText(CloseReason) noexcept;
    struct Event {enum class Kind { Click,RightClick,Wheel,Character,Key } kind; float x=0,y=0; int value=0; bool control=false; uint64_t generation=0;};
    static void PostRenderThunk(RC::Unreal::UObject*,RC::Unreal::UObject*);
    static LRESULT CALLBACK InputThunk(int,WPARAM,LPARAM);
    void EnsureInputHook();
    void RemoveInputHook() noexcept;
    void RenderCanvas(RC::Unreal::UObject*);
    void RenderInputDiagnostic(RC::Unreal::UObject*, RC::Unreal::UObject*);
    void SyncViewportInput(RC::Unreal::UObject*,RC::Unreal::UObject*);
    bool ReleaseViewportInput() noexcept;
    void TickInputSafety();
    void AbortOpen(const char* stage, const char* reason,
                   CloseReason cause = CloseReason::RenderFailure) noexcept;
    bool HasInputLease() const noexcept;
    RC::Unreal::UObject* FindLocalPlayerController(RC::Unreal::UObject*);
    void CaptureControllerInput(RC::Unreal::UObject*);
    bool ReleaseControllerInput() noexcept;
    void ObserveNativeInputMode(RC::Unreal::UFunction*, void*);
    void ResetWorld();
    void ReadCatalog();
    void Submit(const QuickUI::Command&);
    // Requested state is distinct from a successfully submitted Canvas frame.
    std::atomic<bool> m_open{false},m_inputReady{false};
    std::atomic<bool> m_frameSubmitted{false},m_inputDiagnostic{false};
    std::atomic<uint64_t> m_openRequestedAt{0},m_lastFrameAt{0};
    std::atomic<uint64_t> m_completedFrames{0};
    std::atomic<int> m_requestedTab{-1},m_currentTab{0};
    std::atomic<uint64_t> m_generation{1};
    std::atomic<HWND> m_window{nullptr};
    HHOOK m_inputHook=nullptr;
    DWORD m_windowThread=0;
    std::mutex m_eventMutex;
    std::vector<Event> m_events;
    WeakObjectHandle m_lockedViewport,m_lockedController,m_world;
    bool m_seenWorld=false;
    QuickInput::SavedFlag m_cursorFlag;
    bool m_nativeModeOwned=false;
    std::atomic<bool> m_modeObserverActive{false};
    DWORD m_modeThread=0;
    RC::Unreal::UFunction* m_uiOnlyFunction=nullptr;
    RC::Unreal::UFunction* m_gameOnlyFunction=nullptr;
    RC::Unreal::UFunction* m_gameAndUIFunction=nullptr;
    QuickInput::ControllerLocks m_controllerLocks;
    QuickInput::BoolSlot m_cursorSlot;
    bool m_cleanupErrorLogged=false;

    std::atomic<bool> m_renderErrorLogged{false};
    QuickUI::Model m_ui;
    uint64_t m_seenGeneration=0,m_requestSequence=0,m_pendingRequest=0;
    std::array<uint64_t,3> m_catalogRevisions{};
    uint64_t m_lastCatalogReadAt=0;
    int m_lastCatalogCategory=-1;
    std::atomic<bool> m_catalogReadRequested{true};
    bool m_catalogAttempted=false;
    uint16_t m_highSurrogate=0;
    int m_wheelRemainder=0;
    std::unordered_map<std::string,WeakObjectHandle> m_canvasIcons;
    std::unordered_set<std::string> m_failedCanvasIcons;
    RC::Unreal::Hook::GlobalCallbackId m_inputModeHook=RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_worldResetHook=RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId m_inputSafetyHook=RC::Unreal::Hook::ERROR_ID;
    static inline SafetyHookInline s_postRenderHook{};
    static inline std::atomic<InGameQuickMenu*> s_activeMenu{nullptr};
    static inline std::recursive_mutex s_lifetimeMutex;
};
}
