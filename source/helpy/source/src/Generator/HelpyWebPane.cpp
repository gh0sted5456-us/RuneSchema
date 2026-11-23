#include <unknwn.h> // Before C++/WinRT, so it can query classic COM interfaces.
#include "Generator/HelpyWebPane.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Generator/HelpyTextInput.h"
#include "Runtime/HelpyBrowser.h"
#include "Core/HelpyBrowserAddress.h"
#include "Core/HelpyWebOrigin.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Runtime/HostServices.h"
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace PS::HelpyWebPane {
namespace {
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
constexpr wchar_t ClassName[] = L"RuneSchema.Helpy.WebPane.v1";
constexpr size_t MaxPanes = 4;
constexpr LONG MinPixels = 64, MaxPixels = 4096;
// The browser's slot among the pages; plugin routes never contain control characters.
const std::string BrowserRoute = "\x01Helpy.browser";
namespace WGC = winrt::Windows::Graphics::Capture;
namespace WD3D = winrt::Windows::Graphics::DirectX::Direct3D11;

std::wstring Wide(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
std::string Utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
bool Text(const char* text, size_t limit) {
    if (!text) return false;
    size_t n = 0; for (; n <= limit && text[n]; ++n) if (static_cast<unsigned char>(text[n]) < 32) return false;
    return n > 0 && n <= limit;
}

// Texture mode: Windows Graphics Capture of the off-screen page window into a texture on Helpy's device.
struct Shot {
    ID3D11Device* Device{};
    WGC::GraphicsCaptureItem Item{nullptr};
    WGC::Direct3D11CaptureFramePool Pool{nullptr};
    WGC::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::SizeInt32 Size{};
    ComPtr<ID3D11Texture2D> Texture;
    ComPtr<ID3D11ShaderResourceView> View;
    UINT Width = 0, Height = 0;
    bool Failed = false;
    ~Shot() { try { if (Session) Session.Close(); if (Pool) Pool.Close(); } catch (...) {} }
};
struct Pane {
    std::string Route;
    std::wstring Origin, Home, Current, Pending;
    std::string Title;
    uint64_t NavigateId = 0, LastUsed = 0;
    HWND Window{};
    ComPtr<ICoreWebView2Controller> Controller;
    ComPtr<ICoreWebView2> Page;
    bool Starting = false, Loading = false, Ready = false, Failed = false, Showing = false;
    RECT Placed{};
    // Texture mode.
    std::unique_ptr<Shot> Capture;
    HWND Input{};
    std::array<bool, 3> Buttons{};
    bool Hovered = false, KeyFocus = false;
    POINT Mouse{LONG_MIN, LONG_MIN};
    // Helpy's browser: any web address, its own profile, an address bar and an address bar.
    bool Browser = false;
    std::array<char, HelpyBrowserAddress::MaxLength + 1> Address{};
    std::wstring AddressShown;
    bool Editing = false;
};
struct Environment {
    ComPtr<ICoreWebView2Environment> Value;
    bool Starting = false, Failed = false, Extensions = false;
};
struct State {
    HWND Owner{};
    HINSTANCE Module{};
    Mode Showing = Mode::Hidden;
    ID3D11Device* Device{};
    bool Registered = false;
    Environment Pages, Browser; // Plugin pages and Helpy's browser keep separate profiles.
    BrowserSettings Settings;
    std::wstring Applied;      // The ad blocker folder loaded into the browser profile ("" = none).
    bool AppliedOnce = false, ExtensionBusy = false;
    std::optional<std::wstring> FailedExtension;
    uint64_t BrowserGeneration = 0;
    std::string ExtensionStatus;
    std::map<std::string, std::shared_ptr<Pane>> Panes;
    std::string Requested;
    RECT Bounds{};       // Window mode: the workspace in Helpy's client coordinates.
    SIZE Pixels{};       // Texture mode: the page size in pixels.
    uint64_t Frame = 0;
    WD3D::IDirect3DDevice CaptureDevice{nullptr};
    ID3D11Device* CaptureDeviceFor{};
};
thread_local State state;
std::atomic<HWND> Visible{nullptr};
// Window mode: where the shown page sits relative to Helpy's client origin, so it follows a drag at once.
std::atomic<LONG> OffsetX{0}, OffsetY{0};

void OpenExternal(std::wstring_view url) {
    if (HelpyWebOrigin::External(url)) ShellExecuteW(state.Owner, L"open", std::wstring(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
Environment& EnvironmentFor(const Pane& pane) { return pane.Browser ? state.Browser : state.Pages; }
bool Allowed(const Pane& pane, const std::wstring& url) {
    return pane.Browser ? HelpyBrowserAddress::Web(Utf8(url)) : HelpyWebOrigin::Inside(std::wstring_view(url), std::wstring_view(pane.Origin));
}
void Go(Pane& pane, const std::wstring& url) {
    if (!Allowed(pane, url)) return;
    if (!pane.Page) { pane.Pending = url; return; }
    pane.Loading = true;
    if (FAILED(pane.Page->Navigate(url.c_str()))) { pane.Loading = false; pane.Failed = true; }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* pane = reinterpret_cast<Pane*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE:
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        break;
    case WM_SIZE:
        if (pane && pane->Controller) { RECT client{}; GetClientRect(window, &client); pane->Controller->put_Bounds(client); }
        return 0;
    case WM_SETFOCUS:
        if (pane && pane->Controller) pane->Controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;
    case WM_ERASEBKGND: {
        RECT client{}; GetClientRect(window, &client);
        const auto brush = CreateSolidBrush(RGB(22, 20, 16));
        FillRect(reinterpret_cast<HDC>(wParam), &client, brush); DeleteObject(brush);
        return 1;
    }
    case WM_CLOSE:
        return 0; // Helpy closes pages; Alt+F4 here must not destroy one behind its back.
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// The ad blocker in the browser profile: whatever is installed there is removed, then Helpy's own
// download is loaded (nothing when ad blocking is off). Runs again whenever the setting changes.
bool CurrentBrowser(const std::weak_ptr<Pane>& weak,uint64_t generation) {
    const auto pane=weak.lock();
    return state.Settings.Enabled&&generation==state.BrowserGeneration&&pane&&pane->Page;
}
void FinishExtension(const std::weak_ptr<Pane>& weak,uint64_t generation,const std::wstring& wanted,bool ok,bool changed=true) {
    state.ExtensionBusy=false;
    if (!CurrentBrowser(weak,generation)) return;
    if (ok) {
        state.Applied=wanted;state.AppliedOnce=true;state.FailedExtension.reset();
        state.ExtensionStatus=wanted.empty()?"":"Ads are blocked in the browser.";
        if (!wanted.empty()) HelpyBrowser::Activated(std::filesystem::path(wanted));
        if (const auto pane=weak.lock();changed&&pane&&pane->Page) pane->Page->Reload();
    } else {
        state.AppliedOnce=false;state.FailedExtension=wanted;
        state.ExtensionStatus=wanted.empty()?"Helpy couldn't remove the ad blocker. Toggle blocking on and off to retry.":"The ad blocker couldn't be loaded.";
        if (!wanted.empty()) HelpyBrowser::Rejected(std::filesystem::path(wanted),state.ExtensionStatus);
    }
}
void SyncExtension(const std::shared_ptr<Pane>& pane) {
    if (!pane->Browser||!pane->Page||state.ExtensionBusy) return;
    const auto wanted = state.Settings.Extension.wstring();
    if ((state.AppliedOnce&&wanted==state.Applied)||(state.FailedExtension&&*state.FailedExtension==wanted)) return;
    state.FailedExtension.reset();
    const auto generation=state.BrowserGeneration;
    const std::weak_ptr<Pane> weak=pane;
    ComPtr<ICoreWebView2_13> withProfile;ComPtr<ICoreWebView2Profile> profile;ComPtr<ICoreWebView2Profile7> extensions;
    if (!state.Browser.Extensions||FAILED(pane->Page.As(&withProfile))||FAILED(withProfile->get_Profile(&profile))||!profile||FAILED(profile.As(&extensions))) {
        FinishExtension(weak,generation,wanted,false,false);
        state.ExtensionStatus="This WebView2 Runtime can't manage the ad blocker. Update Microsoft Edge WebView2, then restart Helpy.";
        return;
    }
    state.ExtensionBusy=true;
    state.ExtensionStatus=wanted.empty()?"Turning the ad blocker off...":"Loading the ad blocker...";
    const auto add=[weak,generation,extensions,wanted](bool removedAny) {
        if (!CurrentBrowser(weak,generation)) { FinishExtension(weak,generation,wanted,false,false);return; }
        if (wanted.empty()) { FinishExtension(weak,generation,wanted,true,removedAny);return; }
        if (FAILED(extensions->AddBrowserExtension(wanted.c_str(),Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
            [weak,generation,wanted](HRESULT status,ICoreWebView2BrowserExtension*) -> HRESULT {
                FinishExtension(weak,generation,wanted,SUCCEEDED(status));return S_OK;
            }).Get()))) FinishExtension(weak,generation,wanted,false);
    };
    if (FAILED(extensions->GetBrowserExtensions(Callback<ICoreWebView2ProfileGetBrowserExtensionsCompletedHandler>(
        [weak,generation,wanted,add](HRESULT status,ICoreWebView2BrowserExtensionList* list) -> HRESULT {
            if (!CurrentBrowser(weak,generation)) { FinishExtension(weak,generation,wanted,false,false);return S_OK; }
            UINT count=0;
            if (FAILED(status)||!list||FAILED(list->get_Count(&count))||count>64) {
                FinishExtension(weak,generation,wanted,false,false);return S_OK;
            }
            std::vector<ComPtr<ICoreWebView2BrowserExtension>> installed;
            for (UINT i=0;i<count;++i) {
                ComPtr<ICoreWebView2BrowserExtension> item;
                if (FAILED(list->GetValueAtIndex(i,&item))||!item) { FinishExtension(weak,generation,wanted,false,false);return S_OK; }
                installed.push_back(item);
            }
            if (installed.empty()) { add(false);return S_OK; }
            const auto removals=std::make_shared<std::pair<size_t,bool>>(installed.size(),true);
            const auto removed=[weak,generation,wanted,removals,add](HRESULT result) {
                removals->second=removals->second&&SUCCEEDED(result);
                if (--removals->first) return;
                if (removals->second) add(true);
                else FinishExtension(weak,generation,wanted,false,false);
            };
            for (const auto& item:installed) {
                const auto result=item->Remove(Callback<ICoreWebView2BrowserExtensionRemoveCompletedHandler>(
                    [removed](HRESULT result) -> HRESULT { removed(result);return S_OK; }).Get());
                if (FAILED(result)) removed(result);
            }
            return S_OK;
        }).Get()))) FinishExtension(weak,generation,wanted,false,false);
}

void Configure(const std::shared_ptr<Pane>& pane) {
    auto* page = pane->Page.Get();
    const std::weak_ptr<Pane> weak = pane;
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(page->get_Settings(&settings))) {
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_IsWebMessageEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE); // A separate window: never captured in the game overlay.
    }
    ComPtr<ICoreWebView2Settings4> forms;
    if (pane->Browser && settings && SUCCEEDED(settings.As(&forms))) { forms->put_IsPasswordAutosaveEnabled(FALSE); forms->put_IsGeneralAutofillEnabled(FALSE); }
    EventRegistrationToken token{};
    page->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            const auto pane = weak.lock(); if (!pane) return S_OK;
            LPWSTR text = nullptr;
            if (FAILED(args->get_Uri(&text)) || !text) { args->put_Cancel(TRUE); return S_OK; }
            const std::wstring uri(text); CoTaskMemFree(text);
            if (Allowed(*pane, uri) || (pane->Browser && uri == L"about:blank")) { pane->Loading = true; return S_OK; }
            args->put_Cancel(TRUE);
            if (pane->Browser) return S_OK; // Only web addresses: no file:, javascript: or app links.
            BOOL user = FALSE;
            if (SUCCEEDED(args->get_IsUserInitiated(&user)) && user) OpenExternal(uri);
            return S_OK;
        }).Get(), &token);
    page->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [weak](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            const auto pane = weak.lock(); if (!pane) return S_OK;
            BOOL ok = FALSE; args->get_IsSuccess(&ok);
            COREWEBVIEW2_WEB_ERROR_STATUS error{}; args->get_WebErrorStatus(&error);
            if (!ok && error == COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) return S_OK;
            pane->Loading = false; pane->Ready = ok == TRUE; pane->Failed = ok != TRUE;
            LPWSTR source = nullptr;
            if (SUCCEEDED(sender->get_Source(&source)) && source) { pane->Current = source; CoTaskMemFree(source); }
            return S_OK;
        }).Get(), &token);
    page->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
        [weak](ICoreWebView2* sender, IUnknown*) -> HRESULT {
            const auto pane = weak.lock(); if (!pane) return S_OK;
            LPWSTR title = nullptr;
            if (SUCCEEDED(sender->get_DocumentTitle(&title)) && title) { pane->Title = Utf8(title).substr(0, 160); CoTaskMemFree(title); }
            return S_OK;
        }).Get(), &token);
    page->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
        [weak](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
            args->put_Handled(TRUE);
            const auto pane = weak.lock(); if (!pane) return S_OK;
            BOOL user = FALSE; args->get_IsUserInitiated(&user);
            if (!user) return S_OK;
            LPWSTR text = nullptr; if (FAILED(args->get_Uri(&text)) || !text) return S_OK;
            const std::wstring uri(text); CoTaskMemFree(text);
            if (Allowed(*pane, uri)) Go(*pane, uri); // The browser opens links in its own page; popups need a click.
            else if (!pane->Browser) OpenExternal(uri);
            return S_OK;
        }).Get(), &token);
    page->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
        [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
            return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
        }).Get(), &token);
    ComPtr<ICoreWebView2_4> downloads;
    if (SUCCEEDED(pane->Page.As(&downloads))) downloads->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
        [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT { return args->put_Cancel(TRUE); }).Get(), &token);
    page->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
        [weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT {
            if (const auto pane = weak.lock()) { pane->Failed = true; pane->Loading = false; pane->Ready = false; }
            return S_OK;
        }).Get(), &token);
    pane->Controller->add_MoveFocusRequested(Callback<ICoreWebView2MoveFocusRequestedEventHandler>(
        [](ICoreWebView2Controller*, ICoreWebView2MoveFocusRequestedEventArgs* args) -> HRESULT {
            if (state.Owner) SetForegroundWindow(state.Owner);
            return args->put_Handled(TRUE);
        }).Get(), &token);
    Go(*pane, pane->Pending.empty() ? pane->Home : pane->Pending);
    pane->Pending.clear();
    if (pane->Browser) SyncExtension(pane);
}

