---
channel: experimental
promote_to: CONTROLLER-NAVIGATION.md
---

!!! info "Pre-release · 0.8.1.0"
    Included in 0.8.1.0. USB/Bluetooth hardware and live-game acceptance remain
    separate from virtual-device and render fixtures. [Candidate](PRE-RELEASE.md).

# Use Helpy with a controller

The window and native overlay share the same controls. Open Helpy with its normal
shortcut or tray icon, then use the D-pad or left stick to move the highlighted
selection. Confirm activates buttons, switches, lists and item cards. Menus and
dialogs retain their normal validation; selection alone never delivers an item.

The splash button shows your controller's confirm glyph: **Cross** on
PlayStation, **A** on Xbox or **B** on Nintendo. Confirm enters Home; use the
D-pad or left stick to highlight a tile, then confirm to open it. Controller
entry assigns the first focus target automatically, so a mouse click is not
required to start navigation. Returning to Home focuses its first available
tile. Mouse/keyboard mode keeps the normal **Enter** label.

## Navigation and quick actions

| Input | Action |
| --- | --- |
| Left / right trigger | Previous / next main section |
| Left / right bumper | Previous / next visible panel; restore its last focused control |
| D-pad / left stick | Move within the highlighted panel |
| Bottom face button | Select or deselect an item; enter or activate a control |
| Right face button | Back; close the current editor before returning to its parent |
| Left face button | Actions for the focused item |
| Top face button | Item Bay search |
| Menu / Options / Plus | Send selected Item Bay items to cart |
| Right stick | Enter pointer mode and move the cursor |
| Right-stick click | Grab; click again to drop without holding the stick down |
| Left-stick click | Toggle Item Bay multi-select; zoom in on the map |

Moving the right stick enters pointer mode and shows a gold ring cursor. The
bottom face button becomes **left-click / drag**, and the left face button becomes
**right-click** at the pointer. On PS5 these are **Cross** and **Square**; Xbox uses
**A** and **X**. Pointer mode stays active when the stick rests. D-pad, left-stick
or menu navigation returns to focus controls. Mouse clicks and navigation
confirmation are routed exclusively, so one press cannot trigger both. The
footer shows the current actions; remapping Confirm or Actions also updates the
corresponding mouse button. Right-stick click holds the grab until its next
click. Hiding Helpy, losing focus or leaving pointer mode cancels the grab.

A gold border identifies the focused panel. The D-pad stays within its boundary;
use the bumpers to switch between Item Bay controls, results and cart. Quantity
fields support controller adjustment after activation; explicit **−**, **+** and
remove buttons remain available. Active editors, popups and drags keep focus.

For one item, choose **Send to player…** from its right-click/actions menu. Set
quantity, choose a connected player and send. Helpy remembers the chosen player
by character identity and asks again if that player is absent. This action uses
only that item and leaves staged cart contents unchanged.

Open **Mods → Mod Menu 2** for live mod controls. It keeps the same panel focus and controller hints. Activate a slider or
dial with the bottom face button, adjust with left/right, then confirm to submit
the edit. Bumpers and section changes wait until the editor is closed.

On the map, the **left stick pans**, its **press zooms in**, and the **top face
button zooms out** while the canvas has focus. The right-stick pointer can select
locations or click and drag the map. The usual mouse controls remain available.

**Settings → Helpy → Controllers** offers **Always on**, **Auto detect** and
**Disabled (mouse & keyboard)**. Auto detect follows new input activity, including
the controller pointer. Always on keeps the controller presentation and hints.
Cursor speed, cursor enablement, glyph choice and per-controller bindings are
saved locally and shared by standalone Helpy and its overlay. Stick dead zones
prevent resting drift; cursor movement uses elapsed frame time.

PlayStation touchpad movement, one-finger tap/press and two-finger actions remain
available. Synthetic pointer events stay associated with the controller so they
do not switch Auto detect to mouse mode. The pointer stops and releases both mouse buttons
when Helpy loses input focus or controller input is disabled. Held face buttons
must be released before they can click again after a focus or mode change.

This pass follows [Xbox navigation guidance](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/112)
and [focus guidance](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/113).
Virtual-device/render fixtures are separate from physical USB/Bluetooth and live
game verification.

## Open and hide Helpy from the controller

Open **Settings → Helpy → Startup → Controller shortcut**. The controller
shortcut is independent of the normal keyboard shortcut; F2 remains available
unless you have changed it.

- **Xbox / PS / Home button** is the default. It uses the controller's Guide
  button to open or hide Helpy while the assigned game or Helpy has focus.
- **View + Menu** uses both small menu buttons together: Share + Options on
  PlayStation, or Minus + Plus on Nintendo. Use it when another application
  reserves the Guide button. Check it against your own game/controller bindings.
- **Disabled** stops background controller-shortcut checks. Normal controller
  navigation still works when Helpy is open.

Holding a shortcut does not repeatedly toggle the panel. Rebinding or returning
from another application requires held buttons to be released before they can
toggle Helpy. SDL and native Guide reports are deduplicated. Hidden checks reuse
Helpy's existing window loop, at most once every 50 ms; they do not render the
panel, search items or scan game objects.

**Reserve Guide button in game** requests Windows GameInput's
exclusive foreground Guide policy for Dragonwilds. It works independently of
the game's graphics API. The matching bridge forwards Guide presses to Helpy
without requiring the panel to be open. A small lease renews every two seconds;
the reservation is released when Helpy exits, when disabled, or within five
seconds of lost bridge contact. Other game controls are not reserved by this
setting. Missing GameInput support leaves the SDL shortcut and keyboard binding
available; Settings shows the bridge status.

