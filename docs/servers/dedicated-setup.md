# Install RuneSchema on a dedicated server

Stop the dedicated server completely before changing its UE4SS installation,
RuneSchema DLL, PAKs, or mod list. Preserve the existing installation and
`RSDragonwilds/Saved/SaveGames` first.

## 1. Find the server's UE4SS folder

Use the folder next to the dedicated-server executable, normally:

```text
RSDragonwilds/
  Binaries/
    Win64/
      RSDragonwildsServer-Win64-Shipping.exe
      ue4ss/
        Mods/
```

Use the **server** folder, not the Steam client's `Win64` folder and not the
Game Pass client's `WinGDK` folder. If UE4SS is not installed for that server
build, set it up before copying RuneSchema.

## 2. Install the single package

Extract the [current RuneSchema ZIP](../CURRENT-RELEASE.md) so the server has:

```text
ue4ss/Mods/RuneSchema/
  dlls/main.dll
  mods/runeschema.txt
  plugins/plugins.txt
  scripts/main.lua
  settings/settings.ini           # legacy settings.jsonc is supported
```

Keep the package's bridge PAK triplets intact. Copy your server's existing
`mods` folder, `runeschema.txt`, and settings aside before an update, then
restore your chosen configuration without mixing DLLs from old releases.

In UE4SS `Mods/mods.txt`, enable RuneSchema itself:

```text
RuneSchema : 1
```

## 3. Install gameplay mods deliberately

Put each RuneSchema mod under `RuneSchema/mods/<ModName>`. List each enabled
folder in `RuneSchema/mods/runeschema.txt`, with a required base mod before a
dependent add-on:

```text
BaseContent : 1
ServerExpansion : 1
DisabledTrial : 0
```

Do not put a second copy of the same PAK in the game's `Content/Paks` folder.
`0` stops RuneSchema from loading the folder, but cannot stop another copy
installed outside it. Read the mod author's server/client requirements and
[align the clients](mod-parity.md).

## 4. Start and verify

Start the dedicated server and inspect **that server's** `UE4SS.log`:

1. Confirm RuneSchema's version and successful startup.
2. Confirm the enabled mod names and mounted PAK packages.
3. Confirm required bridge plugins loaded.
4. Check item, recipe, building, quest, and combat registration summaries
   relevant to your mod list.
5. Resolve the first failure before using an important world.

The RuneSchema authoring tab can be present on a server, but a headless
server does not render a player UI. Do not treat absence of a visible panel
as evidence that its loaders failed.

## Edit DedicatedServer.ini in the server's Helpy

On a server with an interactive desktop, open **Server / Co-op** in its local
Helpy window. Dedicated configuration is available on that server; the optional
client co-op workspace does not provide remote server-configuration access.

The default candidate is `RSDragonwilds/Saved/Config/WindowsServer/DedicatedServer.ini`.
Confirm it is the active profile; for a custom `-UserDir` or save directory,
select that profile's existing `WindowsServer/DedicatedServer.ini` explicitly.
The editor never creates a guessed configuration file. Select/reload discards
unsaved edits only after confirmation.

Existing sections, unknown keys, repeated entries, BOM, comments and line
endings are retained. Passwords are masked; native player/admin/ban records
remain redacted even if you reveal scalar secrets. Only the five documented
scalar settings are currently editable. UTF-16 and oversized files must be
edited externally.

Jagex warns that active-server INI edits can be lost. **Stage reviewed changes**
therefore writes a private adjacent change file, not the active configuration.
Copy the offline apply command, stop the server, then run it on the server
computer. The bundled `Apply-DedicatedServerSettings.ps1` refuses to run while
a Dragonwilds process exists. It compares only the edited scalar baselines,
preserving the latest native bookkeeping written during shutdown, and saves
atomically with an adjacent backup. Changed/missing/duplicate edited keys are
rejected. Start the server afterward. Pending changes and backups may contain
passwords: keep them private. Neither staging nor applying grants live rights.

### Verified settings and observed fields