void CreateController(const std::shared_ptr<Pane>& pane) {
    auto& environment = EnvironmentFor(*pane);
    if (!environment.Value || !pane->Window || pane->Controller || pane->Starting) return;
    pane->Starting = true;
    const std::weak_ptr<Pane> weak = pane;
    if (FAILED(environment.Value->CreateCoreWebView2Controller(pane->Window,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [weak](HRESULT status, ICoreWebView2Controller* controller) -> HRESULT {
                const auto pane = weak.lock();
                if (!pane || !pane->Window) { if (controller) controller->Close(); return S_OK; }
                pane->Starting = false;
                if (FAILED(status) || !controller) { pane->Failed = true; return S_OK; }
                pane->Controller = controller;
                ComPtr<ICoreWebView2Controller2> color;
                if (SUCCEEDED(pane->Controller.As(&color))) color->put_DefaultBackgroundColor({255, 22, 20, 16});
                RECT client{}; GetClientRect(pane->Window, &client); controller->put_Bounds(client);
                controller->put_IsVisible(pane->Showing);
                if (FAILED(controller->get_CoreWebView2(&pane->Page))) { pane->Failed = true; return S_OK; }
                Configure(pane);
                return S_OK;
            }).Get()))) { pane->Starting = false; pane->Failed = true; }
}

