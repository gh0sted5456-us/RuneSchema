# Blueprints loader

**Folder:** `RuneSchema/mods/<ModName>/blueprints/`

Supported reflected defaults on existing loaded classes and components.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/blueprints` for supported reflected defaults on an existing loaded
class or component. It does not create Blueprint classes.

```json
[
  {
    "/Game/Path/BP_Target": {
      "Data": { "Duration": 10.0 }
    }
  }
]
```

Confirm every field against live reflection. Restart after changing a class
default. Use a cooked Blueprint in a PAK when a new class is required.

## Runtime widgets

`$RuntimeWidget` applies reflected properties to live widget-tree objects and can
bind a multicast UI event to a reflected function on the owner or another object
reachable from the owner.

```json
{
  "/Game/UI/MainMenu/WBP_MainMenu_CharacterSelect": {
    "$RuntimeWidget": {
      "Character.EditAppearanceButton": {
        "Visibility": "Visible",
        "bIsEnabled": true,
        "RenderOpacity": 1.0,
        "$Bind": {
          "Event": "OnButtonBaseClicked",
          "Target": "Character",
          "Function": "GoToEditAppearance"
        }
      }
    }
  }
}
```

Widget paths are dot-separated reflected object properties starting at the live
owner widget. `Target` uses the same path form. Omit `Target`, or use
`"$Owner"`, to bind the function on the owner itself.

Runtime rules are event-driven through the existing ProcessEvent observer. They
do not create a polling timer. Delegate bindings are deduplicated by target
object and function.

Delegate compatibility is based on reflected parameter contracts rather than raw
`ParmsSize`. RuneSchema compares parameter count, direction, reflected property
type, referenced object/class/struct/enum type, nested container element types,
and return type. Equivalent signatures are therefore allowed even when Unreal
pads their parameter buffers differently.

A whole `$RuntimeWidget` block can be storefront-gated:

```json
"$RuntimeWidget": {
  "$Storefront": "GamePass",
  "$SkipMessage": "No need for this mod, please uninstall.",
  "Character.EditAppearanceButton": {
    "Visibility": "Visible"
  }
}
```

`$Storefront` accepts `"GamePass"`/`"WinGDK"`,
`"Steam"`/`"SteamGOG"`/`"GOG"`, `"Any"`, or an array of those
values. A mismatched block is not registered. `$SkipMessage` is optional and
is emitted only when the running storefront is known.

## Simple rules

- Use `/blueprints` only for supported reflected defaults on an existing loaded class or component.
- Use `$RuntimeWidget` only for existing live widget-tree objects.
- `$Bind.Event` must be a reflected multicast delegate and `$Bind.Function` must resolve on the selected target.
- Delegate and function reflected parameter contracts must be compatible; raw parameter-buffer size is not used as the compatibility test.
- Use optional `$Storefront` metadata when a live UI rule is only meaningful on one storefront.
- Confirm every field, widget path, event, and function against live reflection.
- Restart after changing class defaults or `$RuntimeWidget` rules. Cook a Blueprint in a PAK when a new class is required.

## FAQ

### FAQ-BLUEPRINTS-001 — Can the Blueprints loader create a new Blueprint class? {#faq-blueprints-001}

No. It edits supported reflected defaults on classes or components that already
exist and are loaded. A new Blueprint class must be cooked and mounted separately.

### FAQ-BLUEPRINTS-002 — Do class-default changes hot-reload safely? {#faq-blueprints-002}

Treat them as restart-required. The loader reference specifically calls for a
restart after changing a class default.

### FAQ-BLUEPRINTS-003 — Can a Blueprint mod change a live menu button? {#faq-blueprints-003}

Yes, when the button already exists in the cooked widget tree. Use
`$RuntimeWidget` to resolve the live widget, apply reflected properties, and
optionally bind a compatible multicast event to an existing reflected function.
It does not create new widgets or functions.

## Working examples

- [Fixed Menu: character-creation labels](../examples/FixedMenu/blueprints/character_creation_text.jsonc)
- [Restore Appearance: live CommonUI button and native action](../examples/RestoreAppearance/blueprints/10-RestoreAppearance.jsonc)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
