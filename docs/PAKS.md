# Mod folders and cooked content

Use `paks/<pack-name>/` beneath a RuneSchema mod to keep multiple cooked content packs separate from JSON loaders:

```text
RuneSchema/mods/GravetideStaff/
  assets/
  recipes/
  paks/
    GravetideStaff/
      GravetideStaff.pak
      GravetideStaff.utoc
      GravetideStaff.ucas
    AnotherPack/
      AnotherPack.pak
      AnotherPack.utoc
      AnotherPack.ucas
```

Include only the files produced by that pack's build; a pak-only build does not need fabricated IoStore files. Keep companion files and any signature together, with original names. These are packaged containers, not loose FModel exports or uncooked content directories.

The installed consolidated Gravetide staff belongs to `mods/Weapons`, so its corresponding organized layout would be `mods/Weapons/paks/GravetideStaff/`. Its existing flat `mods/Weapons/paks/` layout remains valid. No installed files were moved for this update.

Legacy `mods/GravetideStaff/GravetideStaff/<containers>` is also compatible. Do not install both layouts with duplicate copies of the same containers. External folders organize files; they do not change cooked `/Game/Mods/...` or plugin mount paths inside a pak. Keep JSON asset references pointed at those cooked paths.

## Discovery and order

RuneSchema's existing GetPakFolders hook registers the `RuneSchema/mods/` root with Unreal. The engine discovers pak files recursively. RuneSchema's folder validator recognizes `paks` and also permits other named subfolders containing pak/IoStore files. There is no one-pak-per-mod restriction and no new loader/scanner is required for this layout.

The AA_/ZZ_ JSON mod order is not a pak mount-priority mechanism. Native pak naming/mount rules still decide overlapping cooked content. Mod enable/disable settings govern RuneSchema JSON loaders; the current root-level pak hook does not filter mounted containers by those settings. Keep disabled content/backup containers outside the registered `mods` tree when they must not mount.

## Verification

Reviewed current `src/Loader/DragonWildsMainLoader.cpp`, WarnAboutUnknownFolders and GetPakFolders, and the locally installed Epic UE 5.6 source `Engine/Source/Runtime/PakFile/Private/IPlatformFilePak.cpp`, FindPakFilesInDirectory/FindAllPakFiles. The latter invokes IterateDirectoryRecursively. This verifies the layout mechanism; it is not a test of arbitrary pak content, signatures or game-version compatibility. See also [Epic's FPakPlatformFile API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/PakFile/FPakPlatformFile).

PakLayout1 keeps this engine discovery unchanged. Unknown-folder warnings now point authors to `paks/<pack-name>/`; legacy-folder validation handles filesystem errors and accepts real container files. `/appearance` is not a loader folder: use the appearance fields documented in [Appearance](APPEARANCE.md).
