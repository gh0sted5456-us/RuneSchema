# Build a Helpy cache from cooked PAKs

## Windows: drop a mod ZIP to build its cache

1. Make a ZIP containing your cooked `.pak`, `.utoc` and `.ucas` files and an optional
   `background.png`. Legacy `.pak`-only mods are supported too. Nested ZIP folders
   are accepted; keep package filenames unique. Name the ZIP after the final
   RuneSchema mod folder, or pass `-ModId` explicitly.
2. Extract the authoring-tools download. Drag your mod ZIP onto
   **Build-Helpy-Cache.bat**. Windows opens a PowerShell console with progress for
   ZIP extraction, input hashing, item discovery, picture capture and publishing.
3. The builder finds an inactive Steam, Xbox/Game Pass or Epic installation,
   prepares its decoder, and selects the offline mapping automatically. There are
   no game-path, mapping or decoder prompts. Install the **.NET 10 SDK** first;
   internet is needed for first-use dependencies. Git Bash is only needed for Bash.
4. The completed **cache/** folder appears beside your mod ZIP. It contains
   `offline-helpy.rsmk-cache`, `offline-helpy-report.json`, and `mod.png` derived
   from your background. Add this folder to your RuneSchema mod ZIP alongside
   `paks/`. Your original ZIP and PAKs are unchanged.

An existing output cache is preserved as `cache.previous-<unique-id>` before the
new folder is published. A failed build leaves the current output in place.
Review the report: this is a partial presentation cache, not proof that every
cooked asset was registered or that gameplay works. Path traversal, duplicate
paths, links, ambiguous package names and incomplete trios are rejected before
extraction. Only PAK files, the chosen background and an optional `.usmap` are staged; ZIP scripts and
executables are never run.

The normal command is simply:

```powershell
.\build-helpy-cache.ps1 -ZipPath "D:\Mods\MyMod.zip"
```

For custom installations, the optional expert overrides remain available:

```powershell
.\build-helpy-cache.ps1 -ZipPath "D:\Mods\MyMod.zip" `
  -Game "D:\Games\RSDragonwilds" -Mapping "D:\Mappings\Game.usmap" `
  -Oodle "D:\Tools\oo2core_9_win64.dll" -ModId "MyMod"
```

Use `-OutputDirectory` to choose a different cache destination. Keep the target
game/server installation inactive for the whole read. A different private
installation can keep running; the existing process checks apply to the selected
source installation.

## Automatic inputs and optional mappings

The Windows BAT needs only the mod ZIP. Nested folders are accepted; a
`background.png` is optional. The Bash route uses the same automatic input resolver.

- Game selection first checks the ZIP/script folder's parents, then Steam
  libraries, XboxGames folders and Epic manifests. Active installations are
  skipped. The chosen path is printed. `-Game` / `--game` overrides this choice.
- One optional **`.usmap`** can be included anywhere in the ZIP, or placed beside
  the ZIP/script. An explicit `-Mapping` / `--mapping` takes priority. This is a
  schema file, distinct from an Unreal **`.umap`** level. A malformed or ambiguous
  supplied mapping fails clearly and retains the previous output.
- Otherwise, the builder reads the existing platform mapping without changing it.
  If unavailable, it downloads the newest published snapshot from
  [RSDW Archive](https://github.com/RSDWArchive/RSDWArchive), checks its Git blob
  fingerprint, size and schemas, and retains a verified copy for offline reuse.
  An archive snapshot can lag the installed game: review skipped assets in the
  report, or supply a matching mapping. A fingerprint check verifies the download,
  not compatibility with every game update.
- CUE4Parse's pinned decoder bootstrap runs only if its authoring copy is missing.
  `-Oodle` / `--oodle` still accepts a local decoder. DLLs in mod ZIPs are never run.

Downloaded inputs live only in
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core/cache/authoring-tools/`.
The script never installs a supplied/downloaded mapping into RuneSchema, changes
UE4SS settings, or includes it in the output cache. The report identifies the
selected mapping and its SHA-256. Players import the presentation cache without
needing mappings, the SDK or these tools.

Use the optional **offline cache builder** after cooking and arranging your mod.
It reads the installed game's native containers and your selected PAKs while
that installation is closed, then creates the mod's `cache/` folder. RuneSchema
does not run this script at startup, and the script does not load a world,
execute Blueprint gameplay, edit PAKs, register items or modify saves.

## Download the authoring tools ZIP

The October 27 Helpy source maintenance update adds **Tools > Utilities > Download authoring
tools ZIP**. This manual download saves `RuneSchema-Authoring-Tools.zip` in
Helpy's authoring-tools export folder. **Open authoring-tools folder** opens
the actual location. Extract the ZIP and start with its `README.md`.

The archive contains the Windows drag-and-drop launcher, PowerShell and Bash builders, a standalone
walkthrough, the MIT license and dependency notices. Helpy verifies the
download's size and SHA-256 before replacing an older ZIP. It does not extract
or execute the script, install dependencies or start a scan. A cancelled or
failed download keeps the previous archive. There is no startup download or
background update polling for these tools.

The tools are published independently of the DLL release. Existing builds can
still use the [single-file script](https://raw.githubusercontent.com/gh0sted5456-us/RuneSchema/main/resources/tools/build-helpy-cache.sh)
and this walkthrough; the original 0.8.0.2 ZIP does not have the new button.

**Bash route requires:** Windows, Git Bash and the .NET 10 SDK. The same
automatic game, mapping and decoder setup applies. First use restores locked
NuGet dependencies and may download the offline decoder and archive mapping.

## 1. Arrange the final mod layout

```text
MyMod/
  mod.png                         # optional banner
  paks/
    MyCookedPackage/
      MyCookedPackage_P.pak
      MyCookedPackage_P.ucas
      MyCookedPackage_P.utoc
      build-helpy-cache.sh         # put the script beside the PAK trio
```

[Save the single-file Bash script](https://raw.githubusercontent.com/gh0sted5456-us/RuneSchema/main/resources/tools/build-helpy-cache.sh)
with that exact `.sh` filename. You can move/remove the script after generating
the cache. It embeds its reviewed source and dependency lock, builds in a
temporary folder, and removes that temporary build on exit.

The builder walks upward from `paks/` to find `MyMod`. This matters: a portable
fingerprint uses portable source content and its logical mod ID; renaming the
installation folder does not invalidate the same source content.
Stage the layout above before running it; a bare trio in an arbitrary export
folder cannot establish the future RuneSchema installation layout. If needed,
use `--mod-root` and `--mod-id` to specify the final folder identity explicitly.
Generate after making your final JSONC/Lua/PAK changes. The `cache/` output and
the Bash script itself are excluded from the content fingerprint.

## 2. Run it in Git Bash

Close the Dragonwilds installation being scanned, including its dedicated
server if running. A separate installation may stay open. The tool does not
close or terminate a game. Run the script without arguments for automatic setup:

```bash
bash build-helpy-cache.sh
```

Or supply explicit paths; forward-slash Windows paths work in Git Bash:

```bash
bash build-helpy-cache.sh \
  --game 'D:/SteamLibrary/steamapps/common/RSDragonwilds/RSDragonwilds' \
  --mapping 'C:/AuthorFiles/current-build.usmap' \
  --oodle 'C:/AuthorTools/oo2core_9_win64.dll'
```

`--game` points to the **project folder containing `Content/Paks`**, not the
Steam library, `Binaries` folder, RuneSchema directory or individual PAK.
The script needs the native content to resolve dependencies such as a borrowed
vanilla texture. It mounts only the selected mod's `paks/` plus native content;
unrelated installed mods cannot silently supply its item records.

For a different working directory or distribution folder name:

```bash
bash build-helpy-cache.sh \
  --game 'D:/Games/RSDragonwilds/RSDragonwilds' \
  --mapping 'C:/AuthorFiles/current-build.usmap' \
  --oodle 'C:/AuthorTools/oo2core_9_win64.dll' \
  --mod-root 'C:/Mods/MyMod' --mod-id 'MyMod'
```

## 3. Review the generated cache

```text
MyMod/
  cache/
    offline-helpy.rsmk-cache
    offline-helpy-report.json
    mod.png                       # when an input banner was supplied
  paks/...
```

The bundle contains readable item/journal references, exact object paths,
known persistence IDs and 64-pixel thumbnails. Unknown IDs remain empty.
A banner can come from the mod root or beside the script; it is also embedded
as an inert source-art record when the PAK has an independent mod namespace.
The containing mod must still be active and own that namespace at import time.

Read the report's counts and `Issues`. All exports are marked **partial**:
the tool cannot render material/Slate-driven icons, execute construction
scripts or certify every unloaded Blueprint. Unsupported formats, missing
dependencies and unclassified exports are reported; Helpy's bounded runtime
capture remains the fallback. Source strings are currently captured as
`en-US`; another locale cannot be claimed by changing a label.

Only the builder's named output files are replaced. Other bundles/files in
`cache/` are preserved. Changed inputs, an open target game, invalid mappings
or an empty result fail without replacing the last usable bundle. Hashes of
unchanged input files are memoized in the user's temporary directory; they
are not distributed in the mod.

## 4. Package and check in Helpy

Include `cache/*.rsmk-cache` with the mod. The script and report are optional
authoring aids; players do not need Bash, .NET, mappings or Oodle to import it.
Do not distribute your mapping, decoder DLL, game PAKs or extracted game files
as part of the cache tooling.

This tool's owner-scoped bundles require the updated Helpy cache importer.
An older importer can skip them as incompatible. The new importer accepts
validated presentation independently of live registry readiness; it never
registers the supplied IDs or grants gameplay authority. Owner scoping lets a mod's own bundle remain useful
when another mod shares its plugin namespace. It does not permit a mod to
claim a borrowed vanilla item or an ambiguous overwritten asset.

Enable **Keep icons between launches**, install/enable the mod, and open
Helpy's **Icon cache** window. Automatic mod-cache import copies validated
presentation into `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/cache`.
It does not wait for a character/world or live source verification.
Fingerprints exclude the destination folder name. Each generated bundle has a
`CacheVersion`: unchanged exports retain it and changed exports increment it.
Keep the previous output bundle when generating an update so its revision can
advance. A new version replaces that source's old entries, including deletions.
Try searching for one known item. Its cached name/picture is a reference;
delivery, use, equipment and world actions still resolve current game data.

For diagnosis, explicitly **Import** the generated bundle. The importer
reports what was accepted/skipped. Running the same import again should keep
unchanged pictures rather than rewriting them.

## Build the native cache for a game update

`--base-game` excludes all mod containers, even when mods are installed:

```bash
bash build-helpy-cache.sh \
  --game 'D:/Games/RSDragonwilds/RSDragonwilds' \
  --mapping 'C:/AuthorFiles/current-build.usmap' \
  --oodle 'C:/AuthorTools/oo2core_9_win64.dll' \
  --base-game --output 'C:/CacheBuilds/current-game'
```

Add `--include-mods` to **also** build each installed RuneSchema mod with a
`paks/` folder. Each mod gets a separate output under
`<output>/mods/<ModName>/cache/`; mod content never enters the native bundle.
Uncacheable mods are reported and processing continues; any such failure
makes the command exit nonzero so an automated publication can stop for review.
Installed mods and players' Helpy caches are not changed by this mode.

The native bundle is written directly in the chosen output folder. Review it,
verify import against the matching cook, then use the separate
[publication staging helper](../CACHE-BUNDLES.md#publish-a-vanilla-bundle-to-github).
Supply the executable's exact **ProductVersion** as `--game-version`, such as
`++dominion+hotfix-CL-254097`; the marketing version alone is not that value.
Publication is an explicit maintainer step, never an automatic script upload.

See [cache ownership](../HELPY-CACHE-OWNERSHIP.md),
[bundle format](../CACHE-BUNDLES.md) and
[source/dependency notices](https://github.com/gh0sted5456-us/RuneSchema/tree/main/tools/helpy-cache).
