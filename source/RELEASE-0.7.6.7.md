# RuneSchema 0.7.6.7

- Restores RuneSchema vendor stock in standard gameplay by treating generated
  vendor recipes as owned, session-only recipes instead of requiring them to
  pre-exist in the settled persistence registry snapshot.
- Keeps vendor offers transient by placing them in both the visible recipe set
  and Dominion's non-persistent recipe set when a shop opens.
- Removes transient offers from previously opened RuneSchema vendors before
  activating the current shop, preventing stock and category bleed between
  merchants.
- Retains strict live-object, ownership, lease, and deterministic
  `PersistenceID` validation before an offer is exposed.
