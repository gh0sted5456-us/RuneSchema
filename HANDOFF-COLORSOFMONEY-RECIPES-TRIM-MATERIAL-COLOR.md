# Handoff: ColorsOfMoney recipe coverage, trim restoration, and material-aware color phase shifting

## Mission

Continue the ColorsOfMoney integration without destabilizing the current RuneSchema 0.7.7.1 build.

The work has three connected goals:

1. Reference every packaged native dye and undye recipe needed by the expanded equipment set so its output `ItemData` becomes live and RuneSchema can register it.
2. Put Gold, Copper, and Silver trim armor back into the shipped package instead of using the trim-free/slim package lane.
3. Improve the texture-generation pipeline so dye colors are phase-shifted according to the material being colored, rather than applying one generic RGB tint across cloth, leather, painted metal, bare metal, trim, and emissive regions.

Do not solve this with another global Asset Registry rescan. The proven bridge is the cooked recipe reference: loading a cooked `RecipeData` also resolves its consumed and created `ItemData` references.

## Important current state

### RuneSchema

- Workspace: `C:\Users\user\Documents\ChatGPT\RuneSchema Development`
- Current experimental version: `0.7.7.1`
- Known-good package: `C:\Users\user\Documents\ChatGPT\RuneSchema Development\dist\RuneSchema-0.7.7.1-Universal.zip`
- The current installed build successfully supports entering a world, returning to the menu, and entering a world again.
- A previous re-entry crash was fixed by rooting the 16 AdditionalWeapons attack classes and releasing those roots at shutdown. Do not undo that lifetime fix.
- The one-time `SearchAllAssets(true)` experiment was removed. It added zero Asset Registry records and was associated with instability.
- Preserve the current save-pruning behavior. Pruning must only remove PersistenceIDs that fail to resolve from their applicable live registry after the startup load lane is complete.

RuneSchema already supports the safe recipe mechanism needed here. In `DragonWildsRecipeModLoader::ResolveOrCreate`, a recipe key beginning with `/` is treated as an object path: RuneSchema finds or loads the existing cooked recipe, roots it, optionally unlocks it, and does **not** construct a transient recipe or generate a replacement PersistenceID.

### ColorsOfMoney

- Project: `C:\Users\user\Documents\Unreal Projects\Coinage`
- Installed RuneSchema module:
  `D:\Program Files (x86)\Steam\steamapps\common\RSDragonwilds\RSDragonwilds\Binaries\Win64\ue4ss\Mods\RuneSchema\mods\ColorsOfMoney`
- Installed container:
  `...\ColorsOfMoney\paks\ColorsOfMoney\ColorsOfMoney_ExpandedCandidate_P.{pak,ucas,utoc}`
- Container inspection tool:
  `C:\Users\user\Documents\Unreal Projects\Coinage\Build\Tools\retoc-0.1.5\retoc.exe`

The installed UTOC currently contains:

- 40,726 listed records
- 8,609 packages under `/ColorsOfMoney/Recipes/Native/`
- 568 legacy armor dye recipes
- 568 legacy armor undye recipes
- 2,664 armor `ItemData` packages

The original station JSON references only the 568 legacy dye recipes and 568 legacy undye recipes. The PAK also contains 2,096 expanded armor items, but their native recipes were packaged without installing the corresponding RuneSchema bindings. That is why only part of the armor becomes live.

The expanded manifest already describes:

- 2,096 expanded armor variants
- 2,192 expanded weapon variants
- 4,288 expanded equipment variants total
- 16 color finishes: eight regular and eight deep

The base native-recipe package contains 8,609 recipes because the relevant generated definitions total 8,587 plus 22 core/distribution recipes. The current PAK is not missing those native recipe packages; the runtime reference/binding layer is missing.

## Phase 1: fix native recipe references safely

### Required design

Generate RuneSchema `recipes/*.jsonc` reference shards whose **keys are the full cooked recipe object paths**. Do not recreate these recipes using short names.

Use this form:

```json
{
  "/ColorsOfMoney/Recipes/Native/RECIPE_dye_colors_of_money_item_armour_t1_body_adventurers_black.RECIPE_dye_colors_of_money_item_armour_t1_body_adventurers_black": {
    "Unlock": true,
    "AddTo": [
      {
        "Table": "DT_ProcessingStationDataTable",
        "Row": "CoinageDyeStation",
        "Array": "Recipes"
      }
    ]
  }
}
```