void StartEnvironment(bool browser) {
    auto& target = browser ? state.Browser : state.Pages;
    if (target.Value || target.Starting || target.Failed) return;
    target.Starting = true;
    try {
        const auto profile = HostServices::StateDirectory() / (browser ? L"web-browser" : L"plugin-browser");
        RuneSchemaData::Safe(profile);
        std::filesystem::create_directories(profile);
        // Keep painting while hidden behind the game or off-screen (Texture mode captures it there).
        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        if (options) options->put_AdditionalBrowserArguments(L"--disable-features=CalculateNativeWinOcclusion --disable-backgrounding-occluded-windows");
        // The browser can load the ad blocker; it is only ever loaded from Helpy's own download.
        ComPtr<ICoreWebView2EnvironmentOptions6> extensions;
        target.Extensions = browser && options && SUCCEEDED(options.As(&extensions)) && SUCCEEDED(extensions->put_AreBrowserExtensionsEnabled(TRUE));
        const auto status = CreateCoreWebView2EnvironmentWithOptions(nullptr, profile.c_str(), options.Get(),
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [browser](HRESULT status, ICoreWebView2Environment* environment) -> HRESULT {
                    auto& target = browser ? state.Browser : state.Pages;
                    target.Starting = false;
                    if (FAILED(status) || !environment) { target.Failed = true; return S_OK; }
                    target.Value = environment;
                    for (auto& [_, pane] : state.Panes) if (pane->Window && pane->Browser == browser) CreateController(pane);
                    return S_OK;
                }).Get());
        if (FAILED(status)) { target.Starting = false; target.Failed = true; }
    } catch (...) { target.Starting = false; target.Failed = true; }
}

