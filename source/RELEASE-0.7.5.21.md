# RuneSchema 0.7.5.21

## Resource-node scale stability

- Keeps authored resource scale absolute at both permanent and temporary actor creation.
- Re-normalizes RuneSchema-managed native-respawn actors once per second after the game's
  `ResourceRespawnComponent` restores them. This prevents a reused or legacy-scaled ore node
  from carrying a compounded transform into another respawn.
- Limits reconciliation to authoritative worlds, active cells, ordinary actor entries, and
  entries that explicitly use native respawn.
- Adds a regression contract that rejects scale multiplication from the actor's current scale.

The Steam/Game Pass lanes, loaders, SafeSave behavior, plug-ins, and packaged mappings are
otherwise unchanged from 0.7.5.20.
