#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <windows.h>

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
    void ReloadTheme();
    void Shutdown();
    bool Ready() const{return m_ready.load();}
    std::string Error() const;
private:
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    void ThreadMain();
    void RenderFrame();
    bool CreateDevice();
    void DestroyDevice();
    void CreateTarget();
    void DestroyTarget();
    void SetError(std::string value);

    HINSTANCE m_module{};HWND m_hwnd{},m_gameWindow{};std::atomic<DWORD> m_threadId{0};
    ID3D11Device* m_device{};ID3D11DeviceContext* m_context{};IDXGISwapChain* m_swapChain{};ID3D11RenderTargetView* m_target{};
    std::thread m_thread;std::atomic<bool> m_ready=false,m_visible=false,m_stop=false,m_started=false;
    mutable std::mutex m_errorMutex;std::string m_error;
};
}
