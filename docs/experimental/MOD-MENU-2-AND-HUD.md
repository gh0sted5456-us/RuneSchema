---
channel: experimental
promote_to: MOD-MENU-2-AND-HUD.md
---

!!! info "Pre-release · 0.8.1.0"
    Use matching core, Helpy and Lua files. Mod Menu 2 is optional/key-gated;
    it is not enabled in the stable package. [Candidate](PRE-RELEASE.md).

# Mod Menu 2 and HUD layout

The activation key unlocks Mod Menu 2; a running game and its Lua adapter are
still required for live controls. Helpy now reads large menu snapshots without
treating a small response buffer as an inactive Core. Offline/disconnected
status is shown separately from key acceptance. Activation changes still need
a full game restart after disabling the separate Mod Menu.

Mod Menu 2 brings compatible mod settings into Helpy's themed two-pane interface.
Opening it uses the same overlay, cursor and input capture as normal Helpy.
Closing it returns input to the game. Passive HUD elements remain click-through.
Management stays inside Helpy; Mod Menu 2 does not add to or change Dragonwilds'
settings menu and does not open the original Mod Menu page.

The **License** entry at the bottom of Mod Menu 2's category column displays
Maxxfilth's original MIT notice and its permissions. It remains readable without
a connected game and includes **Copy license**. The original notice is embedded
from the packaged license file. MIT permits adapting and distributing the licensed
code while retaining its copyright and permission notice; see the
[MIT license](https://opensource.org/license/mit).

## Enable Mod Menu 2

1. Disable or remove the separate Maxxfilth Mod Menu through UE4SS, then close
   Dragonwilds. A mod already loaded in the current process needs a restart.
2. Place the maintainer-provided `modmenu2.key` in
   `RuneSchema/plugins/RuneSchema.Helpy/`. `RuneSchema/plugins/Helpy/` is also
   recognized. The key contains the activation hash, a blank line and the
   complete original MIT license. Preserve its text.
3. Start Dragonwilds with the matching candidate and open Helpy. **Mod Menu 2**
   appears in navigation after the key is recognized.
4. Under **Settings → Helpy → Mod Menu 2**, enable the integration and optionally
   choose its own F-key shortcut. Activation changes require restarting the game;
   changing the shortcut takes effect immediately in an active session.

No separate Mod Menu registration runs without the key and enabled integration.
If the external Mod Menu is configured to run, or already published its runtime
version, RuneSchema leaves the integration inactive. Changing `mods.txt` during
play does not unload an existing mod. The key unlocks a feature; it is not a
server authentication credential. Local test packages keep it separate.

## Existing mod settings

The adapter reads `modmenu.json` and the equivalent `modmenu.txt` schemas from
external UE4SS mods and enabled RuneSchema Lua subloaders. It supports toggle,
slider, choice, text, key and action-button rows, headers, conditional visibility,
and host/server ownership. Schema version labels are informational.

Existing configuration files and the original `ModMenu.<id>.<key>`, `.rev` and
`.action` shared variables remain in use. `ModMenu.request.set` and
`ModMenu.request.open` are supported, including `|row=<key>` jumps. Changes are
validated against the current file; a stale editor cannot overwrite another
writer's changes. Replaced configurations are backed up in Helpy's AppData area.
Unknown config entries and comments are retained where the original format
supports them. A setting marked for restart still needs a restart.

The legacy UMG pause menu, quick panel and input hooks are not installed. Helpy
supplies presentation and input capture. Health is reported as unknown or off
unless another diagnostic has measured it. Mod Menu 2 messages go to
`RuneSchema.log`.

## Optional character-editor presentation

Existing schemas work without any new fields. A mod can add a `helpy` object to
group controls into tabs and request a character-preview column:

```json
{
  "id": "ExampleCharacterEditor",
  "name": "Character editor",
  "helpy": {
    "layout": "character",
    "tabs": [
      { "name": "Appearance", "keys": ["Height", "Build"] },
      { "name": "Style", "keys": ["Hair", "Color"] }
    ],
    "preview": { "channelVariable": "ExampleCharacterEditor.preview" }
  },
  "settings": [
    { "key": "Height", "type": "slider", "min": 0, "max": 1, "default": 0.5 },
    { "key": "Build", "type": "slider", "min": 0, "max": 1, "default": 0.5 },
    { "key": "Hair", "type": "choice", "options": ["Short", "Long"] },
    { "key": "Color", "type": "text", "default": "Brown" }
  ]
}
```

Tabs contain setting keys. Up to 16 tabs and 200 keys per tab are accepted. A
narrow window stacks the preview below the controls. Without a provider, the
settings still work and the preview area explains what is missing.

The mod supplies the preview; Helpy does not capture an arbitrary character
automatically. A native provider uses RuneSchema's existing
`CharacterPreviewFrame::Channel` in `source/helpy/source/include/Core/CharacterPreviewFrame.h`.
It creates a game-process-owned channel named
`Local\RuneSchema.CharacterPreview.<game-pid>.<identifier>` and publishes that
name through the shared variable above. The fixed transport contains a 512×512
RGBA frame, serial counter, status and camera controls. Use the declared struct,
mutex and validation; never send engine pointers. Read camera requests and
publish pixels under the channel lock. Helpy uses nonblocking reads and returns
the existing yaw, pitch, zoom and stop requests. Close the provider when stopped
or its consumer heartbeat expires. Another active character preview is preserved.

This is the extension point for JBBB or another character editor. It does not
change that mod's models, sliders or persistence implementation by itself.

## Public HUD layout

**Settings → Helpy → HUD** is available without a Mod Menu 2 key. Its canvas uses
the connected game's viewport size, with a 1920×1080 preview while disconnected.
Drag an element, or use the position sliders with keyboard/controller navigation,
then choose **Save HUD layout**. Saved positions apply to the running HUD.

- The minimap switch applies and saves immediately, including when disabling it.
  Adjust its size and zoom, then choose **Save HUD layout**. It uses the downloaded or
  imported world-map image cache and the existing coalesced coordinate sample.
  It does not scan PAKs or query the game every rendered frame.
- **Keep player centered** follows movement. Turn it off to hold the view at the
  player position captured when you change the switch, or use **Center view on
  player** to recenter. **Lock minimap position** separately prevents moving the
  minimap in the layout editor. Size, zoom, both locks and placement persist after
  **Save HUD layout**, including after restarting Helpy or Dragonwilds.
- Minimap position updates are coalesced at no more than ten requests per second
  while its game is focused, with interpolation between samples and immediate
  snapping after large teleports or stale data. Ordinary coordinates retain the
  slower sampling rate. No hidden-game polling is added.
- Place X/Y/Z and Pitch/Yaw below the minimap in two centered rows with the same
  panel width and text fitted to that width, or position the horizontal strip
  separately. Coordinates have their own visibility toggle.
- Position notifications and the startup loading indicator.

The passive minimap uses native presentation, including exclusive fullscreen
when the graphics adapter is supported. Coordinates, notifications and loading
also retain the existing desktop presentation fallback for windowed/borderless
games. Opening Helpy replaces the passive HUD with the interactive application.
The minimap needs a valid cached map projection and a current local player
position; menu screens and dedicated servers do not have a player minimap.

Game Pass can expose the installation through a WindowsApps junction. Mod Menu
activation and its backend resolve that host-provided root to the same physical
runtime before inspecting files. Links inside a mod payload are still rejected;
this does not permit a mod configuration to write outside its registered scope.

## Credits

Based off Maxxfilth's Mod Menu, adapted natively for RuneSchema. The schema,
configuration and shared-variable backend comes from Mod Menu 2.0.2, Copyright
(c) 2026 Maxxfilth, under MIT. The full notice is retained in
`licenses/ModMenu-MIT.txt` and packaged notices.
[Original Mod Menu](https://www.curseforge.com/runescape-dragonwilds/ue4ss-mods/mod-menu).