Undye recipes use the same path-keyed form but target `CoinageRemoveADyeMagic`.

Do not repeat `Properties` for a cooked recipe unless a deliberate patch is required. With no `Properties` block, RuneSchema loads the baked recipe, retains its baked `PersistenceID`, places the existing object in the station array, and applies the requested unlock.

### Armor coverage target

Keep the original legacy station coverage and append the expanded native recipes:

- Legacy dye recipes: 568
- Expanded armor dye recipes: 2,096
- Expected total armor dye recipes exposed: 2,664
- Legacy undye recipes: 568
- Expanded armor undye recipes: 2,096
- Expected total armor undye recipes exposed: 2,664

The 2,096 expanded dye recipes consist of 1,048 regular-color armor recipes and 1,048 deep-color armor recipes. Every expanded armor variant has one undye recipe.

The generator must derive names from:

`Build\EquipmentIntegration\ColorsOfMoneyEquipmentVariants.json`

Filter `kind == "Armor"`. Convert each `dye_recipe` or `undye_recipe` leaf name to:

`/ColorsOfMoney/Recipes/Native/<recipe-name>.<recipe-name>`

Before emitting a reference, verify that both `<recipe-name>.uasset` and `<recipe-name>.uexp` exist in `Build\NativeRecipeStage`, and verify that the package name is present in the candidate UTOC manifest. A recipe must never be referenced merely because it was planned.

Shard generated files to at most 256 definitions per JSONC file and keep each file comfortably below RuneSchema's 2 MiB input limit. Use stable filenames and stable lexical ordering so rebuild diffs remain reviewable. Suggested names:

- `70-NativeArmorDyeReferences-001.jsonc`
- `71-NativeArmorUndyeReferences-001.jsonc`

It is acceptable to generalize this generator later for weapons and core recipes, but first validate the armor-only lane. Do not introduce a second runtime recipe object for any baked recipe.

### Core recipe coverage

The native-candidate staging path currently clears the ordinary RuneSchema `recipes` folder. Consequently, the next chat must also ensure core recipes such as deep-dye creation, pattern acquisition, coin conversion, and other packaged native recipes remain bound.

Two safe approaches exist:

1. Generate path-keyed reference manifests for every native recipe actually present in the selected UTOC (preferred long-term).
2. Preserve the existing core RuneSchema recipe JSON for recipes that are not being replaced by cooked native references.

Never install the existing full 21,602-recipe binding set against the 8,609-recipe slim PAK. References must match the exact candidate being shipped.

### Validation

Before installation, fail the build if any of these differ:

- emitted reference count versus selected packaged-native recipe count;
- duplicate recipe object paths;
- missing `.uasset`/`.uexp` pairs;
- reference paths missing from the UTOC package manifest;
- armor variants without a dye recipe;
- armor variants without an undye recipe;
- recipe outputs not matching their manifest `cooked_item`;
- recipes attached to the wrong station row.

At runtime, verify that recipe registration causes the expected armor `ItemData` objects to become live. Do not infer success solely from the PAK file list.

## Phase 2: restore all trim armor

The installed candidate was produced through a trim-free/slim lane. Restore Gold, Copper, and Silver trim assets, recipes, icons, localization, and material assignments.

Relevant scripts and manifests:

- `Build\Generate-GoldTrimVariantManifest.ps1`
- `Build\Generate-GoldTrimRecipes.ps1`
- `Build\Generate-GoldTrimUndyeRecipes.ps1`
- `Build\Generate-MetalTrimIcons.ps1`
- `Build\Finalize-MetalTrimStage.ps1`
- `Build\Generate-MetalTrimCapeLocalizationPlan.ps1`
- `Build\Validate-MetalTrimCapeStage.ps1`
- `Build\Package-ColorsOfMoneyExpandedCandidate.ps1`
- `Build\Stage-ExpandedRuneSchemaCandidate.ps1`

Expected trim coverage from the current validators:

- Gold Trim variants: 2,227
- Copper Trim variants: 2,227
- Silver Trim variants: 2,227
- Copper undye recipes: 2,096
- Silver undye recipes: 2,096
- Gold undye recipes: 2,096
- Metallic cape variants: 6
- Total metal-trim/cape localization entries: 10,983

