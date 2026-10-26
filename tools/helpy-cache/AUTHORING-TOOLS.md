# RuneSchema authoring tools

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

This ZIP contains the Windows drag-and-drop builder and the optional `build-helpy-cache.sh` builder. It creates
Helpy's `cache/` folder for a cooked mod, or a separate native-game cache.
Helpy downloads this archive only when requested. Extract it yourself; Helpy
does not run the script or install its dependencies.

## Before you start

- Use **Windows** and the **.NET 10 SDK** (Git Bash only for the Bash route). The first run restores
  the locked NuGet dependencies, so it needs an internet connection.
- Game, mapping and decoder inputs are resolved automatically; the optional
  overrides below are for custom installations or a specific cook.
- Close the game installation you intend to scan, including its dedicated
  server. A separate installation can stay open. The script will refuse an
  active target; it does not terminate games.

## Create a mod's cache

1. Extract this ZIP. Copy `build-helpy-cache.sh` beside your mod's cooked PAK
   trio in its final RuneSchema folder layout:

   ```text
   MyMod/
     mod.png                         (optional banner)
     paks/
       MyPackage/
         MyPackage_P.pak
         MyPackage_P.ucas
         MyPackage_P.utoc
         build-helpy-cache.sh
   ```

2. Open Git Bash in that folder. For automatic setup, run:

   ```bash
   bash build-helpy-cache.sh
   ```

   Or provide the paths explicitly:

   ```bash
   bash build-helpy-cache.sh \
     --game 'D:/Games/RSDragonwilds/RSDragonwilds' \
     --mapping 'C:/AuthorFiles/current-build.usmap' \
     --oodle 'C:/AuthorTools/oo2core_9_win64.dll'
   ```

   `--game` is the project folder containing **Content/Paks**. It is not the
   Steam library, `Binaries` folder, RuneSchema folder or an individual PAK.
   Quote paths containing spaces. Git Bash accepts forward-slash Windows paths.

3. Review the output under your mod root:

   ```text
   MyMod/cache/
     offline-helpy.rsmk-cache
     offline-helpy-report.json
     mod.png                         (when a banner was supplied)
   ```

   Read the report's counts and `Issues`. Coverage is partial: the tool reads
   cooked records and supported textures; it does not execute Blueprint logic
   or render every material-driven icon. Helpy can capture missing art later.

4. Include `cache/*.rsmk-cache` with the mod. In Helpy, keep **Keep icons
   between launches** enabled and enable the mod. Its matching cache imports
   automatically; **Icon cache > Import** also allows a manual check.
   Players do not need the script, Bash, .NET, mappings or Oodle to use it.

Portable fingerprints use source content rather than installation-folder names.
Keep the logical mod ID consistent with its final RuneSchema mod identity. Finish your JSONC/Lua/PAK changes before generating the cache.
If your staging layout is different, add:

```bash
--mod-root 'C:/Mods/MyMod' --mod-id 'MyMod'
```

## Create a native-game cache

Run this from the extracted tools folder, with the target game closed:

```bash
bash build-helpy-cache.sh \
  --game 'D:/Games/RSDragonwilds/RSDragonwilds' \
  --mapping 'C:/AuthorFiles/current-build.usmap' \
  --oodle 'C:/AuthorTools/oo2core_9_win64.dll' \
  --base-game --output 'C:/CacheBuilds/current-game'
```

`--base-game` excludes mods from the native bundle. Add `--include-mods` to
also generate separately owned bundles under `<output>/mods/<ModName>/cache/`.
This does not replace installed mods or players' Helpy caches. Review results
before distributing them; the script never uploads anything automatically.

## If something fails

- **Missing prerequisites:** install Git Bash and the .NET 10 SDK, then open a
  new Git Bash terminal.
- **Target is active:** close only the installation named by the tool and retry.
- **Cannot find Content/Paks:** correct `--game` to the project folder above it.
- **Mapping/decoder error:** use the correct current-build `.usmap` and local
  decoder DLL. Renaming an unrelated file does not make it compatible.
- **Empty or partial output:** read the report and exact error. Unsupported
  assets can remain uncached; cached references never install gameplay content.

The tool preserves the previous usable bundle on failure. Do not distribute
game PAKs, mapping files, decoder DLLs, player saves or private diagnostics in
this tools ZIP. See `LICENSE` and `DEPENDENCIES.md` for the source/dependency
notices; cached artwork and text retain their original rights.

Full walkthrough:
https://gh0sted5456-us.github.io/RuneSchema/modders/offline-helpy-cache/

Reviewed source and dependency lock:
https://github.com/gh0sted5456-us/RuneSchema/tree/main/tools/helpy-cache
