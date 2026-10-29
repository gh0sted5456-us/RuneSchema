---
hide:
  - toc
---

# For players

<p class="rs-hub-lead">Install RuneSchema, add the mods you want to try, and use Helpy to browse content and manage your setup. You can play with compatible mods without writing JSON or cooking a PAK.</p>

<div class="rs-hub-links">
  <a href="../CURRENT-RELEASE/">Download RuneSchema<small>One package for supported game installs.</small></a>
  <a href="../INSTALLATION/">Install or update<small>Find the right UE4SS folder and check startup.</small></a>
  <a href="manage-mods/">Manage mods<small>Enable, disable, and order add-ons.</small></a>
  <a href="../TROUBLESHOOTING/">Troubleshoot<small>Start with the first failed loading step.</small></a>
</div>

## Before you change a mod list

Back up any character or world you care about. Mod content may be stored in either one, and turning a mod off does not automatically reverse its effects. Read [save safety](../SAFE-SAVE-AND-LEDGER.md) before removing gameplay content.

RuneSchema itself is enabled in UE4SS `Mods/mods.txt`. Individual add-ons are enabled in `RuneSchema/mods/runeschema.txt`. These are different lists.

## Quick Settings

Open **Home → Quick settings** for a centered panel. Expand **Content & cache**,
**Performance**, or **Game launch & saves** to work with one group at a time.
The panel scrolls on smaller windows and has a dedicated Close button.

**Game launch & saves** includes **Blanket save cleanup (fallback)** and an
activation choice: **Next character/world entry** or **Next game launch**.
Blanket is off by default, selecting only the character/world being loaded;
selection and preview alone do not authorize cleanup. Existing explicit
Blanket choices are preserved. Read
[cleanup modes and backup limits](../SAFE-SAVE-AND-LEDGER.md#choose-when-cleanup-runs)
before changing the policy. Core quick settings are also available through the
optional [Mod Menu page](../SAFE-SAVE-AND-LEDGER.md#optional-mod-menu-settings-page).

## Helpy frame rate

Open **Home → Quick settings → Performance**. Enable **Show game FPS in game**
for a small counter over Dragonwilds in windowed or borderless mode. **F10** toggles
it by default; change **FPS shortcut** to another unused key. Helpy's main window
can stay closed. Hiding the overlay stops its timing requests and drawing.

Enable **Show FPS in Helpy's window** for both counters beside its window controls.
On a dedicated-server desktop,
the game counter is labelled **Server tick**. It measures engine ticks rather than
GPU presentation; the Helpy counter measures its own rendered frames.

**Helpy FPS limit** adjusts Helpy between 15 and 60 FPS (30 by default). It does not
change Dragonwilds' frame-rate limit. Unfocused Helpy renders at up to 10 FPS;
hidden or minimized Helpy stops rendering. **Counter refresh** offers 0.5, 1 or
2 seconds. Game timing is averaged once per second by an existing engine callback,
and Helpy reads that cached value only while the counter is visible. `--` means
timing is unavailable or stale, including when paired with an older core DLL.

These preferences persist in the platform's Helpy `theme.jsonc` under `runtime`:
`showFps` and `showGameFps` (both default `false`), `fpsHotkey` (default `"F10"`),
`helpyFpsLimit` (default `30`) and `fpsRefreshMs` (default `1000`).
Lowering the limit reduces Helpy's rendering work; it does not
prove that every source of a game hitch has been removed.

## Other useful guides

- [Steam, Epic, Game Pass, and co-op](../COMPATIBILITY.md)
- [What the optional bridges do](../OPTIONAL-BRIDGES.md)
- [File guide for CurseForge and Nexus](../MOD-HOSTS-AND-GETTING-STARTED.md)
