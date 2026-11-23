#pragma once
// Host-owned web pages inside plugin workspaces (RuneSchemaNavUiV1::WebPage).
// The page lives in a small opaque window owned by Helpy's window.
//   Window mode (Helpy's window, its layered game overlay): the window is kept over the workspace.
//   Texture mode (the native in-game overlay): the window is kept off-screen, Windows Graphics
//   Capture copies it into a texture on Helpy's device, Helpy draws that texture in the workspace
//   and forwards the mouse, wheel and keys to the page. The game keeps focus.
#include "Runtime/RuneSchemaPluginApi.h"
#include "imgui.h"
#include <windows.h>
#include <filesystem>
#include <string>

struct ID3D11Device;
namespace PS::HelpyWebPane {
enum class Mode { Hidden, Window, Texture };
class Frame {
public:
    // Hidden: no page this frame (notifications HUD, server). Texture needs Helpy's D3D11 device.
    Frame(HWND owner, HINSTANCE module, Mode mode, ID3D11Device* device);
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
// Called from a plugin's draw callback: draws the toolbar and the page in the rest of the workspace.
int32_t Draw(const std::string& route, const RuneSchemaWebPageV1* page);
// True for the window of the page shown over Helpy's window (Window mode). Safe from any thread.
bool Owns(HWND window);
// From Helpy's WM_MOVE: moves the shown page with Helpy's window at once, so a drag doesn't trail.
void FollowOwner(HWND owner);
void Close();

// Helpy's own web browser (the Browser tab; off until the player turns it on in Settings). It has
// its own profile, separate from plugin pages, and goes to any http or https address.
struct BrowserSettings {
    bool Enabled = false;
    std::filesystem::path Extension; // The unpacked ad blocker, or empty for none.
    bool PictureInPicture = false;
};
// Called once a frame before Frame. Turning the browser off closes its page.
void Browser(const BrowserSettings& settings);
// Draws the browser: toolbar, address bar and the page in the rest of the workspace.
int32_t DrawBrowser();
// The ad blocker's state in the browser, for Settings.
std::string BrowserExtensionStatus();
// Picture-in-picture: true while a video plays in the browser and it has been watched in the game
// overlay, so the notifications HUD keeps the page captured and shows the video.
bool PictureInPicture();
float PictureInPictureAspect(); // The playing video's width / height.
// Draws the playing video into box (HUD surface coordinates). False when there is nothing to draw yet.
bool DrawPictureInPicture(ImDrawList* draw, ImVec2 min, ImVec2 max);
}