This reservation applies to applications using **GameInput**. Steam can read
controllers independently, so Helpy cannot promise a global lock on the Guide
button. If Steam or Game Bar also opens, change its Guide-button action or use
the menu-button combination. Helpy does not edit those applications' settings.
See Microsoft's [Guide focus policy](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/enums/gameinputfocuspolicy)
and Valve's [Guide Button Chord configuration](https://partner.steamgames.com/doc/features/steam_controller/getting_started_for_players).

## Give the workspace more room

The main left navigation has an **expand/collapse arrow** and a **pin** beside
the logo. Anyone can collapse it into an icon rail; hover or focus an icon to see
its page name. Expanding restores the previous sidebar width. Both controls
accept mouse, keyboard and controller navigation.

Controller mode folds an unpinned rail automatically. Expand it to choose a page;
it folds again after selection. Pin it to keep the labels visible. Pin and manual
collapse preferences are saved, while temporary controller expansion is not.
Returning to mouse/keyboard restores the manual preference.

## Devices and button labels

The app bundles pinned **SDL 3.4.16** controller support for Xbox, supported
PlayStation DualShock/DualSense and Nintendo controllers, with XInput fallback.
SDL's device mappings provide USB/Bluetooth normalization and reported labels.
The Windows driver, connection mode and any Steam Input/emulation layer still
affect which device identity is visible. No translation driver is installed by Helpy.

Automatic glyphs follow the selected device's labels. A manual **Xbox**,
**PlayStation**, **Nintendo** or **Generic** layout is available when a mapper
hides the original controller. Layout changes apply immediately. Labels describe
physical positions; they do not change the underlying game's bindings.

| Action | Xbox | PlayStation | Nintendo |
| --- | --- | --- | --- |
| Confirm / select, south button | A | Cross | B |
| Cancel / back, east button | B | Circle | A |
| Item/mod actions, west button | X | Square | Y |
| Open keyboard for a focused text field, north button | Y | Triangle | X |
| Change tabs in the focused pane | LB/RB | L1/R1 | L/R |
| Change main sections outside active editors/dialogs | LT/RT | L2/R2 | ZL/ZR |

These positions follow SDL's documented
[gamepad button layout](https://wiki.libsdl.org/SDL3/SDL_GamepadButton).
Generic glyphs use south/east/west/north labels when the device is unknown.
If multiple controllers are connected, a newly active mapped device becomes
the selected one; disconnects release its held controls.

## Items, mods, maps and text

- Press **Down** from a section's tabs/header to focus its content. Use the
  D-pad or left stick to move between controls and
  between adjoining panes. Press **Right** from a category list to enter its
  settings/content pane. Pane borders do not need an extra confirm press.
- Focus an Item Bay card and confirm to select it. Enable **Select multiple**
  for a batch, then use **Send to cart**. The west button opens the existing
  actions menu, including family/details commands. Drag/drop remains available.
  Use the D-pad between cards and controls. Focus the search field and use the
  north button for the keyboard; cancel retains the existing search.
- Focus a mod card and open its actions with the west button. Existing **Up**,
  **Down**, enable switches and save controls also accept normal navigation.
- Focus the map canvas. The right stick pans, stick clicks zoom in/out, confirm
  selects its center crosshair, and the west button opens actions there. Existing
  authority, ground and placement checks still apply.
- On DualSense/PS5 and DualShock controllers, slide one finger on the touchpad
  to move the pointer; press the touchpad to click or hold it to drag. Lift and
  reposition your finger without jumping the cursor. Disable **PlayStation
  touchpad pointer** under **Settings → Helpy → Controllers**
  if another input mapper handles it. Helpy processes touchpad clicks only while
  its interface has focus; hiding it, disconnecting or disabling the option
  releases its click. Steam Input/emulation may hide the original touchpad.
  This uses SDL's [touchpad state](https://wiki.libsdl.org/SDL3/SDL_GetGamepadTouchpadFinger).
- Activate a text field with the controller to open the in-app keyboard, or use
  the north button on a focused field. Navigate its keys, then choose **Done**
  to apply the text or **Cancel** to retain the original. Password previews stay
  masked. This works inside the server PIN dialog and with fields rebuilt each
  frame. It does not open a desktop keyboard over an exclusive-fullscreen game.

The keyboard offers letters, numbers, symbols, shift, space, clear and backspace.
It retains capacity/character restrictions and keeps the original field's identity.
No user-buffer pointer is retained between frames. Closing Helpy cancels open
dialogs and clears the temporary keyboard draft.

## Build and package requirements

The Windows x64 SDK download is version/hash-pinned in Helpy's CMake file.
Build with the existing Windows SDK/MSVC prerequisites; dependency and build
outputs belong in the configured build directory. An existing verified SDK may
be supplied through CMake's standard `FETCHCONTENT_SOURCE_DIR_HELPY_SDL` override.
The Guide reservation bridge requires Windows SDK **10.0.26100 or later** to
build. It loads Windows GameInput dynamically; an unavailable runtime leaves the
SDL and keyboard shortcuts available and does not prevent Helpy from starting.
Keep **SDL3.dll beside Helpy.exe** when installing the optional app or a matching
runtime update. Preserve its complete zlib notice in the packaged licenses.

Virtual fixtures cover Xbox, PS4, PS5 and Switch Pro labels/state, Guide and
menu-button shortcuts, bounded event queues, shutdown and nested text entry.
Shortcut tests cover held/rebound buttons, focus changes, duplicate reports and
native reservation expiry/cleanup where GameInput is available. Production UI captures cover small and
large layouts. These do not establish acceptance on every physical controller,
firmware, adapter or Bluetooth driver.
