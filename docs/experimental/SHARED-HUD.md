---
channel: experimental
promote_to: modders/SHARED-HUD.md
---

!!! info "Pre-release · 0.8.1.0"
    Use matching Core, Helpy and HUD files from the [0.8.1.0 candidate](PRE-RELEASE.md).
    Real-app/game acceptance of the public interface remains pending.

# Shared HUD plugin

`RuneSchema.HUD` owns native presentation and game input capture. Helpy is one
client; other apps can use the renderer through an in-game UE4SS adapter. An app
keeps its own UI, profiles and local fallback. HUD access grants no gameplay,
networking or filesystem authority.

The plugin lives at `RuneSchema/plugins/RuneSchema.HUD/dll/RuneSchema.HUD.dll`.
Core loads its manifest through the existing plugin lifecycle. Graphics hooks
prepare only when requested. Dedicated servers skip it. Helpy's offline cache
browser and separate desktop window remain usable without native presentation.
Initialization verifies Core's input/extension capabilities; an older Core with
the same release label cannot silently claim this interface. Repair retains
older self-contained packages and requires HUD files when Helpy declares them.

## Public adapter headers

The SDK is in `source/shared/`:

| Header | Contract |
| --- | --- |
| `RuneSchemaHudApi.h` | C function table, client lifetime and activation commands |
| `RuneSchemaHudFrames.h` | Frame/input layout, heartbeats and ownership |
| `RuneSchemaHudSurface.h` | Optional C++ D3D11 texture producer |
| `RuneSchemaUserSecurity.h` | Same-user security for shared objects |

An in-game adapter obtains a reference to the already-loaded module with
`GetModuleHandleExW`, then resolves `RuneSchemaHud_Query` as
`RuneSchemaHudQueryFn`. Check `StructSize` before reading fields and check the
required function pointers. `ApiVersion` identifies the interface; a semantic
release label or a directory does not establish readiness. Retain the module
reference until all registered clients are closed. Do not load another plugin
copy or use Helpy's private desktop pipe.

Call `RegisterClient("YourMod.Appearance", producerPid, &client)`. It succeeds
only after HUD initialization and validates a live producer owned by the same
Windows user. The producer may be the game process or the app's EXE. Each client
gets a distinct opaque handle. This is a trusted local-mod interface, not a
sandbox for untrusted native code.

## Helpy activation and WebGUI apps

Registered clients appear under **Tools → Extensions** after Refresh. Entries
remain **Integration pending** until the app sends
`{"Action":"UiReady","Ready":true}`. Declare readiness only after creating
and verifying the actual UI producer; network readiness is separate.

The adapter calls `DuplicateWakeEvent(client, &handle)` to receive its own event
handle, then waits on that event and its own shutdown event. It owns and closes
the duplicated handle. `NextCommand(client, &command)` consumes the latest
coalesced activation intent:

- `RS_HUD_COMMAND_ACTIVATE`: Helpy has requested the panel and hides itself.
  Prepare/open the producer through the shared surface protocol below.
- `RS_HUD_COMMAND_DEACTIVATE`: hide the app's panel and release its HUD surface.
- `RS_HUD_COMMAND_NONE`: no pending intent.

Marshal commands to the app's UI thread using its own IPC. Neither an event nor
an activation request proves the panel is displayed. Use the actual bound
surface, heartbeat and presented-frame status for that claim. Escape/Close in
the app hides its page and calls `Hide` or `Release`; it does not terminate
Helpy. Before teardown, send `UiReady=false`, stop/join the adapter's worker,
close duplicated handles, then `CloseClient` and release the module reference.

Helpy uses Dear ImGui, not a WebView. It does not directly embed a localhost URL.
The supported path is an **app-owned WebView producer**: the app owns navigation,
its token-protected HTTP backend and WebView close messages, produces shared
pixels, and lets Helpy activate it through this lifecycle. No token or URL needs
to enter Core. A WebGUI app still needs a verified pixel/input adapter; these
headers do not convert a browser window into shared textures automatically.

A missing RuneSchema/HUD interface leaves standalone local behavior available.
An app whose user selected RuneSchema-only integration can instead remain
integration-pending. It must not silently activate a second menu or transport.

## Presentation protocol

`Call(client, request, response, capacity, &size)` accepts UTF-8 JSON. Allocate
`RS_HUD_MAX_MESSAGE` response bytes. An undersized buffer performs no mutation.

1. Request `{"Action":"Prepare"}` and use bounded `Status` retries while the
   game's renderer is detected. Read `Ready`, `Api`, `Width`, `Height`, `Window`,
   `AdapterLow` and `AdapterHigh`.
2. Create `PS::HudGpu::Producer` on the matching D3D11 adapter with a fresh
   32-character lowercase hexadecimal nonce. Set game viewport and panel bounds.
   Pixels are premultiplied, display-encoded RGBA. Use `Begin`/`Publish` and the
   three slots/shared fences; retain textures while the GPU consumes them.
3. Request `{"Action":"Open","Nonce":"YOUR_NONCE","Interactive":true}`.
   The registered PID and client handle override any request-supplied identity.
   `Pending=true` needs a delayed retry of the same Open; keep retries bounded.
4. Once `OwnsSurface=true` and the page is bound, refresh its heartbeat, set
   `Visible=1` for an interactive panel or `Visible=2` for a passive HUD, then
   request `Show`. The app consumes forwarded input and owns widget navigation.
5. `Hide` releases that client's presentation/input. `Release` also relinquishes
   ownership. The registered client can reopen later. `CloseClient` unregisters it.

This version presents **one surface at a time**. A live interactive owner cannot
be displaced by another client. A competing Open returns `Busy=true`. Interactive
panels can replace passive HUDs; passive clients wait. Helpy waits without
changing its overlay preference or opening a desktop fallback. Foreign
Show/Hide/Release/Close operations cannot release the current owner's surface.

The plugin retains the existing DX11/DX12/OpenGL and opt-in Vulkan adapters.
Heartbeats expire after 1.5 seconds. Focus loss, overflow and shutdown release
capture. A passive client cannot gain input merely by changing its visibility
value. Native capture transitions tell Core to suppress game movement/look;
Helpy's own close does not clear another client's request. Apps use this input
route instead of competing game-window/controller hooks. There are at most 32
registered adapters; dead producer registrations are reclaimed on registration.

## Separate content and networking

The [appearance route](CHARACTER-APPEARANCE.md) remains independent. The mod owns
providers, preview, validation, profiles and Submit. Core alone owns its enrolled
registry route and server bridge. A registered HUD does not enroll content or
publish a cosmetic change. No real appearance/WebGUI adapter is activated by
installing this plugin.
