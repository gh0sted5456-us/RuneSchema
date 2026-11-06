---
channel: experimental
promote_to: COMPATIBILITY.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Platform and multiplayer support

## Linux hosting, Wine and Proton

!!! warning "Compatibility investigation — not yet verified on Linux"
    The 0.8.1.0 candidate is built and tested on Windows. It has not passed a real
    Wine/Proton client or hosted-server test. Do not interpret the presence of
    Windows DLLs or Helpy.exe as native Linux server support.

Jagex supplies both Windows and Linux dedicated servers. Its official container
launches a Linux server script. Stock UE4SS currently documents native Linux
builds as unsupported ([Jagex hosting guide](https://runescapedragonwilds.help.jagex.com/hc/en-gb/articles/45365343055249-Dedicated-Servers-How-to-Guide),
[official container](https://github.com/runescape/rsdw-dedicated),
[UE4SS build support](https://docs.ue4ss.com/)).

A Windows x64 Dragonwilds server running inside Wine is the relevant path for
the existing RuneSchema DLLs. A native Linux executable cannot load these PE DLLs
as a normal UE4SS mod. It would require a compatible native loader, Linux builds
of RuneSchema and its native plugins, and platform-specific game bindings.
Before deploying on Shockbyte, confirm whether its service runs **Binaries/Win64**
through Wine or **Binaries/Linux**, and whether the provider permits choosing the
Windows server executable and supplying the UE4SS proxy override.

Users have reported loading UE4SS and RuneSchema 0.8.0.1 on their hosts. That is a
useful baseline for the same Windows-under-Wine configuration, rather than proof
that the newest renderer, bridge and host setup are equivalent. Compared with
0.8.0.1's in-process Direct3D 11 UI, the current game-side bridge delay-loads its
optional graphics libraries and leaves the desktop renderer/SDL in Helpy.exe.
An import-contract test prevents the bridge from acquiring those eager UI
dependencies; noninteractive window stations do not auto-launch the GUI.

For a Wine test, keep the game, UE4SS, RuneSchema DLLs and Helpy in the **same Wine
prefix and user session**. Helpy's bridge uses Windows-local IPC; a separately
started prefix is not a remote client. AppData caches live inside that prefix.
The UE4SS proxy must load as native: its maintainers describe
`WINEDLLOVERRIDES="dwmapi.dll=n,b"` for the dwmapi proxy. Match the override to the
proxy actually supplied by your UE4SS package; do not rename RuneSchema's DLLs
([UE4SS maintainer guidance](https://github.com/UE4SS-RE/RE-UE4SS/issues/1189)).

Helpy is a graphical application. Its desktop window needs a working display and
Direct3D 11 translation; its exclusive-fullscreen overlay additionally needs
verified cross-process GPU sharing and input capture. The desktop renderer has
a composition fallback, but successful desktop rendering does not prove native
overlay support. A headless hosted server should not require Helpy's graphical
window to register content. The optional cache authoring worker also needs Windows
PowerShell and the Windows .NET SDK; Wine alone does not install these prerequisites.

The required test is a disposable Windows server under Wine: confirm UE4SS and
all RuneSchema native plugins load, registration matches, a Windows client joins,
item delivery/teleport requests complete, and shutdown releases the bridge. Test
Helpy cache browsing separately with no game, then connection and reconnect in the
same prefix. Record Wine/Proton, UE4SS, game and graphics-driver versions with logs.
Provider file upload access alone does not establish permission to run this setup.

### 0.8.1.0 Wine compatibility packaging

The source includes a packaging-only Wine path. Normal Windows packages
retain UPX compression. The Wine variant keeps ZIP compression and removes UPX
from the native DLLs, so the platform loader resolves their imports directly.
Every source component and license notice is retained. Standard framework paths
are normalized to `ue4ss/Mods/RuneSchema/dlls/main.dll` casing.

After assembling and licensing the **complete matching runtime ZIP**, maintainers
can create its separate Wine variant on the Windows build machine:

```powershell
./build/package-wine-compat.ps1 -SourceArchive ./RuneSchema-0.8.1.0.zip -DestinationArchive ./RuneSchema-0.8.1.0-Wine.zip
```

The script uses the project's existing UPX tool when a source DLL is packed;
`-UpxPath` can select its configured location. It preserves the source archive
and refuses to overwrite an existing destination. This is a packaging operation,
not installation, a UE4SS upgrade or a change to saves/settings.

The Wine package includes a read-only Python 3 checker:

```text
python3 /path/to/RuneSchema/tools/proton/check-ue4ss-abi.py /path/to/RuneSchema/dlls/main.dll /path/to/ue4ss/UE4SS.dll
```

It checks the actual exported names/ordinals, without loading either DLL and
without rejecting a host based on a version label. Missing exports identify a
host that cannot satisfy the core's imports. Matching exports do not prove C++
layout compatibility, Wine's system libraries or gameplay support.

The 0.8.1.0 candidate passes this export check against the working Windows host.
Synthetic checker tests and package-preservation/unpacking tests also pass.
**A real Wine/Proton load remains required before this variant is called a
confirmed fix or supported release.** A core-only test overlay must be identified
as such; it is not a replacement for the complete matching runtime package.

### Core DLL fails with `0x45A`

`0x45A` is Windows error 1114, `ERROR_DLL_INIT_FAILED`. It does not identify the
failing dependency or initializer. A fresh UE4SS log showing this while loading
`RuneSchema/dlls/main.dll` establishes a loader failure, not a registry or world
cleanup failure ([Microsoft error reference](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1000-1299-)).

The official 0.8.0.3 core DLL is UPX-packed and passes UPX's integrity test. A
controlled Windows fixture reproduced error 127 for an unpacked DLL with a
missing dependency export, but error 1114 when UPX deferred that same missing
export into its loader. UPX's loader returns failure if `GetProcAddress` cannot
resolve such an import
([upstream loader](https://github.com/upx/upx/blob/devel/src/stub/src/amd64-win64.pe.S)).
This explains why an import mismatch can appear as `0x45A`; it does not prove
the cause of a particular Proton report or establish that compression is broken.

Compared with the official 0.8.0.1 core, 0.8.0.3 adds seven UE4SS imports, including
`UFunction::GetFuncPtr`, `FWeakObjectProperty::StaticClass`, `Lua::execute_string`,
FURL construction/destruction and `RegisterLoadMapPreCallback`. The loaded host
must export the exact imported APIs. "Latest UE4SS" alone is insufficient to
compare two installations.

Record the exact Proton version and the UE4SS version/commit from its fresh
startup banner. Enable `PROTON_LOG=1` using
[Proton's runtime options](https://github.com/ValveSoftware/Proton#runtime-config-options)
and keep the existing proxy override. A controlled unpacked copy of the **same**
RuneSchema version can expose a named missing import or isolate a packed-loader
interaction. Such a diagnostic copy is not a complete runtime package or a
confirmed fix. Preserve the installed DLL outside the mod directory and restore
it after the test. Avoid introducing extra `mods`/`Mods` symlinks while isolating
the loader failure; use the directory actually configured by UE4SS.

RuneSchema uses one mod format for Steam, Epic, and Game Pass. You can use the same JSON definitions across stores. The game program is different between
store versions, so the correct RuneSchema and UE4SS package still matters.

## Character saves

The useful character information is the same kind of game data on each store,
but the stores keep it in different places. Steam and Epic use normal local save
files. Game Pass uses the Xbox save container.

RuneSchema does not move files between stores and does not directly rewrite an
Xbox container. It checks the character information while the game is loading
it, then lets the game and the store save normally.

## Multiplayer

Every player should use the same gameplay-changing mod set when a mod adds:

- items or equipment;
- recipes or stations;
- quests, dialogue, or events;
- enemies, drops, or vendors;
- magic or equipment behavior.

Visual-only changes may not be needed by a dedicated server, but every client
that must display a new mesh, icon, material, sound, or interface element needs
the matching PAK content.

The host or server decides gameplay results. Clients still need matching item
and recipe information so those results can be shown and used correctly.

Building IDs have their own exchange during joining. RuneSchema includes them
in the first server announcement and retries the client acknowledgment up to
16 times, two seconds apart, while the owned player connection becomes ready.
Retries stop after a matching reply and do not repeatedly reload quest or
world snapshots. A temporary `BUILDINGS-PENDING` message means this exchange
is still finishing; it does not establish that your mod lists differ. The
`BUILDINGS-READY` message confirms the exchange completed. Each placement
still checks the selected ID against both building lists; an actual mismatch
remains blocked with `RS-NET-003`.

## Game updates

A game update can move content, rename fields, or change how a feature starts.
After an update:

1. keep important characters backed up;
2. confirm that RuneSchema supports the new game build;
3. test with a small mod set first;
4. read the log for unavailable features or missing content paths;
5. test both first entry and re-entry before continuing normal play.

## UE4SS scheduling and API audit

Unpublished maintenance candidate; use matching Core, Helpy and Lua files.

- The optional ModActor loader schedules its initial world check with
  `ExecuteInGameThreadWithDelay(0, ...)`. If the host lacks delayed actions or
  rejects scheduling, it reports that initial check as deferred and retains
  its load-map hooks. It does not fall back to an async timer.
- Relay, mapping, HUD and Mod Menu helpers already use game-thread delayed
  actions. RuneSchema's shipped Lua helpers do not call `ExecuteWithDelay`,
  `ExecuteAsync` or `LoopAsync`.
- The Core implements the current single-hook-pointer `on_lua_start` callback.
  Its deprecated vector overload is deliberately retained for older supported
  hosts that invoke only that callback. Removing it would break their package
  discovery; installation remains idempotent if a host invokes both.
- The audit found explicit `FNAME_Find`/`FNAME_Add` choices and `.Get()` access
  in the inspected lookup/object-pointer paths; no SDK upgrade is bundled.
  Unreal's `IncludeDeprecated` property flag is intentional reflection behavior,
  not a deprecated UE4SS API.

These changes follow UE4SS's [delayed-action API](https://docs.ue4ss.com/dev/lua-api/global-functions/delayedactions.html),
[callback overloads](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/UE4SS/include/Mod/CppUserModBase.hpp)
and [upgrade guide](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/docs/upgrade-guide.md).
Scheduler tests cover missing/failed scheduling, deferred execution and map-load
fallback. These checks do not replace live Steam/Game Pass or Lua-restart tests.
