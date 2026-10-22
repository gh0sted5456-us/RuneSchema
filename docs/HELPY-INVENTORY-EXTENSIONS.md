
Scrap and automatic deleted-item Helpy Storage have been retired at the user's
request. No Delete observer, Scrap input/prompt, refund action, restore action,
Storage tab or Scrap setting is compiled into this candidate. Native Delete is
not intercepted. Existing recovery files are preserved; earlier source snapshots
remain in artifacts. Native chest and personal-inventory inspection remain.

## Favorite / Junk

Favorites are built in and enabled by default. Clicking Helpy's native inventory
marker or middle-clicking a hovered bag item cycles Unmarked → Favorite → Junk →
Unmarked for that exact character/item instance. Press/release must stay on the
same item; focus, selection or character changes cancel the click. No vanilla
item action is intercepted. The old InventoryFavorites Lua mod should be
disabled on the next approved installation, retaining its files and saved data.

Helpy and the in-game markers use the same personalization store. Live item-type
marks are character-scoped; stack overrides inherit that character's type marks.
Catalog-only labels remain local catalog preferences, not server-wide rules.
Only Favorite and the Junk flag are projected onto the multiplayer mark wire.
Nicknames, private tags, locks, portraits and inventory contents are not included
in this initial mark protocol.

## Multiplayer mark replication

The client exports bounded pages for its own loaded character through the
existing owned-player RegistryBridge RPC. The server derives the active character
from the owning pawn/controller, rejects mismatched owners and malformed pages,
and retains the existing rate/replay bounds. Ordinary mark sync does not require
the item-grant/admin allowlist and cannot invoke an item mutation.

Pages carry a content version and cursor. Duplicate retries are acknowledged;
partial, out-of-order, oversized or cross-character snapshots cannot partially
replace visible marks. A completed snapshot replaces only that character's
remote marks atomically through the existing conflict-checked personalization
writer. Cleared labels propagate. Remote marks never overwrite the host's own
local labels. Host inventory rows read the replica and disable mark editing.

The relay runs without the inventory or Helpy window open, resets across
world/controller/character changes, and periodically resynchronizes. Missing or
older server support retains local marks and backs off instead of retrying item
actions. This is owner-to-authority replication, not a public broadcast of every
player's labels to all clients. Live two-peer Steam/Game Pass acceptance remains
pending; unit/contract checks alone do not prove network delivery or MMB input.

## Requested next extension — not implemented in this candidate

- Public, owner-controlled nicknames and cropped portraits.
- Private tags remain owner-only.
- Inventory viewing and adding/removing items by other players requires explicit
  server-issued rights, not possession of a Favorite/Junk replica.
- The host can grant delegation; a delegate may grant only the same or narrower
  rights, targets and lifetime. Parent revocation invalidates descendant grants.
- View, add, remove and delegate are distinct rights; no self-escalation and no
  implication that native role badges already confer these rights.

The account/connection identity and server-owned permission gate must be verified
before exposing cross-player mutations. See HELPY-ADMIN-DESIGN.md. This build does
not broaden the existing client item-grant policy.
