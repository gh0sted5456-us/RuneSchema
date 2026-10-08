# Golden Interaction Text

A client-side RuneSchema mod that changes Dragonwilds' world-interaction text
to a warm golden yellow (`#F2B82E`).

The mod styles these existing widgets in `WBP_HUD_InteractionPrompt`:

- `ItemNameTextBlock` — interaction item or object name;
- `ItemAdditionalDescriptionTextBlock` — secondary interaction description;
- `InventoryStateTextBlock` — inventory/status line.

It changes only `Color`. Native text, localization, font, size, shadow, wrapping,
alignment, and interaction behavior remain unchanged.

## Install

Copy the complete `GoldenInteractionText` folder into:

```text
<Dragonwilds>/RSDragonwilds/Binaries/Win64/ue4ss/Mods/RuneSchema/mods/
```

For Game Pass, use the equivalent `WinGDK/ue4ss/Mods/RuneSchema/mods/` path.

Add this line to `RuneSchema/mods/runeschema.txt` if RuneSchema does not add it
automatically:

```text
GoldenInteractionText : 1
```

Restart Dragonwilds after installation. This mod requires RuneSchema 0.7.7.4m
or a later build that supports `$RuntimeWidget.$TextStyle`.

## Customize the color

Edit `blueprints/10-GoldenInteractionText.jsonc` with the game closed. The
current normalized RGBA color is:

```json
{ "R": 0.95, "G": 0.72, "B": 0.18, "A": 1.0 }
```