bool EnsureWindow(const std::shared_ptr<Pane>& pane) {
    if (pane->Window) return true;
    if (!state.Registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = WindowProc; wc.hInstance = state.Module; wc.lpszClassName = ClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        state.Registered = true;
    }
    // Owned (not a child): Helpy's overlay window is layered, which a child web view cannot draw into.
    pane->Window = CreateWindowExW(WS_EX_TOOLWINDOW, ClassName, L"Helpy web page", WS_POPUP | WS_CLIPCHILDREN,
        0, 0, 1, 1, state.Owner, nullptr, state.Module, pane.get());
    return pane->Window != nullptr;
}

void Destroy(Pane& pane) {
    if (pane.Browser) { ++state.BrowserGeneration;state.AppliedOnce=false;state.FailedExtension.reset(); }
    pane.Capture.reset(); pane.Input = nullptr;
    if (pane.Controller) pane.Controller->Close();
    pane.Controller.Reset(); pane.Page.Reset();
    if (pane.Window) { if (Visible.load() == pane.Window) Visible.store(nullptr); DestroyWindow(pane.Window); pane.Window = nullptr; }
    pane.Starting = pane.Loading = pane.Ready = pane.Showing = false;
}

void Hide(Pane& pane) {
    pane.Capture.reset(); pane.KeyFocus = false; pane.Hovered = false; pane.Buttons = {};
    if (!pane.Showing) return;
    pane.Showing = false;
    if (pane.Controller) pane.Controller->put_IsVisible(FALSE);
    if (pane.Window) ShowWindow(pane.Window, SW_HIDE);
}

