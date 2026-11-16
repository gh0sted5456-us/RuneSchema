---
channel: experimental
promote_to: DEVELOPER-TOOLS.md
---

!!! warning "Independent local development build"
    This plugin is not in the published 0.8.1.0 download. Additional developer
    actions are still being investigated. Use matching locally built files.

<span id="developer-tools"></span>

# Advanced Tooling

The native Helpy plugin lives at
`Helpy/plugins/RuneSchema.DeveloperTools/dll/RuneSchema.DeveloperTools.dll`.
It adds the optional **Tools → Advanced Tooling** capability. Disabling or
removing the plugin hides that capability. Existing plugin IDs, `devmenu.key`
and saved settings remain compatible; this is a presentation change.

The overview groups the available tools into themed cards. Its tabs provide
Items, World, Spawns, Buildings, Journal and Compatibility using Helpy's shared
controls, controller navigation and normal host/server permission checks.

**Compatibility → Scan compatibility** creates a manual, read-only report after
a world loads. **Copy report** copies the result. Reports are also exported to
AppData under `live/jobs/exports/native-contracts`. Scanning does not invoke the
listed functions, change saves or run automatically during startup.

A listed function is not proof that it works in a shipping build. Additional
health, skills, quest and camera actions still need separate implementations
and live verification.

## Activation key

The plugin is locked unless its own folder contains `devmenu.key`:
`Helpy/plugins/RuneSchema.DeveloperTools/devmenu.key`.
The file contains this SHA-256 digest of RuneSchema's MIT `LICENSE`, with LF
line endings and no BOM or trailing newline in the hashed text:

```text
c305102727d92699e78aa77b1769f65ddf4ad12468f950bc05300e32ce7e2309
```

Restart Helpy after adding the key. Missing or invalid keys prevent DLL
activation and hide its menu. Removing the key also rejects subsequent plugin
requests and the manual game-bridge inspection. It does not disable Helpy's
ordinary tools. The prototype ZIP leaves this opt-in key separate.

## Provenance

This implementation uses RuneSchema's own code. The independent mod
[Dev Menu Revived](https://www.nexusmods.com/runescapedragonwilds/mods/680?tab=description)
is a feature reference; its source, assets and branding are not included.
Its Nexus permissions currently require author permission for modification and
asset reuse and prohibit reupload. That differs from the separate Mod Menu's
MIT license used by Mod Menu 2.
