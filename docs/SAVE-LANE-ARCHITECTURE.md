# Steam and Game Pass save lanes

RuneSchema uses one ownership and cleanup policy with two storage adapters. The
logical character data is the same; the storefront controls how a verified
change is committed.

## Shared policy

Both lanes build the same current ownership snapshot, compare it with the
previous ledger, and retire only RuneSchema-owned identities whose mod or
`$declaration` disappeared. The planner is bounded, deterministic, idempotent,
and fails closed. Vanilla content and unknown third-party identities are never
deleted merely because a lookup failed.

## Steam/GOG adapter

Steam character state is ordinary JSON below
`%LOCALAPPDATA%/RSDragonwilds/Saved/SaveCharacters`. RuneSchema can therefore
perform a bounded parse, backup, atomic replacement and read-back before it
commits the new ownership ledger. Steam Cloud may synchronize those files, but
it does not change their local representation.

## Game Pass/WinGDK adapter

Xbox Game Save stores the same character JSON as a provider-owned blob inside
WGS containers. Container indexes, blob tables and cloud synchronization are
part of the provider transaction. RuneSchema must not treat a GUID-named blob
as an ordinary save file while the provider is active.

The WinGDK lane therefore registers retired identities before hydration,
removes supported state from the live player components, verifies the live
collections, and lets the game's save provider serialize the result. Failed
verification leaves that category pending in the previous ledger.

Offline conversion or recovery tools may inspect WGS while the game is closed,
but that is a repair workflow—not RuneSchema's in-process save backend.

## Non-negotiable invariants

- Storefront detection selects exactly one adapter before save work begins.
- Steam paths are never a fallback for a WinGDK transaction.
- WGS blobs are never rewritten directly in-process.
- The ledger advances only for categories whose cleanup was verified.
- A category RuneSchema cannot safely parse remains pending rather than being
  guessed or discarded.
- Save scanning stays bounded; it never performs an unbounded registry or disk
  crawl during world entry.