void Show(const std::shared_ptr<Pane>& pane, RECT screen, HWND order) {
    if (!EnsureWindow(pane)) { pane->Failed = true; return; }
    StartEnvironment(pane->Browser);
    CreateController(pane);
    const bool moved = !EqualRect(&screen, &pane->Placed);
    if (moved || !pane->Showing) {
        SetWindowPos(pane->Window, order, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
        pane->Placed = screen;
    }
    if (!pane->Showing && pane->Controller) pane->Controller->put_IsVisible(TRUE);
    pane->Showing = true;
}

// Texture mode keeps the page window just past the right edge of the virtual screen.
RECT Offscreen(SIZE size) {
    const LONG x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) + 256;
    const LONG y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    return {x, y, x + size.cx, y + size.cy};
}
bool CaptureSupported() {
    static const bool supported = [] { try { return WGC::GraphicsCaptureSession::IsSupported(); } catch (...) { return false; } }();
    return supported;
}
WD3D::IDirect3DDevice CaptureDevice(ID3D11Device* device) {
    if (state.CaptureDevice && state.CaptureDeviceFor == device) return state.CaptureDevice;
    state.CaptureDevice = nullptr; state.CaptureDeviceFor = nullptr;
    ComPtr<IDXGIDevice> dxgi;
    if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))) return nullptr;
    winrt::com_ptr<::IInspectable> inspectable;
    if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), inspectable.put()))) return nullptr;
    state.CaptureDevice = inspectable.as<WD3D::IDirect3DDevice>(); state.CaptureDeviceFor = device;
    return state.CaptureDevice;
}
void StartCapture(Pane& pane) {
    if (pane.Capture && pane.Capture->Device == state.Device) return;
    pane.Capture = std::make_unique<Shot>();
    auto& shot = *pane.Capture;
    shot.Device = state.Device;
    try {
        const auto device = CaptureDevice(state.Device);
        if (!device || !pane.Window) throw std::runtime_error("No capture device.");
        const auto factory = winrt::get_activation_factory<WGC::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(factory->CreateForWindow(pane.Window, winrt::guid_of<WGC::GraphicsCaptureItem>(), winrt::put_abi(shot.Item)));
        shot.Size = shot.Item.Size();
        shot.Pool = WGC::Direct3D11CaptureFramePool::CreateFreeThreaded(device, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, shot.Size);
        shot.Session = shot.Pool.CreateCaptureSession(shot.Item);
        try { shot.Session.IsCursorCaptureEnabled(false); } catch (...) {}
        try { shot.Session.IsBorderRequired(false); } catch (...) {} // Off-screen anyway; older Windows ignores it.
        shot.Session.StartCapture();
    } catch (...) { shot.Failed = true; }
}
// The newest captured frame, copied into the texture Helpy draws.
void PollCapture(Pane& pane) {
    if (!pane.Capture || pane.Capture->Failed || !pane.Capture->Pool) return;
    auto& shot = *pane.Capture;
    try {
        WGC::Direct3D11CaptureFrame latest{nullptr};
        while (auto frame = shot.Pool.TryGetNextFrame()) latest = frame;
        if (!latest) return;
        const auto size = latest.ContentSize();
        auto access = latest.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> source;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&source)));
        D3D11_TEXTURE2D_DESC from{}; source->GetDesc(&from);
        const UINT width = std::min<UINT>(std::max(1, size.Width), from.Width), height = std::min<UINT>(std::max(1, size.Height), from.Height);
        if (!shot.Texture || shot.Width != width || shot.Height != height) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            shot.Texture.Reset(); shot.View.Reset();
            winrt::check_hresult(shot.Device->CreateTexture2D(&desc, nullptr, &shot.Texture));
            winrt::check_hresult(shot.Device->CreateShaderResourceView(shot.Texture.Get(), nullptr, &shot.View));
            shot.Width = width; shot.Height = height;
        }
        ComPtr<ID3D11DeviceContext> context; shot.Device->GetImmediateContext(&context);
        const D3D11_BOX box{0, 0, 0, width, height, 1};
        context->CopySubresourceRegion(shot.Texture.Get(), 0, 0, 0, 0, source.Get(), 0, &box);
        if (size.Width != shot.Size.Width || size.Height != shot.Size.Height) {
            shot.Size = size;
            shot.Pool.Recreate(CaptureDevice(shot.Device), winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
        }
    } catch (...) { shot.Failed = true; }
}
// The page's own input window (Chromium's render widget), found once per page window.
HWND InputWindow(Pane& pane) {
    if (pane.Input && IsWindow(pane.Input)) return pane.Input;
    pane.Input = nullptr;
    if (!pane.Window) return nullptr;
    EnumChildWindows(pane.Window, [](HWND child, LPARAM found) -> BOOL {
        wchar_t name[64]{}; GetClassNameW(child, name, 64);
        if (wcscmp(name, L"Chrome_RenderWidgetHostHWND") == 0) { *reinterpret_cast<HWND*>(found) = child; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&pane.Input));
    return pane.Input;
}
// Texture mode: the mouse, wheel and keys over the drawn page go to the page window.
void Forward(Pane& pane, ImVec2 at, ImVec2 size, bool hovered) {
    const auto target = InputWindow(pane);
    if (!target || size.x < 1 || size.y < 1) return;
    auto& io = ImGui::GetIO();
    const LONG width = pane.Placed.right - pane.Placed.left, height = pane.Placed.bottom - pane.Placed.top;
    POINT point{LONG((io.MousePos.x - at.x) * width / size.x), LONG((io.MousePos.y - at.y) * height / size.y)};
    point.x = std::clamp<LONG>(point.x, 0, std::max<LONG>(0, width - 1)); point.y = std::clamp<LONG>(point.y, 0, std::max<LONG>(0, height - 1));
    MapWindowPoints(pane.Window, target, &point, 1);
    WPARAM keys = (io.MouseDown[0] ? MK_LBUTTON : 0) | (io.MouseDown[1] ? MK_RBUTTON : 0) | (io.MouseDown[2] ? MK_MBUTTON : 0)
        | (io.KeyCtrl ? MK_CONTROL : 0) | (io.KeyShift ? MK_SHIFT : 0);
    const LPARAM where = MAKELPARAM(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y));
    const bool tracking = hovered || pane.Buttons[0] || pane.Buttons[1] || pane.Buttons[2];
    if (tracking && (point.x != pane.Mouse.x || point.y != pane.Mouse.y)) { PostMessageW(target, WM_MOUSEMOVE, keys, where); pane.Mouse = point; }
    if (!tracking && pane.Hovered) { PostMessageW(target, WM_MOUSELEAVE, 0, 0); pane.Mouse = {LONG_MIN, LONG_MIN}; }
    pane.Hovered = hovered;
    constexpr std::array<UINT, 3> down{WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN}, up{WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP};
    for (size_t i = 0; i < 3; ++i) {
        const bool held = io.MouseDown[i] && (hovered || pane.Buttons[i]);
        if (held != pane.Buttons[i]) { PostMessageW(target, held ? down[i] : up[i], keys, where); pane.Buttons[i] = held; }
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) pane.KeyFocus = hovered;
    if (hovered && io.MouseWheel != 0.f) {
        POINT screen = point; ClientToScreen(target, &screen);
        PostMessageW(target, WM_MOUSEWHEEL, MAKEWPARAM(keys, static_cast<SHORT>(io.MouseWheel * WHEEL_DELTA)), MAKELPARAM(static_cast<SHORT>(screen.x), static_cast<SHORT>(screen.y)));
    }
    if (!pane.KeyFocus) return;
    // Typing goes to the page while it has focus (the plugin's own search box above always works).
    for (const auto character : io.InputQueueCharacters) {
        if (character <= 0xFFFF) PostMessageW(target, WM_CHAR, character, 1);
        else { const auto code = static_cast<UINT>(character) - 0x10000; PostMessageW(target, WM_CHAR, 0xD800 + (code >> 10), 1); PostMessageW(target, WM_CHAR, 0xDC00 + (code & 0x3FF), 1); }
    }
    constexpr std::pair<ImGuiKey, UINT> special[]{{ImGuiKey_Backspace, VK_BACK}, {ImGuiKey_Delete, VK_DELETE}, {ImGuiKey_Enter, VK_RETURN}, {ImGuiKey_KeypadEnter, VK_RETURN},
        {ImGuiKey_Tab, VK_TAB}, {ImGuiKey_LeftArrow, VK_LEFT}, {ImGuiKey_RightArrow, VK_RIGHT}, {ImGuiKey_UpArrow, VK_UP}, {ImGuiKey_DownArrow, VK_DOWN},
        {ImGuiKey_Home, VK_HOME}, {ImGuiKey_End, VK_END}, {ImGuiKey_PageUp, VK_PRIOR}, {ImGuiKey_PageDown, VK_NEXT}, {ImGuiKey_Escape, VK_ESCAPE}};
    for (const auto& [key, vk] : special) if (ImGui::IsKeyPressed(key, true)) {
        // Enter and Backspace become characters in the page's own message loop.
        PostMessageW(target, WM_KEYDOWN, vk, 1); PostMessageW(target, WM_KEYUP, vk, 0xC0000001);
    }
}

