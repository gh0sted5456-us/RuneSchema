# RuneSchema 0.7.9

RuneSchema 0.7.9 uses one `main.dll` for Steam/GOG and Game Pass/WinGDK.
The runtime detects the storefront at startup and selects the matching native
support automatically.

## What changed

- RuneSchema content uses one authoring definition for standalone and
  multiplayer. NPCs no longer use a `Multiplayer` field.
- Server authority still controls gameplay changes in multiplayer; clients
  receive replicated actors and presentation.
- `/assets` clones register with the live item subsystem before recipes,
  rewards, or inventory grants use them.
- Consumable packs may be cloned from compatible native pack ItemData and may
  replace their `Items to Drop` contents.
- RuneSchema recipe unlocks may persist normally once their live RecipeData has
  a valid PersistenceID.
- Vendor-generated recipes remain transient.
- Safe Clean removes orphaned RuneSchema identities when the supplying mod is
  no longer installed.
- Steam/GOG and Game Pass use the same JSON loaders and cleanup rules while
  keeping storefront-specific native bindings separate.

## Save handling

RuneSchema does not maintain a restore history or rewrite Xbox Game Save
containers directly. Enabled content registers first. Character cleanup then
removes unresolved RuneSchema identities at the game's normal character-load
boundary and lets Dragonwilds save normally.

Removing a mod is treated as removing that content. Reinstalling it later is a
fresh installation; cleaned progress is not recreated automatically.

## Multiplayer

Authors do not choose a single-player or multiplayer mode for RuneSchema
content. Install the same mod and cooked assets on the server and clients that
need to render them. RuneSchema uses the current world role to decide authority
and presentation.

## Startup log

RuneSchema identifies itself at startup, including:

- RuneSchema version;
- detected storefront;
- selected native binding lane;
- mapping status; and
- current network role as the game becomes ready.

## Packages

- **Universal**: RuneSchema core plus optional bundled plugins.
- **Core**: RuneSchema without optional plugins.
- Use the UE4SS runtime intended for the installed Steam/GOG or Game Pass build.

See the [Authoring Guide](AUTHORING-GUIDE.md), [Loader Reference](LOADER-WALKTHROUGHS.md),
and [Compatibility](COMPATIBILITY-BACKBONE.md).
