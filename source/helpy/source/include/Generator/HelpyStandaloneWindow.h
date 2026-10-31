#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <filesystem>
#include <array>
#include <windows.h>
#include "Core/HelpyTrayIcon.h"
#include "Core/HelpyGameOverlay.h"
#include "Core/HelpyCompositionSurface.h"
#include "Runtime/HelpyNativeOverlayClient.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

namespace PS {
class HelpyStandaloneWindow {
public:
    explicit HelpyStandaloneWindow(HINSTANCE module):m_module(module){}
    ~HelpyStandaloneWindow(){Shutdown();}
    bool Start();
    void Toggle();
    void PollHotkey(bool down);
    void ReloadTheme();
    void Shutdown();
    bool Ready() const{return m_ready.load();}
    bool TrayReady()const{return m_tray.Ready();}
    bool WindowVisible()const{return m_visible.load();}
    std::string Error() const;
#if defined(HELPY_PREVIEW)
    int Preview(const std::filesystem::path& output,bool capture,int width=1280,int height=900);
#endif
private:
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    void ThreadMain();
    void RenderFrame();
    bool CreateDevice(const LUID* adapter=nullptr);
    void RebindNativeDevice(const LUID& adapter);
    void DestroyDevice();
    void CreateTarget();
    void DestroyTarget();
    void SetError(std::string value);
    bool OpenPopup(bool desktop=false);
    void SyncGameOverlay(bool opening=false);
    void ClosePopup();
    bool SyncItemBay();
    void DestroyItemBay();
    bool SyncIconCache();
    void DestroyIconCache();
    bool OwnsWindow(HWND window)const;
    bool SyncWorkspacePanel(size_t index);
    void DestroyWorkspacePanel(size_t index);

    HelpyTrayIcon::Icon m_tray;
    HelpyGameOverlay m_gameOverlay;
    HelpyCompositionSurface m_composition;
    HelpyNativeOverlayClient m_nativeOverlay;
    std::chrono::steady_clock::time_point m_nextOverlayGame{};
    HINSTANCE m_module{};HWND m_hwnd{},m_gameWindow{};std::atomic<DWORD> m_threadId{0};
    std::atomic<HWND> m_publishedWindow=nullptr;
    std::atomic<HWND> m_publishedBayWindow=nullptr;
    std::atomic<HWND> m_publishedCacheWindow=nullptr;
    ID3D11Device* m_device{};ID3D11DeviceContext* m_context{};IDXGISwapChain* m_swapChain{};ID3D11RenderTargetView* m_target{};
    HWND m_bayWindow{};IDXGISwapChain* m_baySwapChain{};ID3D11RenderTargetView* m_bayTarget{};
    HWND m_cacheWindow{};IDXGISwapChain* m_cacheSwapChain{};ID3D11RenderTargetView* m_cacheTarget{};
    struct WorkspaceSurface {HWND Window{};IDXGISwapChain* Swap{};ID3D11RenderTargetView* Target{};};
    std::array<WorkspaceSurface,5> m_workspaces{};
    std::array<std::atomic<HWND>,5> m_publishedWorkspaces{};
    std::thread m_thread;std::atomic<bool> m_ready=false,m_visible=false,m_stop=false,m_started=false;
    mutable std::mutex m_errorMutex;std::string m_error;
};
}