void Fallback(const Pane& pane, const char* reason) {
    ImGui::TextWrapped("%s", reason);
    const auto url = pane.Current.empty() ? pane.Home : pane.Current;
    if (HelpyImGuiTheme::SoftButton("Open in your browser")) OpenExternal(url);
}
}

Frame::Frame(HWND owner, HINSTANCE module, Mode mode, ID3D11Device* device) {
    state.Owner = owner; state.Module = module; state.Device = device;
    state.Showing = mode == Mode::Texture && (!device || !CaptureSupported()) ? Mode::Hidden : mode;
    state.Requested.clear(); state.Bounds = {}; state.Pixels = {}; ++state.Frame;
}

Frame::~Frame() {
    const auto* context=ImGui::GetCurrentContext();
    bool popup=context&&ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    if (context) for (const auto* window:context->Windows)
        if (window->Active&&!window->Hidden&&(window->Flags&ImGuiWindowFlags_Tooltip)) { popup=true;break; }
    const bool ownerShown = state.Owner && IsWindowVisible(state.Owner) && !IsIconic(state.Owner);
    const bool sized = state.Bounds.right > state.Bounds.left + 48 && state.Bounds.bottom > state.Bounds.top + 48;
    HWND shown = nullptr;
    for (auto& [route, pane] : state.Panes) {
        const bool wanted = route == state.Requested && !pane->Failed;
        if (wanted && state.Showing == Mode::Window && ownerShown && sized && !popup) {
            pane->Capture.reset();
            POINT topLeft{state.Bounds.left, state.Bounds.top}, bottomRight{state.Bounds.right, state.Bounds.bottom};
            ClientToScreen(state.Owner, &topLeft); ClientToScreen(state.Owner, &bottomRight);
            POINT origin{0, 0}; ClientToScreen(state.Owner, &origin);
            OffsetX.store(topLeft.x - origin.x); OffsetY.store(topLeft.y - origin.y);
            Show(pane, {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y}, HWND_TOP);
            if (pane->Showing) shown = pane->Window;
        } else if (wanted && state.Showing == Mode::Texture && state.Pixels.cx >= MinPixels && state.Pixels.cy >= MinPixels) {
            // Off-screen and below everything: the game keeps focus; Helpy draws the captured page.
            Show(pane, Offscreen(state.Pixels), HWND_BOTTOM);
            if (pane->Showing && pane->Window) StartCapture(*pane);
        } else Hide(*pane);
        if (pane->Browser) SyncExtension(pane);
    }
    Visible.store(shown);
}

