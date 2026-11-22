#include "Generator/HelpyWebPane.h"
#include "Generator/HelpyImGuiTheme.h"
#include "Core/HelpyWebOrigin.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Runtime/HostServices.h"
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>
#include <shellapi.h>
#include <atomic>
#include <map>
#include <memory>

namespace PS::HelpyWebPane {
namespace {
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
constexpr wchar_t ClassName[] = L"RuneSchema.Helpy.WebPane.v1";
constexpr size_t MaxPanes = 4;

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
};
struct State {
    HWND Owner{};
    HINSTANCE Module{};
    bool Allowed = false, Registered = false;
    ComPtr<ICoreWebView2Environment> Environment;
    bool EnvironmentStarting = false, EnvironmentFailed = false;
    std::map<std::string, std::shared_ptr<Pane>> Panes;
    std::string Requested;
    RECT Bounds{};
    uint64_t Frame = 0;
};
thread_local State state;
std::atomic<HWND> Visible{nullptr};

void OpenExternal(std::wstring_view url) {
    if (HelpyWebOrigin::External(url)) ShellExecuteW(state.Owner, L"open", std::wstring(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
void Go(Pane& pane, const std::wstring& url) {
    if (!HelpyWebOrigin::Inside(std::wstring_view(url), std::wstring_view(pane.Origin))) return;
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
    }
    EventRegistrationToken token{};
    page->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            const auto pane = weak.lock(); if (!pane) return S_OK;
            LPWSTR text = nullptr;
            if (FAILED(args->get_Uri(&text)) || !text) { args->put_Cancel(TRUE); return S_OK; }
            const std::wstring uri(text); CoTaskMemFree(text);
            if (HelpyWebOrigin::Inside(std::wstring_view(uri), std::wstring_view(pane->Origin))) { pane->Loading = true; return S_OK; }
            args->put_Cancel(TRUE);
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
            if (HelpyWebOrigin::Inside(std::wstring_view(uri), std::wstring_view(pane->Origin))) Go(*pane, uri);
            else OpenExternal(uri);
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
}

void CreateController(const std::shared_ptr<Pane>& pane) {
    if (!state.Environment || !pane->Window || pane->Controller || pane->Starting) return;
    pane->Starting = true;
    const std::weak_ptr<Pane> weak = pane;
    if (FAILED(state.Environment->CreateCoreWebView2Controller(pane->Window,
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

void StartEnvironment() {
    if (state.Environment || state.EnvironmentStarting || state.EnvironmentFailed) return;
    state.EnvironmentStarting = true;
    try {
        const auto profile = HostServices::StateDirectory() / L"plugin-browser";
        RuneSchemaData::Safe(profile);
        std::filesystem::create_directories(profile);
        const auto status = CreateCoreWebView2EnvironmentWithOptions(nullptr, profile.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT status, ICoreWebView2Environment* environment) -> HRESULT {
                    state.EnvironmentStarting = false;
                    if (FAILED(status) || !environment) { state.EnvironmentFailed = true; return S_OK; }
                    state.Environment = environment;
                    for (auto& [_, pane] : state.Panes) if (pane->Window) CreateController(pane);
                    return S_OK;
                }).Get());
        if (FAILED(status)) { state.EnvironmentStarting = false; state.EnvironmentFailed = true; }
    } catch (...) { state.EnvironmentStarting = false; state.EnvironmentFailed = true; }
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
    if (pane.Controller) pane.Controller->Close();
    pane.Controller.Reset(); pane.Page.Reset();
    if (pane.Window) { if (Visible.load() == pane.Window) Visible.store(nullptr); DestroyWindow(pane.Window); pane.Window = nullptr; }
    pane.Starting = pane.Loading = pane.Ready = pane.Showing = false;
}

void Hide(Pane& pane) {
    if (!pane.Showing) return;
    pane.Showing = false;
    if (pane.Controller) pane.Controller->put_IsVisible(FALSE);
    if (pane.Window) ShowWindow(pane.Window, SW_HIDE);
}

void Show(const std::shared_ptr<Pane>& pane, RECT screen) {
    if (!EnsureWindow(pane)) { pane->Failed = true; return; }
    StartEnvironment();
    CreateController(pane);
    const bool moved = !EqualRect(&screen, &pane->Placed);
    if (moved || !pane->Showing) {
        SetWindowPos(pane->Window, HWND_TOP, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        pane->Placed = screen;
    }
    if (!pane->Showing && pane->Controller) pane->Controller->put_IsVisible(TRUE);
    pane->Showing = true;
}

void Fallback(const Pane& pane, const char* reason) {
    ImGui::TextWrapped("%s", reason);
    const auto url = pane.Current.empty() ? pane.Home : pane.Current;
    if (HelpyImGuiTheme::SoftButton("Open in your browser")) OpenExternal(url);
}
}

Frame::Frame(HWND owner, HINSTANCE module, bool allowed) {
    state.Owner = owner; state.Module = module; state.Allowed = allowed;
    state.Requested.clear(); state.Bounds = {}; ++state.Frame;
}

Frame::~Frame() {
    const bool popup = ImGui::GetCurrentContext() && ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool ownerShown = state.Owner && IsWindowVisible(state.Owner) && !IsIconic(state.Owner);
    const bool sized = state.Bounds.right > state.Bounds.left + 48 && state.Bounds.bottom > state.Bounds.top + 48;
    HWND shown = nullptr;
    for (auto& [route, pane] : state.Panes) {
        if (state.Allowed && ownerShown && sized && !popup && route == state.Requested && !pane->Failed) {
            POINT topLeft{state.Bounds.left, state.Bounds.top}, bottomRight{state.Bounds.right, state.Bounds.bottom};
            ClientToScreen(state.Owner, &topLeft); ClientToScreen(state.Owner, &bottomRight);
            Show(pane, {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y});
            if (pane->Showing) shown = pane->Window;
        } else Hide(*pane);
    }
    Visible.store(shown);
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
                if (it->first != route && (oldest == state.Panes.end() || it->second->LastUsed < oldest->second->LastUsed)) oldest = it;
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
    if (!state.Allowed) {
        Fallback(pane, "Web pages show in Helpy's window and its game overlay, not in the in-game native overlay. Switch overlay mode in Settings, or open this page in your browser.");
        return RS_WEB_UNAVAILABLE;
    }
    if (state.EnvironmentFailed) {
        Fallback(pane, "Helpy could not start its web view. Install or repair the Microsoft Edge WebView2 Runtime, or open this page in your browser.");
        return RS_WEB_FAILED;
    }
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
    ImGui::SameLine();
    if (HelpyImGuiTheme::SoftButton("Open in browser")) OpenExternal(pane.Current.empty() ? pane.Home : pane.Current);
    ImGui::SameLine();
    if (pane.Loading || !pane.Page) ImGui::TextDisabled("Loading...");
    else if (!pane.Title.empty()) ImGui::TextDisabled("%s", pane.Title.c_str());
    if (pane.Failed) {
        ImGui::TextWrapped("The page could not be shown. Check your connection, then choose Retry.");
        return RS_WEB_FAILED;
    }
    const auto at = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
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

bool Owns(HWND window) {
    const auto shown = Visible.load();
    return shown && window && (window == shown || GetAncestor(window, GA_ROOT) == shown);
}

void Close() {
    for (auto& [_, pane] : state.Panes) if (pane) Destroy(*pane);
    state.Panes.clear();
    state.Environment.Reset();
    state.EnvironmentStarting = false;
    Visible.store(nullptr);
}
}