Checked October 16, 2026 against the
[official Jagex server guide](https://dragonwilds.runescape.com/news/how-to-dedicated-servers),
the previously supplied native configuration sample (key names only), and the
installed native mapping. These are different evidence levels: a serialized
property or an INI entry does **not** by itself prove a supported editable
configuration option. There is no claim that a sample exhausts future builds.

The official settings belong under:

```ini
[/Script/Dominion.DedicatedServerSettings]
OwnerId=YOUR_ACCOUNT_PLAYER_ID
ServerName=Your server name
DefaultWorldName=Your new world's default name
AdminPassword=CHOOSE_A_PRIVATE_ADMIN_PASSWORD
WorldPassword=
```

This is a minimal reviewed example, not a file to copy over an existing server's
identity or player records. Required fields must be filled deliberately.

| Field | Evidence / behavior | Helpy handling |
| --- | --- | --- |
| `OwnerId` | Official: required owner player/account identity | Editable scalar; never substitute a character GUID |
| `ServerName` | Official: required server display name | Editable scalar |
| `DefaultWorldName` | Official: required default-world name for creation | Editable scalar; not an existing-world selector |
| `AdminPassword` | Official: required management authentication | Masked editable scalar |
| `WorldPassword` | Official: optional join-password override | Masked editable scalar |
| `ServerGuid` | Sample + reflected `Guid`; native server identity | Runtime managed; preserve |
| `PlatformPolicy` | Sample + reflected `EDomPlatformFamily` | Read-only until values and config binding are validated |
| `bAllowSendingCrashDumps` | Sample + reflected Boolean | Read-only; not a player privilege |
| `KnownPlayerList` | Sample + reflected native player array | Redacted and preserved; can embed `LastAdminPassword` |
| `AdminList` | Reflected array of `DediServerKnownAdminsIds`; not present in inspected sample | Redacted, preserved; not fabricated |
| `BannedUserList` | Reflected array of `DediServerBannedUser`; not present in inspected sample | Redacted, preserved; not fabricated |
| `bCanSaveAllSections` | Observed in sample; engine/config plumbing | Read-only; not a player permission |
| `Port` | Observed in sample; this INI binding not established | Preserve; do not advertise as a confirmed INI control |
| `PublicPort` | Observed only in sample | Preserve, read-only; support unverified |
| `RESTAPIPort` | Observed only in sample | Preserve, read-only; no working REST API inferred |
| `RESTAPIEnabled` | Observed only in sample | Preserve, read-only; no API endpoint enabled by Helpy |
| `RCONEnabled` | Observed only in sample | Preserve, read-only; no RCON support inferred |
| `PublicIP` | Observed only in sample | Preserve, read-only; support unverified |

The inspected sample has 16 distinct keys. The two native list properties add
useful type information but are not evidence of another two required INI
lines. World gameplay rules and command-line/Engine.ini controls are separate
from these settings. Do not pad a "full INI" with invented controls.

### Native roles and privilege bits

The official guide distinguishes owner, admin and regular players: an owner
can ban/unban players online or offline; an admin can ban online regular
players, not unban. Admin authentication is described through the management
password, not a RuneSchema GUID whitelist.

The inspected mapping defines `EPlayerPrivilege` as follows. This is a byte
bitmask, not an editable permission schema or proof that every action is
authorized by one bit in every runtime context.

| Flag | Bit value | Meaning from the native enum |
| --- | ---: | --- |
| `None` | 0 | No privilege bits |
| `Admin` | 1 | Native admin flag |
| `Build` | 2 | Building privilege |
| `OpenChests` | 4 | Chest-opening privilege |
| `Chat` | 8 | Chat privilege |
| `Owner` | 128 | Native owner flag |

For example, `14 = 2 + 4 + 8` is **Build + OpenChests + Chat, not Admin**.
`KnownPlayerList.UserId` is an account-backed `UniqueNetIdRepl`; character GUIDs
are different identities. Raw tuple editing and nonzero-mask guesses must not
grant authority. RuneSchema's item-grant/spawn/teleport allowlists remain
separate and require their existing authoritative execution-time checks.
See the [native identity/role integration notes](../HELPY-ADMIN-DESIGN.md).

## 5. Join-test with a matching client

Try one client with the required gameplay PAKs. Verify item use, crafting,
world entry, leaving, and rejoining. A dedicated server's successful mount
does not install missing meshes, sounds, or widgets on the client.

If a LogicMod unexpectedly starts on the dedicated server, follow the
[LogicMod troubleshooting steps](../TROUBLESHOOTING.md#a-logicmod-starts-on-a-dedicated-server).
