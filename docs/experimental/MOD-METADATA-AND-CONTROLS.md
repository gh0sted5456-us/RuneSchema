---
channel: experimental
promote_to: MOD-METADATA-AND-CONTROLS.md
---

!!! info "Pre-release · 0.8.1.0"
    Use the matching 0.8.1.0 Helpy and RuneSchema files.
    [Candidate download and limits](PRE-RELEASE.md).

# Mod metadata and UE4SS controls

## Edit a mod's ID.txt

In **Mods → RuneSchema mods**, right-click an installed mod and choose
**Create / edit ID.txt**. Helpy preserves the text, comments and unknown fields.
It refuses to overwrite a file changed after the editor opened. Previous ID.txt
contents are kept in this runtime's AppData metadata folder.

For a mod without author metadata, local update tracking writes a `.helpy-source`
marker beside ID.txt and keeps a matching AppData record. A changed fingerprint
can create a cache-update notification even when no version was supplied. Small
files are hashed; large binaries use file size and modification time as update
hints. This is change detection, not integrity verification. An author-supplied
replacement ID.txt ends local tracking instead of being overwritten.

Deployment information names Vortex or Mod Organizer 2 only where a deployment
manifest, mod metadata or the current process provides evidence. Having a manager
installed does not establish which manager owns a particular mod. A manager may
replace local edits during deployment; keep author metadata in the managed source
when needed. Helpy never edits the manager's staging database.

## Enable or disable UE4SS loaders

**Mods → UE4SS mods** uses cards by default. An enabled entry has a gray badge;
a disabled entry has a black badge. **Compact** restores the table.

Enable **Manage external UE4SS mods** to allow Helpy to edit this installation's
`ue4ss/Mods/mods.txt`. Disabling an external mod also removes its `enabled.txt`
override after backing it up in AppData. Changes take effect on the next launch.
The inventory reports configured state separately from observed loader health.

The **RuneSchema loaders** tab controls only a parent mod's `ue4ss/` loader.
Helpy writes `<mod>/ue4ss/status.txt`:

```text
0
```

`0` disables that entire UE4SS loader, including its child packages. `1` enables
it; an absent file also means enabled. The parent mod's PAK and JSON loaders keep
their normal enabled state. This does not unload running scripts. Unsupported
native DLL packages remain marked as unsupported rather than being activated.

## Mod tips

An optional `<mod>/cache/tips.json` travels with a mod's cache:

```json
{
  "Format": 1,
  "Tips": [
    "Open the build menu to find this mod's structures.",
    "Keep a clear area around your siege equipment."
  ]
}
```

ID.txt also supports a `[Tips]` section with `Tip=...` lines or plain tip lines.
Both sources are combined and deduplicated. Loading hints rotate every eight
seconds and use only enabled, present mods. The passive loading indicator works
when Helpy's main interface is configured as a regular standalone window too.
Invalid optional tips are ignored without blocking the mod's other metadata.
