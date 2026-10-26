# Build a Helpy cache from cooked PAKs

Use the optional **offline cache builder** after cooking and arranging your mod.
It reads the installed game's native containers and your selected PAKs while
that installation is closed, then creates the mod's `cache/` folder. RuneSchema
does not run this script at startup, and the script does not load a world,
execute Blueprint gameplay, edit PAKs, register items or modify saves.

## Download the authoring tools ZIP

The next Helpy maintenance build adds **Tools > Utilities > Download authoring
tools ZIP**. This manual download saves `RuneSchema-Authoring-Tools.zip` in
Helpy's authoring-tools export folder. **Open authoring-tools folder** opens
the actual location. Extract the ZIP and start with its `README.md`.

The archive contains the reviewed single-file Bash builder, a standalone
walkthrough, the MIT license and dependency notices. Helpy verifies the
download's size and SHA-256 before replacing an older ZIP. It does not extract
or execute the script, install dependencies or start a scan. A cancelled or
failed download keeps the previous archive. There is no startup download or
background update polling for these tools.

The tools are published independently of the DLL release. Existing builds can
still use the [single-file script](https://raw.githubusercontent.com/gh0sted5456-us/RuneSchema/main/resources/tools/build-helpy-cache.sh)
and this walkthrough; the original 0.8.0.2 ZIP does not have the new button.

**Requires:** Windows, Git Bash, .NET 10 SDK, a `.usmap` matching the installed
game build, and your own local `oo2core_9_win64.dll`. First use restores locked
NuGet dependencies. The decoder/mapping files are not bundled or downloaded.
Use the mapping for the platform and game build you are reading; a mapping
from another storefront/update is not automatically interchangeable.

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
fingerprint includes the owning folder name and the PAKs' relative layout.
Stage the layout above before running it; a bare trio in an arbitrary export
folder cannot establish the future RuneSchema installation layout. If needed,
use `--mod-root` and `--mod-id` to specify the final folder identity explicitly.
Generate after making your final JSONC/Lua/PAK changes. The `cache/` output and
the Bash script itself are excluded from the content fingerprint.

## 2. Run it in Git Bash

Close the Dragonwilds installation being scanned, including its dedicated
server if running. A separate installation may stay open. The tool does not
close or terminate a game. Run the script without arguments for prompts:

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
An older importer can skip them as incompatible; it must not bypass ownership
or relabel their content. Owner scoping lets a mod's own bundle remain useful
when another mod shares its plugin namespace. It does not permit a mod to
claim a borrowed vanilla item or an ambiguous overwritten asset.

Enable **Keep icons between launches**, install/enable the mod, and open
Helpy's **Icon cache** window. Automatic mod-cache import checks the active
owner and content fingerprints and copies usable presentation into AppData.
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
