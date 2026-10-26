# RuneSchema authoring tools

This ZIP contains the optional `build-helpy-cache.sh` cache builder. It creates
Helpy's `cache/` folder for a cooked mod, or a separate native-game cache.
Helpy downloads this archive only when requested. Extract it yourself; Helpy
does not run the script or install its dependencies.

## Before you start

- Use **Windows with Git Bash** and the **.NET 10 SDK**. The first run restores
  the locked NuGet dependencies, so it needs an internet connection.
- Supply a `.usmap` matching the platform and installed game build, plus your
  own local `oo2core_9_win64.dll`. These are not included in this ZIP.
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

2. Open Git Bash in that folder. For interactive prompts, run:

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

The final owner folder name and relative PAK layout matter to the cache
fingerprint. Finish your JSONC/Lua/PAK changes before generating the cache.
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
