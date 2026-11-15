---
channel: experimental
promote_to: MOD-MENU-2-AND-HUD.md
---

!!! info "Folder layout during testing"
    The [complete 2026-11-06 candidate](PRE-RELEASE.md) uses the [new runtime layout](RUNTIME-LAYOUT.md): Helpy under `Helpy`, core plugins under `dlls/plugins` and Helpy plugins under `Helpy/plugins`. Legacy layouts remain supported.


!!! info "Pre-release · 0.8.1.0"
    Use matching core, Helpy and Lua files. Mod Menu 2 is optional/key-gated;
    it is not enabled in the stable package. [Candidate](PRE-RELEASE.md).

# Mod Menu 2 and HUD layout

!!! info "Native Helpy plugin in the complete candidate"
    The complete candidate moves session polling and commands into
    `Helpy/plugins/RuneSchema.ModMenu2/dll/RuneSchema.ModMenu2.dll`. Use its
    matching manifest, Core and Helpy. Use the complete package.

The plugin runs in Helpy, using the shared themed controls and input capture.
Only the narrow Lua/game adapter runs inside Dragonwilds. Removing or disabling
the plugin hides the feature and prevents its game-side registrations; the
activation key and separate-Mod-Menu conflict checks still apply. Existing
`modmenu.json`/`modmenu.txt`, variables and callbacks remain compatible.
The plugin includes Maxxfilth's original MIT notice as `ModMenu-MIT.txt`.

The activation key unlocks Mod Menu 2; a running game and its Lua adapter are
still required for live controls. Helpy now reads large menu snapshots without
treating a small response buffer as an inactive Core. Offline/disconnected
status is shown separately from key acceptance. Activation changes still need
a full game restart after disabling the separate Mod Menu.

Open **Mods → Mod Menu 2** for live controls, alongside **Management**. The
plugin keeps its own mod list and License pane; it has no duplicate main-navigation
entry. Existing direct shortcuts still open it.

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
   `RuneSchema/Helpy/plugins/RuneSchema.ModMenu2/`. `RuneSchema/Helpy/` and
   the legacy `plugins/RuneSchema.Helpy/` location are also recognized. The key contains the activation hash, a blank line and the
   complete original MIT license. Preserve its text.
3. Start Dragonwilds with the matching candidate and open Helpy. **Mod Menu 2**
   appears under **Mods** after the key is recognized.
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

Existing schemas work without any new fields. A mod can add a `"--Helpy"` object to
group controls into tabs and request a character-preview column:

```json
{
  "id": "ExampleCharacterEditor",
  "name": "Character editor",
  "--Helpy": {
    "schema": 1,
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
    { "key": "Color", "type": "text", "default": "#BFA35A" }
  ]
}
```

Tabs contain setting keys. Up to 16 tabs and 1,024 keys per tab are accepted. A
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

## Helpy-only controls and shared templates

!!! warning "Local development candidate"
    These extensions require the matching Core, Helpy plugin and Lua adapter.
    They are not in the published package. Existing schemas remain compatible.

Use the root JSON namespace `"--Helpy"`, with `"schema": 1`. The original
Mod Menu reads its ordinary `settings` list and ignores this namespace. It is
valid JSON, not literal `--` comments. The older `helpy` extension remains accepted.

```json
"--Helpy": {
  "schema": 1,
  "controls": { "Height": "dial", "Hair": "radial", "Color": "color" },
  "settings": [
    { "key": "Hip", "type": "dial", "label": "Hip width",
      "min": 0, "max": 1, "step": 0.01, "default": 0.5 }
  ],
  "templates": {
    "id": "appearance", "version": 1,
    "keys": ["Height", "Build", "Hair", "Color", "Hip"],
    "contextVariable": "ExampleCharacterEditor.selectedTarget"
  }
}
```