void FollowOwner(HWND owner) {
    const auto shown = Visible.load();
    if (!shown || !owner || !IsWindowVisible(shown)) return;
    POINT origin{0, 0};
    if (!ClientToScreen(owner, &origin)) return;
    SetWindowPos(shown, nullptr, origin.x + OffsetX.load(), origin.y + OffsetY.load(), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

namespace {
// Shared by plugin pages and the browser.
std::optional<int32_t> Unavailable(const Pane& pane) {
    if (state.Showing == Mode::Hidden) {
        Fallback(pane, "This page can't be shown here. Open Helpy, or open this page in your browser.");
        return RS_WEB_UNAVAILABLE;
    }
    if (EnvironmentFor(pane).Failed) {
        Fallback(pane, "Helpy could not start its web view. Install or repair the Microsoft Edge WebView2 Runtime, or open this page in your browser.");
        return RS_WEB_FAILED;
    }
    return std::nullopt;
}
void Buttons(Pane& pane) {
    BOOL canBack = FALSE, canForward = FALSE;
    if (pane.Page) { pane.Page->get_CanGoBack(&canBack); pane.Page->get_CanGoForward(&canForward); }
    ImGui::BeginDisabled(!canBack);
    if (HelpyImGuiTheme::SoftButton("Back") && pane.Page) pane.Page->GoBack();
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!canForward);
    if (HelpyImGuiTheme::SoftButton("Forward") && pane.Page) pane.Page->GoForward();
    ImGui::EndDisabled(); ImGui::SameLine();
    if (HelpyImGuiTheme::SoftButton("Home")) Go(pane, pane.Home);
    ImGui::SameLine();
    if (HelpyImGuiTheme::SoftButton(pane.Failed ? "Retry" : "Reload")) {
        if (pane.Failed) { Destroy(pane); pane.Failed = false; pane.Pending = pane.Current.empty() ? pane.Home : pane.Current; }
        else if (pane.Page) pane.Page->Reload();
    }
}
// The page itself in the rest of the workspace: over Helpy's window, or the captured texture.
int32_t Surface(Pane& pane, const std::string& route) {
    if (pane.Failed) {
        ImGui::TextWrapped("The page could not be shown. Check your connection, then choose Retry.");
        return RS_WEB_FAILED;
    }
    const auto at = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    if (state.Showing == Mode::Texture) {
        const auto scale = ImGui::GetIO().DisplayFramebufferScale;
        state.Pixels = {std::clamp(LONG(size.x * std::max(1.f, scale.x)), 1L, MaxPixels), std::clamp(LONG(size.y * std::max(1.f, scale.y)), 1L, MaxPixels)};
        state.Requested = route;
        if (pane.Capture && pane.Capture->Failed) {
            Fallback(pane, "Windows couldn't show this page in the game. Open Helpy's window, or open this page in your browser.");
            return RS_WEB_FAILED;
        }
        PollCapture(pane);
        if (pane.Capture && pane.Capture->View) {
            ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(pane.Capture->View.Get())), {std::max(1.f, size.x), std::max(1.f, size.y)});
            Forward(pane, at, size, ImGui::IsItemHovered());
        } else {
            ImGui::Dummy({std::max(1.f, size.x), std::max(1.f, size.y)});
            ImGui::GetWindowDrawList()->AddText({at.x + 12, at.y + 12}, ImGui::GetColorU32(ImGuiCol_TextDisabled), "Loading page...");
        }
        return pane.Ready && !pane.Loading ? RS_WEB_READY : RS_WEB_LOADING;
    }
    const auto clipMin = ImGui::GetWindowDrawList()->GetClipRectMin(), clipMax = ImGui::GetWindowDrawList()->GetClipRectMax();
    RECT client{};
    if (state.Owner && GetClientRect(state.Owner, &client)) {
        state.Bounds = {std::max(client.left, LONG(std::max(clipMin.x, at.x))), std::max(client.top, LONG(std::max(clipMin.y, at.y))),
            std::min(client.right, LONG(std::min(clipMax.x, at.x + size.x))), std::min(client.bottom, LONG(std::min(clipMax.y, at.y + size.y)))};
        state.Requested = route;
    }
    ImGui::Dummy({std::max(1.f, size.x), std::max(1.f, size.y)});
    return pane.Ready && !pane.Loading ? RS_WEB_READY : RS_WEB_LOADING;
}
}

