---
channel: experimental
promote_to: NATIVE-OVERLAY.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Helpy native overlay adapters

Helpy draws its normal UI in its own process. The game-side bridge only composites
the current shared texture and forwards captured input. Game actions keep using
the existing RuneSchema service contract. **Game overlay** and **Native in-game
presentation** default to enabled for new settings. Existing saved choices stay
in use. Disable **Game overlay** in Settings/Quick settings for a regular window.
The system tray opens desktop mode for settings and Windows file dialogs.

| Renderer | Native integration | Requirements |
| --- | --- | --- |
| DX11 | DXGI presentation; deferred commands restore the game's context state | Windows shared-texture/fence support; SDR |
| DX12 | DXGI presentation; verified back-buffer queue and per-buffer completion fences | SDR; queue verified through legacy or enhanced barriers |
| OpenGL | WGL/GDI swap; imported memory and semaphore objects; saved GL state | `EXT_memory_object_win32`, `EXT_semaphore_win32`, compatible modern context |
| Vulkan | Startup layer; imported D3D11 texture memory, external semaphores and present chaining | Opt-in startup enrollment, Win32 external-memory/semaphore support, a graphics/present queue, SDR |

The game and Helpy must use the same GPU. Helpy reads the game's adapter identity
and recreates its UI device on that adapter when needed, including when an offline
window first attaches to a hybrid laptop's game. CPU item caches, settings and
open-page state remain intact; GPU textures are uploaded again. Unsupported combinations show a status
instead of attempting an unchecked GPU copy. Desktop presentation remains usable
in windowed/borderless mode alongside any of these game renderers. Helpy does not
enable an RHI that Dragonwilds does not ship.

## Vulkan startup enrollment

Open **Settings > Helpy > Display > Vulkan startup adapter**. Enable the
adapter for the selected runtime, then restart Dragonwilds. The startup layer must
be present when Vulkan creates its device so it can enable available sharing
extensions. It does not alter the app's requested device features.

The optional `RuneSchema.Helpy.Vulkan.dll` is a small selector. The shared copy,
layer manifest and executable-to-bridge bindings live in
`AppData/Local/RSDragonwilds/RuneSchema/core/HELPY/vulkan/`. Enrollment writes only
Helpy's own manifest value under the current user's
`Software/Khronos/Vulkan/ImplicitLayers` key. Other Vulkan layer registrations
remain intact. The selector delegates to the bridge only for an enrolled exact
executable path. Remove a runtime with the same button; removing the last binding
unregisters Helpy's manifest. A running game's active adapter unloads on exit.

Elevated games ignore per-user Vulkan layers; use normal game launches. The
development tests use process-local manifests and environment variables and do
not register a system/user layer.

## Bounded work and resource ownership

- Three shared textures avoid overwriting a frame still used by the game.
  Producer/consumer ownership uses atomic slot reservations and completion fences.
- Native presentation uses no CPU frame readback, PNG encoding or JSON frame
  transport. JSON is used for preparation and visibility/session changes.
- Hidden native hooks pass through. App rendering and controller state reads
  stop while hidden. Device discovery occurs on device changes, not every frame.
- Pointer movement uses the latest position once per UI frame. Native capture
  keeps the Windows hardware cursor visible independently of UI/game FPS and
  protects its shape from game cursor-hiding calls. A rendered cursor remains
  the fallback if Windows does not confirm visibility. Shape changes send one
  notification when changed; closing restores the game's cursor and display
  count. Click positions preserve event ordering and scale from
  the game client area to the render viewport. High-rate mouse/raw-input and
  unrelated window messages do not contend with the compositor's resource lock.
- DX12 reuses a bounded pool of completed submission allocators instead of
  omitting the overlay when a back-buffer allocator is busy. It does not wait
  for GPU completion on the game's presentation thread.
- While visible, composition serializes with the short resource-control lock
  instead of omitting a frame on contention. DXGI `Present1` updates the full
  frame while captured, so game-only dirty rectangles cannot retain old panel
  or pointer pixels. Hidden frames retain the existing pass-through path.
- Background opacity leaves text and controls opaque. Native status is visible
  directly below its switch; renderer readiness, attachment and failures also
  produce bounded entries in RuneSchema.log.
- DX11/DX12 accept 8-bit and 10-bit SDR back buffers. Display-encoded UI pixels
  use UNORM target views so an sRGB view cannot brighten the theme again.
- Opening failures show their reason and a usable desktop window. The first
  displayed frame produces one status update per opening; attachment alone is
  not reported as successful presentation. No per-frame status requests are used.
- The game receives neutral XInput state while the panel captures input. Cursor
  clipping/recentering is suspended only for the attached foreground game.
- Focus loss, a full input queue, or a heartbeat older than 1.5 seconds releases
  capture. The desktop app owns its normal close/tray/exit behavior.
- Resize, swap-chain destruction, game shutdown and app replacement release
  bindings. GPU resources remain alive until submitted work completes.
- Hook trampolines/code stay pinned for the game's lifetime so an in-flight
  callback cannot jump into an unloaded DLL. This is one bounded allocation,
  not an accumulation per opening.

## Verification and limits

Isolated hardware tests on an NVIDIA RTX 5080 verify native pixels, premultiplied
transparency, bounds, shared fences and hidden pass-through for DX11, DX12,
OpenGL and Vulkan. DX11 tests also verify context state restoration. Vulkan tests
exercise the real loader, startup layer, shared-surface attachment, destruction,
selector delegation and non-enrolled process behavior. These fixtures do not
launch Dragonwilds, change saves or register a real Vulkan layer.

Live Dragonwilds exclusive-fullscreen, AMD/Intel drivers, multiple monitors/DPI
changes, other overlays and controller hardware still need acceptance testing.
HDR, separate Vulkan graphics/present queues and cross-adapter sharing are not
claimed. Native mode keeps secondary panels docked; authoring file dialogs use
Windows and can require desktop focus.

API references: [Microsoft DX12 swap chains](https://learn.microsoft.com/en-us/windows/win32/direct3d12/swap-chains),
[Khronos external memory and synchronization](https://github.khronos.org/Vulkan-Site/guide/latest/extensions/external.html),
[OpenGL Win32 external objects](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/extensions/EXT/EXT_external_objects_win32.txt),
and [Vulkan loader/layer interface](https://vulkan.lunarg.com/doc/view/latest/windows/LoaderLayerInterface.html).
