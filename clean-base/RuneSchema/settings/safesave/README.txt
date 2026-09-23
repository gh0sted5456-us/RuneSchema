RuneSchema SafeSave state
=========================

RuneSchema writes its compact active-content identity snapshot here as
OwnedContentLedger.json. Do not edit it while the game is running.

The snapshot contains RuneSchema-owned identities only. Each successful load
compares the previous snapshot with currently active content, removes missing
known identities from supported save structures, then atomically replaces the
snapshot. It is not a restoration archive.
