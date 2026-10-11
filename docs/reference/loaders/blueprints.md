# Blueprints loader

**Folder:** `RuneSchema/mods/<ModName>/blueprints/`

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Use `/blueprints` to change supported reflected defaults on a class or component
that is already loaded. The loader does not create Blueprint classes; cook a new
class in a PAK and mount it when your mod needs one.

```json
[
  {
    "/Game/Path/BP_Target": {
      "Data": { "Duration": 10.0 }
    }
  }
]
```

Check property names and types against the live class. Restart the game after
changing class defaults.

## Existing menu widgets

`$RuntimeWidget` can apply reflected properties to an existing live widget and
bind a compatible multicast UI event to a reflected function. It does not
create new widgets or functions.

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

Widget paths use dot-separated reflected properties starting at the live owner.
`$Bind.Target` uses the same form; omit it or use `"$Owner"` to bind a function
on the owner. The delegate and function parameter contracts must be compatible.
Runtime rules are event-driven and deduplicated by target and function.

When a rule applies to only one storefront, gate the whole block:

```json
"$RuntimeWidget": {
  "$Storefront": "GamePass",
  "$SkipMessage": "This rule is only needed on Game Pass.",
  "Character.EditAppearanceButton": {
    "Visibility": "Visible"
  }
}
```

`$Storefront` accepts `"GamePass"`/`"WinGDK"`,
`"Steam"`/`"SteamGOG"`/`"GOG"`, `"Any"`, or an array of these values.
`$SkipMessage` is optional and emitted only when the running storefront is known.

## Simple rules

- Use `/blueprints` only for supported reflected defaults on loaded classes or components.
- Use `$RuntimeWidget` only for existing live widget-tree objects.
- Confirm each field, widget path, delegate, and function against live reflection.
- Restart after changes. Cook and mount a Blueprint when a new class is required.

## FAQ

### FAQ-BLUEPRINTS-001 — Can the Blueprints loader create a new Blueprint class? {#faq-blueprints-001}

No. It edits supported reflected defaults on loaded classes or components. A
new Blueprint class must be cooked and mounted separately.

### FAQ-BLUEPRINTS-002 — Do class-default changes hot-reload safely? {#faq-blueprints-002}

Treat them as restart-required.

### FAQ-BLUEPRINTS-003 — Can a Blueprint mod change a live menu button? {#faq-blueprints-003}

Yes, when the button already exists in the cooked widget tree. Use
`$RuntimeWidget` to apply reflected properties and, if needed, bind a
compatible multicast event to an existing function.

## Working example

- [Fixed Menu: character-creation labels](../../examples/FixedMenu/blueprints/character_creation_text.jsonc)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