`controls` changes existing sliders to dials, choices to radial menus, or hex-color
text fields to color controls. Dials and radial segments use scale-aware vector
arcs; narrow radial controls fall back to a regular choice picker.
`settings` adds Helpy-only settings using the same config, variables, callbacks,
visibility and authority rules. Ordinary and Helpy-only settings share a 1,024-row
limit. Radial choices support up to 12 options; larger lists use standard choices.
Circular numeric controls use the normal pointer/controller editing behavior.

### Color controls

Put new color fields in `--Helpy.settings`, where the original Mod Menu ignores
them. Existing hex text fields can use `controls: { "Color": "color" }`.

```json
"settings": [
  { "key": "SkinColor", "type": "color", "label": "Skin color", "default": "#DDB49A" },
  { "key": "AccentColor", "type": "color", "label": "Accent color",
    "alpha": true, "default": "#BFA35AFF" }
]
```

Colors use `#RRGGBB`, or `#RRGGBBAA` with `alpha: true`. Pointer and controller
users can adjust the RGB/RGBA channels; the picker also supports a hue wheel.
Changes are submitted after editing finishes. Both the UI and Lua adapter validate
colors, including imported templates. A malformed default cannot be truncated
into a valid color. Values remain strings in the mod's existing configuration and
callbacks. Convert byte channels to the material/BP color space in the owning mod.

### Import and share templates

**Import template…** opens the in-app review workflow. Choose a file or paste a
shared template; browsing a file is optional. **Templates** also exports files or
copies the current declared values. A shared template carries only declared
settings, not game pointers, a character GUID or its selected-target token.

**Templates** imports/exports JSON files or the clipboard. Import first shows a
review of current and incoming values, then requires **Apply template**. Only the
mod's declared keys are included. Unknown keys, invalid types, out-of-range
values, incompatible versions and changed targets are rejected. The adapter
validates every field before one config write, then publishes the existing
callbacks. A queued action is not reported as applied until the adapter responds.

A character editor should publish a nonempty, stable token for its selected
character through `contextVariable`, and clear it while switching targets.
Helpy checks that token and the page revision again when applying. The mod owns
selection, preview, save handling and networking. No character GUID or target
token is exported in a shared template.

The portable format is `RuneSchema.ModMenu2.Template`, version `1`, with
`mod`, `template`, `templateVersion` and `values`; `name` and `modVersion` are
optional metadata. Templates accept up to 1,024 declared keys. The 1,024-setting
capacity requires the matching local Helpy and Lua adapter builds; existing
downloads retain their 200-setting limit. Its maximum size is 256 KiB.
A future web editor can produce
the same format; no web editor is included in this candidate.

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

## Runnable external Lua example

A runnable external example is available in [the source example folder](https://github.com/gh0sted5456-us/RuneSchema/tree/pre-release/docs/examples/HelpyEditorExample).
Copy its complete `HelpyEditorExample` folder into `ue4ss/Mods`; it includes
`enabled.txt` and its root license notice. The source is in
`docs/examples/HelpyEditorExample`.

Open **Mods → Mod Menu 2 → Character Editor Example**. **Shape** demonstrates
sliders, dials and radial choices; **Colors** demonstrates RGB/RGBA controls.
Import `templates/Athletic.json` to review a complete example. The Lua callback
logs committed settings and reads its local config without executing it.

This is a separate external UE4SS demonstration, not an adapter installed into
Body Morph. It makes no actor, save or network changes. Replace its callback with
the owning mod's implementation and publish a stable selected-target token.
Without RuneSchema, its local defaults/config still load and ordinary Mod Menu
settings remain compatible. Retain the included original-material and third-party
notices when packaging it.

## Edit settings with Dragonwilds closed

Open **Mods → Mod Menu 2** in the desktop app. Helpy reads modmenu.json
or modmenu.txt pages from external UE4SS mods and enabled RuneSchema Lua
packages. Supported settings are saved to each mod's declared config file for
its next launch. JSON files can be reformatted. Lua settings are edited as
plain values in the file. Live actions and character-specific previews need
a connected game. The key and Enable Mod Menu 2 setting still control access.
