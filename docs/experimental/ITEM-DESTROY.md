---
channel: experimental
promote_to: ITEM-DESTROY.md
---

!!! warning "In development · 0.9.0.7"
    The service and Helpy's Destroy button are built and contract-tested; live native
    invocation is not yet accepted. Grand Exchange transfers stay disabled. On a server, call
    the service from your plugin's server half; see
    [Plugin server requests](PLUGIN-SERVER-REQUESTS.md).

# Exact native item destroy (in development)

The game-side `runeschema.item.destroy` service requires the loaded caller's declared `item.destroy` capability and a game-thread call. It targets a whole stack in the active character's Inventory or Personal container by Player GUID, Scope, Slot, Instance GUID, Item path, Count and Request UUID. Equipped items and partial stacks are refused. Split a stack in game first.

The service captures the exact item snapshot before invoking the game's existing `InventoryController:DestroyItemFromInventory` function once. A complete snapshot-bearing response must fit the caller's buffer before any mutation. Successful verification requires the same character/world/owned inventories, absence of the exact GUID and an aggregate item-count decrease equal to the requested stack. An unchanged verified snapshot returns Retained; ambiguous results return Unknown and must not be retried as a new request.

Request UUIDs are caller/character scoped and body-bound. The first response is replayed before a live-item lookup, including when that item is already absent. The in-process receipt cache never evicts Unknown outcomes and refuses further actions at its size limit. This cache is volatile: a consumer must journal its durable intent and reconcile after restart. These are in-memory observations, not native save acknowledgements or market settlement receipts.

Callers enforce administration, delivery restrictions and server routing before using this service. Helpy's own Destroy button does so as described below. Grand Exchange production transfers remain disabled until their separate durable transaction, native save and inventory-receipt tests pass.

Validation: Windows native core build and item-destroy contract test pass on the original 0.9.0.6 base. Integration with 0.9.0.7 and live native invocation remain pending. No stable package or installation is implied.

## Helpy's Destroy button

In Helpy's **Inventory workbench**, right-click a stack and choose **Destroy stack…**. Helpy
asks twice: the menu item, then a dialog where you tick *I understand: permanently destroy
{count} × {item}* before the **Destroy** button works. The dialog expires after a minute.

- **Whole stacks only**, from the Inventory tabs or Personal storage. Equipped items and placed
  storage are not offered. Split a stack in game first to destroy part of it.
- **Where it runs.** In single player, as host or on a server, the inventory panel reads the
  live game and Destroy runs there, for any connected character. The host is the world's
  administrator, so destroying on another player's character isn't paused by their delivery
  flags.
- **Your own character keeps its delivery gate.** If you turned off Helpy deliveries for that
  character, or a mod paused deliveries to it (for example while it trades on the Grand
  Exchange), Destroy is refused with the same reason. A mod's character exception applies as it
  does for deliveries.
- **Through a server.** A client's request goes to the server, which needs saved Helpy access
  from the host or the server's allowlist; a world PIN is not enough. The server acts only on
  the sender's own character, checks its own flags folder, and the client checks its flags
  before sending. (A client's inventory panel can't read the server's inventory yet, so the
  button appears only where the game is authoritative.)
- **The result.** Helpy shows *Destroyed*, *Kept* (the game didn't remove it), *Not destroyed*
  with the reason, or *Unknown*. The item snapshot stays in the game. After an unknown result,
  or if no result arrives in 20 seconds, Destroy is unavailable until the inventory is read
  again. Helpy never retries a destroy.
- *Destroyed* means the stack is gone from the live inventory. It is not a save receipt.
