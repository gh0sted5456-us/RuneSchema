# Helpy inventory extensions — staged implementation

This is not an installed or fully accepted runtime feature. SchemaVision stays
removed; these additions use Helpy's existing interface and native item APIs.

## Delete and Helpy Storage

Native Delete is never canceled, replaced, invoked, or retried by the observer.
The staged adapter watches InventoryUIAPI.HandleDestroyItem and the local/server
InventoryController.DestroyItemFromInventory boundaries. It supports the current
authoritative character's bag, loadout and personal inventory, not foreign chests.

Before the native call, it attempts a bounded, durable item-only capture. A busy,
full, damaged or unavailable store leaves Delete available and logs that no copy
was committed. Nested boundaries capture an item once. After the call, the adapter
rechecks character, pawn, world and inventory identity. Only verified absence of
the instance GUID makes the entry eligible for eventual restoration. A retained
item is not recoverable; uncertain results remain quarantined, without retries.

Storage lives under Helpy's state directory in `storage/<character-guid>.json`.
It contains items and transaction receipts, not appearance, world or character
progression data. Bounds are 4 MiB per document, 1,024 entries and 64 KiB per item.
Completed restored/not-deleted receipts are pruned on the next capture; active
and uncertain records are retained. Corrupt files are never silently replaced.

The Character editor's Helpy Storage tab reuses existing Item Bay placards,
hover/context styling and icon lookup. Reads are asynchronous and local; opening
the tab does not issue another global catalog search. An externally opened file
must match the selected character GUID. This browser has no active Restore button.

## Native item fidelity

The original serializer reads only supported native SaveGame fields. It records
native class/schema identity and property text, plus exact numeric scalar bits
for fields where Unreal's text formatting loses precision. It never serializes
engine container pointers or world/transient object references. Applying a
snapshot is restricted to a fresh RF_Transient item in the transient package with
zero referencing inventories. Existing inventory items are never rewritten.

An isolated Steam native-engine fixture round-tripped base Item and Equipment,
including GUID, quantity, durability, subsecond DateTime ticks and a tiny shield
float. This does not establish coverage for every mod subclass, enchantment or
nonempty tag array, nor does it establish live inventory restoration.

## Shared inventory policy and Scrap

The `helpy.inventory` service provides the shared type/stack Favorite/Junk model,
storage transactions and inverse-recipe planner. Mutations require a 4 KiB reply
buffer before effects and return bounded acknowledgements. Read-only storage
queries retain the normal larger transport bound. A buffer-size probe does not
perform a mutation.

`InspectScrapItem` resolves the current authoritative native recipe registry and
selected live bag instance. It produces a plan only. Scrap is Helpy's own action:
the intended commit consumes the selected quantity and refunds deterministic
crafting components as native world drops, not another item copy in Storage.
Ambiguous, probabilistic, partial-batch and protected/equipped cases are rejected.

## Still required before enabling recovery or Scrap

- In-world proof that the native X/Delete path reaches the observer on each host.
- Exact native restoration into a verified empty inventory slot, with duplicate
  checks and durable intent/result receipts.
- Single-attempt native Scrap consumption and confirmed world-drop receipts,
  including partial/unconfirmed failures and no automatic retries.
- Native right-click/number-key/S input integration and client/server ownership
  acknowledgements. Do not assume cross-network favorite protection is active.

Capabilities intentionally report native capture/restore/Scrap as unavailable
until runtime acceptance, even though the observational adapter is staged.
