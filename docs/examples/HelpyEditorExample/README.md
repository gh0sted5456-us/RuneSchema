# Helpy Character Editor example

**Experimental: requires the matching unpublished Helpy/Mod Menu 2 candidate.**

A runnable external UE4SS mod showing sliders, dials, radial choices, RGB/RGBA
colors and reviewed shared templates. It logs callbacks and **does not edit a
character, save file or network appearance**. It is separate from Body Morph.

1. Close the game. Copy this complete folder to `ue4ss/Mods/HelpyEditorExample`.
   Keep `enabled.txt` and the root license notice.
2. Use the matching RuneSchema candidate with Mod Menu 2 enabled and its key
   installed. Disable the separate original Mod Menu before starting the game.
3. Open **Helpy → Mods → Mod Menu 2 → Character Editor Example**.
4. Adjust **Shape** or **Colors**. Use **Import template…** to browse to
   `templates/Athletic.json`, or paste its JSON, review, then **Apply template**.
   **Templates** also exports/copies the current declared values.

The ordinary `settings` work with the original Mod Menu. `--Helpy` adds only the
new controls. Colors remain hex strings: `#RRGGBB`, or `#RRGGBBAA` with `alpha:true`.
The demo reads bounded typed values from its own `config.txt` without executing
that file. It keeps local defaults/config when RuneSchema is absent.

Replace `api.OnSettingsChanged` with your own mod's callback. The mod owns actor
changes, saving and networking. Call `api.SelectTarget` when changing the selected
character; use an empty token while switching. A template never contains that
token, and Mod Menu 2 rejects stale-target imports. The provided token identifies
only the synthetic demonstration. No character preview provider is bundled.

`api.Stop` cancels its one mod-owned 250 ms loop. It does not use legacy async
timers. Stock UE4SS also cancels mod-owned delayed actions when the mod stops.

The original demo files use `license-jonesing4space.txt`. RuneSchema and the
separate Mod Menu notices are retained under `licenses/`; their MIT grants
remain independent of the demo's original-material notice.
