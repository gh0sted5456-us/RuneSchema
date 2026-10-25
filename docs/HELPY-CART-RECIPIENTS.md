# One cart, multiple recipients

The cart's Give to picker supports up to 16 checked players. Everyone receives
the same selected quantities. Item Service, its browsing Item Bay, and the
floating shared cart use the same staged cart. Recipient selections are session
state; kit files still contain only item paths/counts and never grant authority.

One explicit Give items action snapshots both recipients and quantities. The
host sends one bounded GiveItems request at a time, waits for its correlated
native inventory-delta receipt, then advances. Editing is locked while delivery
is pending. Confirmed items leave the cart after every chosen recipient has one
exact confirmation for its path and count. Definite rejected items also leave
the active cart, with their rejection retained in Last delivery.

In **0.8.0.1b source**, right-click a confirmed Last delivery item to **Reorder**, or right-click the
Last delivery heading to reorder all confirmed items. This adds the actual
delivered quantities once per selected player, not multiplied by the previous
recipient count. Stack-size delivery modes retain their sent quantities.
Existing cart contents and the current player/equip selections are retained.
Nothing is sent until the user reviews the cart and clicks Give items again.
Rejected or uncertain receipts are not reorderable; an inventory-review lock
must be resolved first. Reorder is all-or-nothing within the normal 64-item /
10000-per-item cart limits and refreshes validation for the staged items.

An unconfirmed result, timeout or world change stops the batch. Unconfirmed
items stay staged; the UI shows delivery results and requires an explicit
inventory-review acknowledgment before another delivery. It never retries a
possibly successful grant automatically. An offline/stale player is rejected by
the existing authoritative target validation, not silently replaced by another
recipient. The batch retains its original target snapshot even if selection
changes elsewhere in the UI.

## Current authority boundary

Multi-recipient delivery requires authoritative host/server player paths. The
existing remote client Helpy bridge is self-targeted and returns server queue
acceptance, not the final inventory delta. Sending several GUIDs through that
old route would target the caller repeatedly, so multi-recipient GUID batches
are rejected before submitting anything. Single-recipient legacy forwarding is
unchanged, but an acceptance-only response is not treated as a confirmed grant.

Remote target-aware delivery and server Admin controls require a new explicit
server-validated capability/receipt contract. They are **not** enabled by this
cart change. No permission setting is changed by selecting a player.

## Clone artwork

The native icon resolver publishes item-to-icon dependencies even when tracing
is disabled. Native inspection/creation receipts update the exact item's icon
in visible rows and cached queries. Cards, list view, the cart and drag payloads
all use that resolved icon; an inherited $Clone source icon must not win over
the clone's current Icon. Empty native icons are respected rather than silently
restoring the source icon. UI-only draft preview icons remain pinned to the
chosen appearance until a real created-item receipt exists. Icon confirmations
are bounded and cleared on source/world changes; they do not create items.

Standalone tests verify sequencing, duplicate/malformed/unconfirmed receipts,
partial failure, stop/timeout fences, target bounds, exact clear semantics and
native icon transport into the cart. Live multiplayer acceptance still requires
a host/server test with two real players. No live grants are made by tests.
