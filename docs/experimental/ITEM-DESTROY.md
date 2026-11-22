---
channel: experimental
promote_to: ITEM-DESTROY.md
---

!!! warning "In development · 0.9.0.7"
    Service only, for in-game plugin halves. There is no Helpy Destroy button yet, and
    Grand Exchange transfers stay disabled. Live native invocation is not yet accepted. On a
    server, call it from your plugin's server half; see
    [Plugin server requests](PLUGIN-SERVER-REQUESTS.md).

# Exact native item destroy (in development)

The game-side `runeschema.item.destroy` service requires the loaded caller's declared `item.destroy` capability and a game-thread call. It targets a whole stack in the active character's Inventory or Personal container by Player GUID, Scope, Slot, Instance GUID, Item path, Count and Request UUID. Equipped items and partial stacks are refused. Split a stack in game first.

The service captures the exact item snapshot before invoking the game's existing `InventoryController:DestroyItemFromInventory` function once. A complete snapshot-bearing response must fit the caller's buffer before any mutation. Successful verification requires the same character/world/owned inventories, absence of the exact GUID and an aggregate item-count decrease equal to the requested stack. An unchanged verified snapshot returns Retained; ambiguous results return Unknown and must not be retried as a new request.

Request UUIDs are caller/character scoped and body-bound. The first response is replayed before a live-item lookup, including when that item is already absent. The in-process receipt cache never evicts Unknown outcomes and refuses further actions at its size limit. This cache is volatile: a consumer must journal its durable intent and reconcile after restart. These are in-memory observations, not native save acknowledgements or market settlement receipts.

This addition does not yet expose a Helpy Destroy menu or authorize remote-player actions. Existing Helpy administration, delivery restrictions and server routing must be enforced by those callers before this service is used. Grand Exchange production transfers remain disabled until their separate durable transaction, native save and inventory-receipt tests pass.

Validation: Windows native core build and item-destroy contract test pass on the original 0.9.0.6 base. Integration with 0.9.0.7 and live native invocation remain pending. No stable package or installation is implied.
