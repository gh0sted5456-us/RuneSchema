---
channel: experimental
promote_to: VORTEX-INSTALLATION.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# RuneSchema and Vortex

Development builds produce separate Vortex layouts for Steam/Win64 and Game
Pass/WinGDK. They contain the same framework and do not replace UE4SS.

1. Close Dragonwilds and stop its server before deployment.
2. Install the appropriate UE4SS build separately.
3. Choose the RuneSchema Vortex archive for that installation.
4. Install and deploy with Vortex 1.14 or newer. Explicit copy instructions use
   the game-root Engine Injector mod type.
5. Verify `main.dll` is under
   `RSDragonwilds/Binaries/Win64/ue4ss/Mods/RuneSchema/dlls`, or the corresponding
   `WinGDK` path on Game Pass. Enable `RuneSchema : 1` in UE4SS's `Mods/mods.txt`.

The only ordinary PAK-folder addition is
`RSDragonwilds/Content/Paks/~mods/RuneSchema-README.txt`. It explains where the
framework lives. It does not load content. Plugin containers stay together
inside `RuneSchema/plugins`.

Vortex's file overrides **do not apply to collection installations**. Collection
authors must verify their extension's mod type and destination mapping
separately. A live Vortex deployment and collection workflow have not yet been
certified. See the [official override documentation](https://github.com/Nexus-Mods/Vortex/wiki/File-Based-Override-Instructions).

## Incomplete RegistryBridge triplets

Current framework layout:

```text
RuneSchema/plugins/
  RuneSchema.RegistryBridge/paks/RegistryBridge/
    RegistryBridge_P.pak
    RegistryBridge_P.ucas
    RegistryBridge_P.utoc
  RSNetworking/paks/Networking/
    Networking_P.pak
    Networking_P.ucas
    Networking_P.utoc
```

Older installations may have a retired copy at `RSNetworking/paks/RegistryBridge`.
The development loader prefers the validated, enabled shared provider and
ignores that duplicate. If the shared provider is disabled or invalid, the
legacy copy must still be complete to load.

The reporter's generic `DID-NOT-START ... Incomplete pak/ucas/utoc triplet`
message did not record observed filenames, so the exact deployment fault is
unconfirmed. New validation compares stems without case sensitivity, ignores
metadata-only folders, and lists missing parts and observed container names.
A rejected optional plugin emits [RS-PLUG-009](../ERROR-CODES.md#rs-plug-009);
the core and independent plugins continue. Dependent content may be unavailable.

Ordinary NTFS hardlinks are supported. Payload symlink/reparse validation is
unchanged. Preserve settings and add-ons, repair staging, then redeploy the
complete plugin. Do not delete a random member of its triplet.

## Build the archives

The normal build adds `RuneSchema-<version>-Vortex-Steam.zip` and
`RuneSchema-<version>-Vortex-GamePass.zip`. For an existing clean payload:

```powershell
./build/package-vortex.ps1 -RuneSchemaRoot ./dist/clean/RuneSchema `
  -OutputPath ./dist/RuneSchema-Vortex-Steam.zip -Platform Steam
```

Use `GamePass` for WinGDK. The packager rejects runtime/user data, nonempty mod
folders, reparse entries and incomplete containers. It leaves the input intact.
The standalone Helpy executable remains an optional separate artifact.
