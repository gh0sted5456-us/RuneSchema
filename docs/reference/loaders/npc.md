# NPC loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/npc/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Choose the compatible native actor role/visual source and a stable NPC ID.
2. Create `npc/10-camp-keeper.jsonc`; replace the mock-up class/mesh and location with inspected values.
3. Bind only the needed DialogueID, VendorID, QuestID or JournalEntries. Use supported grounding rules for the intended floor.
4. Restart and verify authority-owned placement, interaction, client presentation and reload without duplicates.

**Tie-ins:** [Dialogue](dialogue.md), [vendors](vendors.md), [quests](quests.md), [journal](journal.md) and [lore](lore.md).


**Folder:** `RuneSchema/mods/<ModName>/npc/`

Creates persistent interactable AI, human, resource-style actors, and merchants.

`Location.Z: "$"` prefers landscape beneath overhead roofs or bridges. To place
an NPC on a particular stair landing, bridge deck or upper floor, add an
approximate `GroundReferenceZ` alongside a `$` location. The shared floor query
allows a 45 cm step upward, rejects wall/underside hits and initial overlaps,
and requires real blocking collision before applying `$+offset`/`$-offset`.
A numeric NPC Z remains an exact authored placement. See the
[stacked-surface grounding rules](spawns.md) for terrain and streaming limits.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

```jsonc
{
  "Id": "merchant",
  "Type": "AI",
  "DisplayName": "Merchant",
  "VendorID": "shop",
  "DialogueID": "hello",
  "VisualSource": "/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C",
  "Location": [1000, 2000, "$+10"]
}
```

NPC definitions use one network-neutral format. Do not add a `Multiplayer`
field. In standalone RuneSchema runs the definition locally; in multiplayer the
server owns creation and clients prepare the replicated presentation.

## Rules

RuneSchema 0.8.0.0's Human-only `EquipmentWhen` rules can latch wearable changes
after a quest completes in this world. They require an explicit shared-world
policy; see [equipment consequences](../../QUEST-ALTERNATIVES-AND-NPC-EQUIPMENT.md)
for persistence, replication and backup requirements. Released schema support
is not a claim of live-world verification for every equipment rule.

- Choose a compatible actor role and visual source.
- Bind `VendorID`, `DialogueID`, `QuestID`, or lore only when needed.
- The `JournalEntries` list requests native player discovery on interaction without replacing the NPC's shop or dialogue. Use local journal/lore IDs, `Mod:Id`, or full cooked journal paths; see the [journal loader](journal.md). `NoInteract` suppresses these bindings.
- `HideWeapon` suppresses inherited weapon presentation on supported human NPCs.
- Human `Equipment` accepts `Necklace` for items whose native slot strategy is
  `Necklace`, alongside the existing `Trinket` slot. `Trinket2` is an alias for
  `Necklace`; do not specify both. Slot validation uses the live item's native
  strategy, so a necklace cannot be assigned to `Trinket`.
- Use `/buildings` for buildable architecture and `/spawns` for general world
  placement rather than using NPCs as generic props.

## FAQ

### Can an NPC act as a merchant?

Yes. Bind a RuneSchema store with `VendorID`. `DialogueID` is optional.

### Do I need separate single-player and multiplayer NPC files?

No. Use one definition.

### Can I hide an inherited weapon?

Yes, on supported human NPCs, with `HideWeapon`.


---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
