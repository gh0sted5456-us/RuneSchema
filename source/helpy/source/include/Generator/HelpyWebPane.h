#pragma once
// Host-owned web pages inside plugin workspaces (RuneSchemaNavUiV1::WebPage).
// The page lives in a small opaque window owned by Helpy's window and kept over
// the workspace area, so it also shows in Helpy's layered game overlay. It is
// hidden in the native (in-game) overlay, where Helpy has no window to own it.
#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include <string>

namespace PS::HelpyWebPane {
class Frame {
public:
    // allowed=false hides every page this frame (native overlay, notifications HUD, server).
    Frame(HWND owner, HINSTANCE module, bool allowed);
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
// Called from a plugin's draw callback: draws the toolbar and reserves the rest of the workspace.
int32_t Draw(const std::string& route, const RuneSchemaWebPageV1* page);
// True for the window of the page now showing. Safe from any thread.
bool Owns(HWND window);
void Close();
}
