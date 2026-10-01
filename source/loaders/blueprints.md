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

## RuneSchema .8 test-bed runtime actions

The `RuneSchema-0.8-test-bed` branch expands `$RuntimeWidget` with reflected
runtime actions. These directives are experimental and are not part of the
stable 0.7.x contract.

### `$Call`

`$Call` invokes an existing reflected function on the live target, the owner,
or another object reachable from the owner. RuneSchema resolves the live
`UFunction`, verifies every named JSON argument is a reflected input parameter,
uses the shared `ActorHelper::FunctionCall` parameter lifecycle, and dispatches
through `ProcessEvent`.

```jsonc
"$RuntimeWidget": {
  "SomePanel": {
    "$Call": {
      "Target": "$Self",
      "Function": "SetRenderOpacity",
      "Args": {
        "InOpacity": 1.0
      }
    }
  }
}
```

`Target` defaults to `"$Self"`. Use `"$Owner"` for the owning widget or a
dot-separated owner path for another reflected object. `$Call` may also be an
array to run several calls in order. The first test-bed implementation is
deliberately input-only: functions with return values or reflected output
parameters are rejected until their result lifetime is explicitly modeled.

### `$When` and `$Once`

`$When` gates a rule on the ProcessEvent function that caused the runtime
refresh. It accepts one function name, an object with `Function`, or an array
of function names. `"Any"`, `"Always"`, and `"Resolved"` retain the normal
event-driven behavior.

```jsonc
"$RuntimeWidget": {
  "Character.EditAppearanceButton": {
    "$When": { "Function": ["Construct", "OnActivated"] },
    "$Once": true,
    "Visibility": "Visible"
  }
}
```

`$Once` is tracked per live owner and rule until world teardown. Runtime
actions have a re-entrancy guard because `$Call` and CommonUI activation
generate ProcessEvent traffic themselves.

### `$Activate`

`$Activate: true` calls `ActivateWidget()`; `$Activate: false` calls
`DeactivateWidget()`. The directive fails closed when those reflected
functions are not present, so ordinary widgets are not treated as CommonUI
activatables.

```jsonc
"$RuntimeWidget": {
  "PauseScreen": {
    "$When": "Resolved",
    "$Once": true,
    "$Activate": true
  }
}
```

### `$TextStyle`

`$TextStyle` changes a specific live UMG text widget when its normal runtime
event fires. It uses the same owner-scoped target resolution as the rest of
`$RuntimeWidget`, performs no global widget scan, and calls the widget's native
`SynchronizeProperties()` once after the reflected fields are updated. Pair it
with `$Once: true` when the style does not need to be reapplied after later
native refreshes.

```jsonc
"$RuntimeWidget": {
  "Header.TitleText": {
    "$When": ["Construct", "OnActivated"],
    "$Once": true,
    "$TextStyle": {
      "Color": { "R": 0.9, "G": 0.72, "B": 0.25, "A": 1.0 },
      "Font": { "Size": 28, "LetterSpacing": 25 },
      "ShadowColor": { "R": 0.0, "G": 0.0, "B": 0.0, "A": 0.75 },
      "ShadowOffset": { "X": 1.0, "Y": 2.0 }
    }
  }
}
```

Supported fields are `Color` (friendly RGBA alias), `ColorAndOpacity` (raw
`FSlateColor`), `Font` (partial `FSlateFontInfo`, so changing only `Size`
preserves other fields), `ShadowColor`, `ShadowColorAndOpacity`, `ShadowOffset`,
`MinDesiredWidth`, `AutoWrapText`, and `Justification`. Font assets and
materials may be supplied through the ordinary reflected `Font` fields when a
specific game font is desired. Unknown members fail closed instead of being
silently ignored.

The next .8 phases are intentionally separate: safe scoped discovery
(`WidgetTree` / `HUDWidgetRefs` / CommonUI containers) before any global
fallback, followed by a RuneSchema-owned `$RuntimeUI` tree for transient UMG.
Do not use unrestricted global widget sweeps as an authoring primitive.

## Scoped runtime discovery

The 0.8 test bed adds `$Find` as a fallback when the ordinary dot-separated
widget path cannot resolve. RuneSchema always tries the direct path first.

```jsonc
"$RuntimeWidget": {
  "InventorySearch": {
    "$Find": {
      "Scope": "HUD",
      "Class": "WBP_HUD_Inventory_C"
    },
    "Visibility": "Visible"
  }
}
```

Supported scopes are deliberately narrow:

- `WidgetTree` — searches only below the owner's live `WidgetTree`. An exact
  `Name` is required; the implementation tries the exact object path first and
  only uses an exact-name/outer-chain fallback.
- `HUD` — resolves live Dominion player controllers for the owner's world and
  walks only each HUD's reflected `HUDWidgetRefs` array.