The 2,227 trim variants per metal cover each armor source in its base finish plus all 16 dyed finishes. Do not collapse this to the 2,096 dyed-only set.

Build the full candidate with:

```powershell
& "C:\Users\user\Documents\Unreal Projects\Coinage\Build\Package-ColorsOfMoneyExpandedCandidate.ps1" `
  -IncludeMetalTrimAndCapes `
  -IncludeNativeRecipes `
  -CompressIoStore
```

Do **not** pass `-ExcludeTrimVariants`.

Run `Validate-MetalTrimCapeStage.ps1 -ReadbackBinary` before packaging. The validation must cover item packages, male and female mesh data, material slots, icons, recipes, undye recipes, localization, unique PersistenceIDs, and binary readback.

After producing the full PAK, regenerate the path-keyed RuneSchema recipe references against that exact PAK. The full trim candidate contains more native recipes than the current 8,609-recipe slim candidate, so stale slim bindings are invalid.

Preserve these trim semantics:

- The underlying regular/deep dye remains visible.
- Only the selected trim region changes to Gold, Copper, or Silver.
- Plumes and tabards retain their underlying dye.
- Bare hardware, non-trim accents, normal maps, and surface response remain intact.
- Undyeing trim returns the appropriate underlying dyed armor, not an unrelated base item.

## Phase 3: material-aware color phase shifting

### Objective

Replace generic whole-texture tinting with a surface-aware hue phase shift. The color operation must preserve authored shading and material response while rotating only dyeable regions toward the selected dye hue.

The existing tool is:

`Tools\ArmorTextureRecolor\Program.cs`

It already provides useful foundations:

- dominant-hue detection;
- armor classification (`Gilded`, `Metal`, `Leather`, `Robe`, `Other`);
- regular/deep tint masks;
- ORM-assisted metallic preservation;
- plume handling;
- Gilded cloth selection;
- separate regular and deep dye strengths;
- selective trim edge generation;
- distinct Gold, Copper, and Silver palettes.

Keep those behaviors, but move the color transform from a flat target-color blend to a material-aware circular hue shift.

### Source data

Build the material map from:

- `Build\EquipmentIntegration\ArmorVisualSourceMap.json`
- `Build\EquipmentIntegration\WeaponVisualSourceMap.json`
- exported BaseColor textures in `SourceTextureExports`;
- exported ORM and other surface textures in `SourceSurfaceExports`;
- `Build\EquipmentIntegration\ArmorTintMaskManifest.json`;
- explicit per-family overrides for ambiguous materials.

Use `Build\Export-EquipmentSurfaceMaps.ps1` to export non-BaseColor material textures. Do not classify pixels from filename alone when an ORM or authored mask exists.

### Mask contract

Retain the existing packed tint-mask channels:

- Red: regular dye strength
- Green: deep dye strength
- Blue: selective trim strength
- Alpha: preserve source alpha unless a separately versioned mask contract explicitly repurposes it

The ORM map is authoritative when available. In the current pipeline, ORM blue identifies metallic regions. Use that signal to prevent cloth/leather dye logic from recoloring buckles, plates, jewelry, weapon blades, and metallic fasteners.

For every material slot, assign one of these surface classes:

- `Cloth`
- `Leather`
- `PaintedMetal`
- `BareMetal`
- `Wood`
- `BoneOrHorn`
- `GemOrGlass`
- `Emissive`
- `SkinOrHair`
- `Unknown`

Store the chosen class and confidence in a generated manifest. Low-confidence or `Unknown` entries must produce a review image and require an explicit override before release.

### Phase-shift algorithm

Perform the transform in a perceptual color space such as OKLab/OKLCH where practical. HSV may remain as a compatibility fallback, but do not directly replace RGB channels with the target RGB value.

For each eligible pixel:

1. Convert source BaseColor from sRGB to linear RGB.
2. Convert to OKLab/OKLCH.
3. Determine the dominant source hue for the material region, not for the entire atlas.
4. Calculate the shortest circular hue delta:

   `delta = wrap(targetHue - sourceRegionHue, -180 degrees, +180 degrees)`

5. Rotate the pixel hue by `delta * maskStrength * materialHueFactor`.
6. Preserve perceptual lightness, then apply only a small material-specific lightness adjustment.
7. Scale chroma with a material-specific factor and clamp it to avoid gamut clipping.
8. Convert back to linear RGB and then sRGB.
9. Blend with the untouched source using the regular/deep mask channel.

Keep microcontrast, dirt, seams, wear, baked shadows, and fabric grain from the original texture. Phase shifting changes hue; it must not flatten the source into a solid color.

### Material policies

Use the following starting policies and expose them as named constants or configuration values rather than scattering magic numbers through the pixel loop.

#### Cloth and robes

- Hue rotation: 90-100% of the target delta in masked regions.
- Regular dye: retain approximately 75-90% of source chroma and use the red mask.
- Deep dye: permit higher target chroma and use the green mask.
- Preserve source lightness variation almost completely.
- Protect embroidery, goldwork, fasteners, and warm trim with ORM/mask evidence.

#### Leather

- Hue rotation: approximately 55-80% of the target delta.
- Retain a controlled warm component so leather does not become plastic.
- Preserve dark pores, edge wear, stitching, buckles, and straps.
- Reduce chroma compared with cloth, especially for regular dyes.
- Deep dye may rotate farther but must preserve leather luminance structure.

#### Painted or enamelled metal

- Hue-shift only the non-metallic paint/enamel region.
- Use ORM metallic evidence to exclude exposed metal.
- Preserve highlights and roughness cues; do not color the entire plate uniformly.
- Deep colors may strengthen the painted region but must not alter the ORM texture.

#### Bare metal

- Ordinary dyes do not phase-shift bare metal.
- Gold/Copper/Silver trim uses the blue trim mask or an authored trim mask.
- Preserve luminance and specular variation. Apply metal hue/chroma to BaseColor only; leave ORM and normals unchanged.
- Starting trim colors already used by the tool are Gold `(224,174,55)`, Copper `(188,103,54)`, and Silver `(183,194,208)`. Convert them through the same linear/perceptual pipeline before blending.

#### Wood, bone, horn, gems, glass, emissives, skin, and hair

- Preserve by default.
- Only recolor when an explicit material-slot policy opts in.
- Never derive an emissive recolor from BaseColor masks alone.

#### Gilded armor

- Dye cloth/tabard regions only.
- Preserve gold plate, chain, leather, decorative trim, and metallic response.
- Keep the existing Gilded cloth detector as a fallback, but prefer material/ORM masks.

#### Plumes

- Plumes may follow the armor dye when the manifest says `dye_plume = true`.
- Use their own region hue and mask; do not let a red plume distort dominant-hue detection for the rest of a helmet atlas.

### Regular versus deep colors

Regular and deep finishes must share a hue family but have distinct strength and chroma policies:

- Regular dye should look weathered and integrated with the original material.
- Deep dye should more fully occupy the eligible material region.
- Deep Black requires special lightness protection so texture details remain visible.
- White should reduce chroma without erasing shadows or material grain.
- Deep White must not clip large regions to flat white.

Retain the current explicit deep palette unless visual review justifies a change:

- Deep Black `(9,9,12)`
- Deep Blue `(35,78,225)`
- Deep Green `(32,177,66)`
- Deep Orange `(230,96,24)`
- Deep Purple `(157,42,199)`
- Deep Red `(213,32,49)`
- Deep White `(232,232,227)`
- Deep Yellow `(234,183,29)`

### Texture safety rules

- Modify BaseColor output only.
- Never hue-shift normal maps.
- Never overwrite ORM channels.
- Never alter opacity/cutout data.
- Never overwrite emissive textures unless a separate explicit emissive rule exists.
- Preserve texture dimensions, alpha, compression intent, addressing, and mip behavior.
- Avoid double color-space conversion; decode sRGB once and encode once.
- Clamp in the target gamut after the perceptual transform, not before it.

### Review artifacts

For each armor family, generate a contact sheet containing:

- vanilla/base texture;
- regular Red, Blue, Green, Black, White, and Yellow;
- corresponding deep variants;
- Gold, Copper, and Silver trim over at least one regular and one deep dye;
- the packed R/G/B tint mask visualized as separate grayscale panels;
- material classification and confidence.

The minimum manual review set must include:

- cloth-heavy robes;
- leather armor with buckles;
- plate armor with cloth underlayers;
- Gilded/Paladin armor;
- helmets with plumes;
- dark source textures;
- bright/white source textures;
- atlases containing both dyeable cloth and protected metal.

Reject a family if trim is recolored as cloth, buckles disappear, black loses all texture detail, white clips, deep colors flatten the material, or male/female variants diverge unexpectedly.

## Recommended execution order

1. Preserve the current installed stable build and container hashes.
2. Generate an exact UTOC package manifest for the candidate being changed.
3. Implement path-keyed cooked recipe reference generation.
4. Validate the armor-only reference lane against the current PAK.
5. Run a cold-launch runtime test and confirm all 2,664 armor outputs become live.
6. Restore Gold/Copper/Silver trim and metallic capes in the staged project.
7. Implement and visually validate material-aware phase shifting.
8. Rebuild all affected BaseColor textures, icons, material instances, item packages, recipes, and localization.
9. Run `Validate-ExpandedEquipmentStage.ps1` and `Validate-MetalTrimCapeStage.ps1 -ReadbackBinary`.
10. Package a full compressed candidate without `-ExcludeTrimVariants`.
11. Regenerate recipe references against the full candidate's UTOC—not the old slim UTOC.
12. Stage the RuneSchema module into a candidate directory and validate every JSONC shard.
13. With the game closed, back up the current install and install the candidate.
14. Run the runtime and save-safety matrix below.

## Runtime and save-safety test matrix

The candidate is not ready merely because the PAK verifies.

Test all of the following:

1. Cold boot to main menu.
2. Load an existing character and enter a world.
3. Confirm registry summaries show the expected ColorsOfMoney recipes and items.
4. Confirm representative regular, deep, and trim armor is visible and craftable.
5. Confirm Gold, Copper, and Silver trim preserve underlying dye colors.
6. Back out to main menu.
7. Re-enter the same world without restarting the game.
8. Repeat world exit/re-entry at least twice.
9. Restart the game and re-enter the same save.
10. Verify no repeated mass-prune warning occurs for valid packaged recipe/item PersistenceIDs.
11. Remove one controlled test item/mod and verify only its genuinely unresolved PersistenceID is pruned on the next startup validation.
12. Measure menu and world frame pacing. Recipe loading occurs once per game execution, not once per menu/world transition.

Failure conditions include:

- crash on world re-entry;
- severe menu or world lag;
- legitimate ColorsOfMoney IDs pruned;
- recipe references recreated with runtime-generated IDs;
- station arrays duplicated on each transition;
- recipes present but outputs missing from the item registries;
- trim recipes pointing to assets omitted from the selected PAK;
- log summaries claiming registration without resolvable live objects.

## Logging requirements

Keep runtime logging condensed. Report aggregate counts plus a small bounded sample, not thousands of per-item lines.

Useful summaries should distinguish:

- native cooked recipes found and loaded;
- recipe paths rejected because the package is absent;
- armor `ItemData` made live through recipe references;
- items registered from RuneSchema-authored content;
- items registered from mounted PAK content;
- dye/undye station placement counts;
- trim recipes and trim items registered;
- duplicate paths suppressed;
- unresolved IDs pruned after registry readiness.

## Do not do these things

- Do not reintroduce `SearchAllAssets(true)`.
- Do not poll or rescan all assets on every menu/world transition.
- Do not create short-name runtime recipes when a cooked native recipe exists.
- Do not install bindings for packages absent from the selected UTOC.
- Do not alphabetically reorder user mod load order.
- Do not make the diagnostic ledger a dependency of pruning.
- Do not prune a PersistenceID that resolves in its applicable live registry.
- Do not ship the trim-free `ExpandedSlim` candidate as the final full build.
- Do not hue-shift normal, ORM, opacity, or emissive textures with the BaseColor algorithm.

## Deliverables for the next chat

1. A deterministic generator for path-keyed native recipe reference shards.
2. Updated staging logic that installs references matching the exact PAK candidate.
3. A full trim-enabled compressed PAK/UCAS/UTOC candidate.
4. Restored Gold, Copper, and Silver trim recipes, undye recipes, icons, materials, and localization.
5. A material-aware BaseColor phase-shift implementation plus review contact sheets.
6. Validation reports proving package, recipe, item, PersistenceID, and material coverage.
7. A runtime log showing successful one-time registration and safe world re-entry.
8. A recoverable backup of the previously stable install.

