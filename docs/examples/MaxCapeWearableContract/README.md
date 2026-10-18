# MaxCape explicit wearable contract

Add the contents of `assets/` and optionally `registry/` to the **existing**
`RuneSchema/mods/MaxCape/` folder while Dragonwilds is fully closed. Keep
MaxCape's original assets, recipes, raw table patch, PAK and Lua package. This
is not a standalone mod and has no new ID or clone declaration.

The asset patch uses the real authored clone alias from `10-MaxCape.json`,
which the inspected Game Pass log resolves to
`/Game/RuneSchema/MaxCape/Items/maxcape.maxcape`. Both mesh-data references match
the installed MaxCape definition exactly. It preserves the PersistenceID,
InternalName, effects, icon, skill gates and recipe. The optional registry
document makes the native mesh dependencies explicit and type-checked.

## What this does not fix or prove

The 2026-10-15 Game Pass log already confirms MaxCape's identity registration
and wearable enumeration. The later `[WEARABLE-BRIDGE][NOT-ENROLLED]` warning is
from the native hide-cache rebuild confirmation, not a missing mod declaration.
Adding `$WearableBridge` is an explicit enrollment request, **not a confirmed
fix** for that warning. It does not clear failed native attempts, insert opaque
cache values or bypass the game's builder. A separate native bridge diagnosis
is still needed if the warning persists after a clean restart/re-equip test.

The custom MaxCape design still comes from its Lua dynamic-material logic and
textures in its PAK. This contract does not replace that logic with JSON or
change the shared vanilla Fire Cape mesh/material assets.

## Verification and testing

The JSONC files validate against the current assets/registry schemas. Their
target and mesh references are compared directly with the installed definition;
that is authoring validation, not live native-cache verification. No installed
files or DLLs were changed when preparing this example.

After applying while closed, restart, load a test world, equip/re-equip Max Cape
and inspect its appearance and the new log. If it is still reported
NOT-ENROLLED, report that accurately rather than treating the schema directive
as proof of successful visual enrollment. Remove only these two new files to
undo the example; it does not require a save edit.