- `CommonUI` — enumerates only loaded
  `CommonActivatableWidgetContainerBase` instances and walks their reflected
  `WidgetList` arrays.

`Name` and `Class` are exact selectors. At least one is required. If more
than one live object matches, RuneSchema rejects the rule instead of choosing
one. CDOs, archetypes, loading objects, and objects being destroyed are excluded.

A discovered target is weak-tracked back to its owner so later ProcessEvent
traffic from that target can refresh the same runtime rules without retaining a
dead UObject pointer. The tracking table is cleared on world teardown.

There is intentionally no general `FindAllOf(UserWidget)` authoring path.

## RuneSchema-owned transient UI

The 0.8 test bed also accepts a separate `$RuntimeUI` block on an existing
owner widget class. This does **not** create a Blueprint class. RuneSchema owns a
transient `UUserWidget`, its `WidgetTree`, viewport lifetime, and cleanup.

```jsonc
{
  "/Game/UI/MainMenu/WBP_MainMenu_CharacterSelect": {
    "$RuntimeUI": {
      "SchemaNotice": {
        "ZOrder": 95,
        "$When": "Construct",
        "Root": {
          "Type": "Border",
          "Name": "NoticeBorder",
          "Padding": { "Left": 12, "Top": 8, "Right": 12, "Bottom": 8 },
          "Children": [
            {
              "Type": "TextBlock",
              "Name": "NoticeText",
              "Text": "RuneSchema 0.8 test bed"
            }
          ]
        }
      }
    }
  }
}
```

The first implementation deliberately whitelists only `CanvasPanel`,
`Border`, `TextBlock`, `Image`, and `Button`. A tree is limited to
64 nodes, depth 8, and 32 children per panel. `Border` and `Button` accept
one child; `TextBlock` and `Image` are leaves. UI and node names must contain
only letters, numbers, and underscores.

Each child may provide a `Slot` object. RuneSchema applies those reflected
properties to the panel slot returned by `AddChild`. Node properties are
otherwise applied with the normal reflected Blueprint property writer.

Runtime-created nodes may use the existing `$Bind` and test-bed `$Call`
directives. This lets a transient Button bind an existing reflected multicast
event to an existing owner function without synthesizing Blueprint bytecode.

`Properties` applies reflected fields to the transient `UUserWidget` itself.
`ZOrder` is limited to -1000 through 10000. Instances are weak-owned, one live
instance per runtime-UI rule, removed from the viewport on world teardown, and
recreated only after the prior owner/widget lifetime has ended.

## Gamepass Edit Appearance restoration

Steam/GOG already exposes the native Edit Appearance control. On Gamepass /
WinGDK, the cooked controls exist under two live character-select owners but
start hidden. The Restore Appearance example uses Gamepass-gated
`$RuntimeWidget` rules to reveal the existing button container, button, and
input legend.

```jsonc
"WBP_MainMenu_CharSelect_C": {
  "$RuntimeWidget": {
    "$Storefront": "GamePass",
    "Character.EditAppearanceButton": {
      "$When": "Resolved",
      "$Find": {
        "Scope": "WidgetTree",
        "Name": "EditAppearanceButton"
      },
      "Visibility": "Visible",
      "bIsEnabled": true,
      "RenderOpacity": 1.0
    }
  }
}
```

The button retains its native `IA_UI_EditAppearance` action and CommonUI
appearance, so the example does not install a synthetic `$Bind`. A second rule
on `WBP_MainMenu_CharacterSelect_C` reveals `InputLegend_EditAppearance`.
This path intentionally does not modify Steam/GOG.

## Simple rules

- Use `/blueprints` only for supported reflected defaults on an existing loaded class or component.
- Use `$RuntimeWidget` only for existing live widget-tree objects.
- `$Bind.Event` must be a reflected multicast delegate and `$Bind.Function` must resolve on the selected target.
- Test bed: `$Call` only invokes reflected functions and every named `Args` entry must resolve to an input parameter.
- Test bed: use `$When` and `$Once` for lifecycle-sensitive runtime actions; recursive ProcessEvent re-entry is suppressed per rule.
- Test bed: `$Activate` is only valid when the target exposes CommonUI `ActivateWidget` / `DeactivateWidget`.
- Test bed: `$Find` is fallback-only and limited to `WidgetTree`, `HUDWidgetRefs`, or CommonUI `WidgetList` discovery.
- Test bed: ambiguous `$Find` results fail closed; no live widget is selected by guesswork.
- Test bed: `$RuntimeUI` creates only RuneSchema-owned transient UMG from the explicit primitive whitelist; it does not create Blueprint classes.
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
- [Restore Appearance: Gamepass-only transient button routed to the native character editor](../examples/RestoreAppearance/blueprints/10-RestoreAppearance.jsonc)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