int32_t Draw(const std::string& route, const RuneSchemaWebPageV1* page) {
    if (!page || page->StructSize < sizeof(RuneSchemaWebPageV1) || !Text(page->Origin, 128) || !Text(page->Home, 2048)
        || !HelpyWebOrigin::Valid(page->Origin) || !HelpyWebOrigin::Inside(std::string_view(page->Home), std::string_view(page->Origin))) {
        ImGui::TextWrapped("This plugin asked for a web page Helpy cannot open.");
        return RS_WEB_FAILED;
    }
    auto& slot = state.Panes[route];
    if (slot && Utf8(slot->Origin) != page->Origin) { Destroy(*slot); slot.reset(); }
    if (!slot) {
        if (state.Panes.size() > MaxPanes) {
            auto oldest = state.Panes.end();
            for (auto it = state.Panes.begin(); it != state.Panes.end(); ++it)
                if (it->first != route && it->first != BrowserRoute && (oldest == state.Panes.end() || it->second->LastUsed < oldest->second->LastUsed)) oldest = it;
            if (oldest != state.Panes.end()) { if (oldest->second) Destroy(*oldest->second); state.Panes.erase(oldest); }
        }
        auto& created = state.Panes[route];
        created = std::make_shared<Pane>(); created->Route = route; created->Origin = Wide(page->Origin);
        return Draw(route, page);
    }
    auto& pane = *slot;
    pane.Home = Wide(page->Home); pane.LastUsed = state.Frame;
    if (page->NavigateId && page->NavigateId != pane.NavigateId && Text(page->Navigate, 2048)
        && HelpyWebOrigin::Inside(std::string_view(page->Navigate), std::string_view(page->Origin))) {
        pane.NavigateId = page->NavigateId;
        Go(pane, Wide(page->Navigate));
    }
    if (const auto blocked = Unavailable(pane)) return *blocked;
    Buttons(pane);
    ImGui::SameLine();
    if (HelpyImGuiTheme::SoftButton("Open in browser")) OpenExternal(pane.Current.empty() ? pane.Home : pane.Current);
    ImGui::SameLine();
    if (pane.Loading || !pane.Page) ImGui::TextDisabled("Loading...");
    else if (!pane.Title.empty()) ImGui::TextDisabled("%s", pane.Title.c_str());
    return Surface(pane, route);
}

void Browser(const BrowserSettings& settings) {
    if (state.Settings.Enabled!=settings.Enabled) { ++state.BrowserGeneration;state.FailedExtension.reset(); }
    state.Settings = settings;
    if (settings.Enabled) return;
    if (const auto found = state.Panes.find(BrowserRoute); found != state.Panes.end()) {
        if (found->second) Destroy(*found->second);
        state.Panes.erase(found);
    }
    // Next time the browser opens, its profile is brought in line with the ad-block setting again.
    state.AppliedOnce = false; state.ExtensionStatus.clear();
}

int32_t DrawBrowser() {
    if (!state.Settings.Enabled) {
        ImGui::TextWrapped("The web browser is off. Turn it on in Settings, under Web browser.");
        return RS_WEB_UNAVAILABLE;
    }
    auto& slot = state.Panes[BrowserRoute];
    if (!slot) { slot = std::make_shared<Pane>(); slot->Route = BrowserRoute; slot->Browser = true; slot->Home = Wide(HelpyBrowserAddress::Home); }
    auto& pane = *slot;
    pane.LastUsed = state.Frame;
    if (const auto blocked = Unavailable(pane)) return *blocked;
    Buttons(pane);
    ImGui::SameLine();
    // The address bar shows where the page is, unless the player is typing in it.
    const auto& where = pane.Current.empty() ? pane.Home : pane.Current;
    if (!pane.Editing && where != pane.AddressShown) {
        const auto text = Utf8(where).substr(0, HelpyBrowserAddress::MaxLength);
        std::fill(pane.Address.begin(), pane.Address.end(), '\0');
        std::copy(text.begin(), text.end(), pane.Address.begin());
        pane.AddressShown = where;
    }
    ImGui::SetNextItemWidth(std::max(120.f, ImGui::GetContentRegionAvail().x));
    if (HelpyTextInput::InputTextWithHint("##helpy-browser-address", "Search or type a web address", pane.Address.data(), pane.Address.size(),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
        if (const auto url = HelpyBrowserAddress::Resolve(pane.Address.data()); !url.empty()) { Go(pane, Wide(url)); pane.AddressShown = Wide(url); }
        pane.KeyFocus = false;
    }
    pane.Editing = ImGui::IsItemActive();
    if (pane.Editing||HelpyTextInput::Owner) pane.KeyFocus=false;
    if (pane.Loading || !pane.Page) ImGui::TextDisabled("Loading...");
    else ImGui::TextDisabled("%s", pane.Title.empty() ? " " : pane.Title.c_str());
    return Surface(pane, BrowserRoute);
}

std::string BrowserExtensionStatus() { return state.Settings.Enabled ? state.ExtensionStatus : std::string{}; }
bool BrowserExtensionFailed() { return state.FailedExtension.has_value(); }
void RetryBrowserExtension() { if (!state.ExtensionBusy) { state.FailedExtension.reset();state.AppliedOnce=false; } }

bool Owns(HWND window) {
    const auto shown = Visible.load();
    return shown && window && (window == shown || GetAncestor(window, GA_ROOT) == shown);
}

void Suspend() {
    for (auto& [_,pane]:state.Panes) if (pane) Hide(*pane);
    Visible.store(nullptr);
}

void Close() {
    for (auto& [_, pane] : state.Panes) if (pane) Destroy(*pane);
    state.Panes.clear();
    state.CaptureDevice = nullptr; state.CaptureDeviceFor = nullptr;
    state.Pages = {}; state.Browser = {};
    ++state.BrowserGeneration;state.FailedExtension.reset();
    state.Applied.clear(); state.AppliedOnce = false; state.ExtensionStatus.clear();
    Visible.store(nullptr);
}
}
