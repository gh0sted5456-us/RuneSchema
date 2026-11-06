---
channel: experimental
promote_to: CONTROLLER-HUD-BINDINGS.md
---

!!! warning "Local development candidate"
    These changes are not included in the published 0.8.1.0 download. Use matching
    locally built Core, Helpy, HUD and Lua helpers. Hardware/game acceptance is pending.

# Bindings and mod HUDs

## Set a shortcut

Open **Settings → Helpy → Startup → Edit bindings**. Select keys on the keyboard,
or switch to **Controller** and select buttons on the detected controller diagram.
**Record combination** captures physical input after held buttons are released.
**Lock in** saves; **Cancel** leaves the previous binding intact. **Defaults**
restores F2 and controller Home/Guide for Helpy.

Combinations such as **Ctrl + Shift + F2** and **View + West** are supported.
The editor rejects overlapping assignments and reserved Windows combinations.
Bindings are saved locally and remain editable without a connected game.
FPS display, Mod Menu 2 and HUD interfaces use the same editor. Mod Menu 2
continues to require its activation key and compatibility checks.

Controller navigation follows visible sidebar order. Confirm edits a field;
Back cancels an edit or popup and returns to the parent section. At the navigation root it leaves Helpy open. The configured Home/Guide shortcut opens and hides Helpy. Focus uses a translucent gold fill inside control borders, including scaled controls and clipped lists.
Bumpers switch contained panels; triggers switch main workspaces. Confirm
enters a category, tab or nested section. Directional input stays in that section
and returning to it restores the last focused control.
Secondary actions live under **More**,
**View** or the existing settings cog. Mouse and controller can reach them.

Guide reservation remains best-effort: Steam can read the same controller
independently. Helpy does not change Steam or Game Bar settings.

## Reconfigure a detected controller

Open **Settings → Helpy → Controllers**. Choose automatic input/glyph detection,
then edit an action on the controller diagram or record its physical button.
Navigation profiles are saved separately for PlayStation, Xbox, Nintendo and
generic layouts. Assigning an occupied button swaps the two actions. Release
held buttons to apply a changed profile; **Reset this controller profile** restores
its defaults. Guide and multi-button app shortcuts remain in **Startup**.

Item Bay's confirm button toggles the focused card, including when **Select
multiple** is off. The footer shows the currently assigned glyphs; pressed inputs
use the theme accent (gold by default), and hints wrap in smaller windows.

On DualSense (PS5) and DualShock 4 touchpads, one finger moves the pointer.
A short stationary tap left-clicks; a physical press clicks/drags. Swipes,
long holds and two-finger gestures do not produce an extra tap click.
Two fingers hold right-click until one is lifted, with pointer
movement suspended during that gesture. Disabling touchpad control or losing
focus releases held mouse buttons. The detected PlayStation glyphs stay active.

## Declare a mod interface

Place `hud.json` under the mod's `ue4ss` folder. A client binding can be
configuration-only; Lua panels use the mod's companion script:

~~~text
RuneSchema/mods/ExampleMod/ue4ss/
  hud.json
  scripts/main.lua
~~~

~~~json
{
  "SchemaVersion": 1,
  "Interfaces": [
    {
      "Id": "editor",
      "Name": "Example editor",
      "Kind": "panel",
      "Keyboard": "Ctrl+F7",
      "Controller": "View+West",
      "Controls": [
        {"Id":"enabled","Type":"toggle","Label":"Enabled","Value":true},
        {"Id":"size","Type":"slider","Label":"Size","Min":0,"Max":2,"Step":0.1,"Value":1},
        {"Id":"apply","Type":"button","Label":"Apply"}
      ]
    },
    {
      "Id": "advanced",
      "Name": "Advanced editor",
      "Kind": "client",
      "Client": "Example.Editor"
    }
  ]
}
~~~

Interface/control IDs use letters, digits, dots, underscores or hyphens.
The full interface identity is `ExampleMod/editor`; keep it stable across updates.
Declarations are read at game startup from enabled RuneSchema mods and enabled
subloaders. External UE4SS mods are not scanned or managed by this feature.

Panels support `label`, `toggle`, `slider`, `choice`, `text` and `button`.
Choices declare an `Options` string array. A label's `Value` is display text.
Controls stay disabled until the mod registers their callbacks:

~~~lua
local hud = rawget(_G, "RuneSchemaHUD")
if hud then
    hud.Register("ExampleMod", "editor", {
        enabled = function(value) settings.enabled = value end,
        size = function(value) settings.size = value end,
        apply = function() apply_settings() end
    })
end
~~~

A callback receives a boolean, number or string; buttons receive no value.
Return `false` to reject a change. An error is isolated and reported in the
RuneSchema log. `hud.Set(owner, id, control, value)` publishes mod-owned changes.
`hud.Unregister(owner, id)` disables the controls and removes pending commands.

Callbacks run on one bounded game-thread loop, started on first registration and
cancelled after the last unregister. RuneSchema supplies presentation and input
capture; the mod owns persistence, gameplay validation and network authority.
It does not copy mod settings into native saves. Keep the mod's standalone
behavior when RuneSchema is unavailable.

`Kind: client` binds a ready producer registered with the [existing HUD API](SHARED-HUD.md).
Exactly one live producer must match `Client`. JSON does not launch an executable,
render a browser or grant authority. In **Tools → Extensions**, choose an interface
to open it or change its bindings. A panel hotkey opens it directly in Helpy's
interactive surface; closing restores the normal input lifecycle.

## Design references

The pass follows Microsoft's [XAG 112](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/112)
for predictable focus and action placement, Dear ImGui's
[navigation/capture guidance](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md),
and SDL's [controller button-label API](https://wiki.libsdl.org/SDL3/SDL_GetGamepadButtonLabel).
Synthetic checks cover controller routes, modal tabs, edit cancellation,
bindings and window sizes. They do not establish physical-device or live-game acceptance.
