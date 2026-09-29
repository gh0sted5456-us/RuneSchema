# RuneSchema 0.7.6.8

- Restores staged retrieval-quest hand-ins by checking the authoritative player inventory before filtering dialogue choices.
- Honors authored hand-in areas and aggregated item requirements when deciding whether a collection choice is available.
- Reports the current carried and required amounts when a collection hand-in is not ready.
- Retains absolute resource, NPC, and managed-spawn scaling; reload and respawn scale idempotence remains covered by regression tests.
