# RuneSchema bridge diagnostics

RuneSchema records bounded, observational lifecycle data for every generic bridge channel. Diagnostics never authorize, reject, retry, or otherwise change gameplay. They contain no account identity, display name, network address, inventory payload, asset path, or server exception text.

Detailed events are disabled by default. Set `bridgeDiagnostics.verbose` to `true` in `settings/settings.jsonc` for a support run. Counters and warning/error events remain active, and RuneSchema writes one compact health summary when a world closes. The read-only in-process snapshot is exposed through `RegistryBridge::DiagnosticSnapshot()` for UE4SS support tooling and automated contracts.

## Event schema

Each event contains:

- `schemaVersion`, `monotonicMs`, and `worldEpoch`
- `networkRole` and `side`
- `channel`, opaque `owner`, stable `entity`, and `action`
- `requestRevision` and locally derived `correlationId`
- `stage`, `outcome`, and safe `reasonCode`
- `retryCount`, `elapsedMs`, and rate-limited `suppressedCount`

Storage is reset at every world boundary and capped at 256 global events and 32 events per correlation. Strings and counter cardinality are bounded. Production builds do not contain the development fault-injection control.

## Sanitized success example

```json
{"schemaVersion":1,"monotonicMs":642,"worldEpoch":3,"networkRole":"dedicated-server","side":"authority","channel":"consumable.authority","owner":"player-2","entity":"SummoningPotions:summon_wolf","action":"consume","requestRevision":17,"correlationId":"3-2-17","stage":"permit_matched","outcome":"accepted","reasonCode":"accepted","retryCount":0,"elapsedMs":0,"suppressedCount":0}
{"schemaVersion":1,"monotonicMs":644,"worldEpoch":3,"networkRole":"dedicated-server","side":"authority","channel":"consumable.authority","owner":"player-2","entity":"SummoningPotions:summon_wolf","action":"consume","requestRevision":17,"correlationId":"3-2-17","stage":"callback_completed","outcome":"executed","reasonCode":"executed","retryCount":0,"elapsedMs":0,"suppressedCount":0}
{"schemaVersion":1,"monotonicMs":650,"worldEpoch":3,"networkRole":"client","side":"requester","channel":"registry.action","owner":"player-1","entity":"SummoningPotions:summon_wolf","action":"receipt","requestRevision":17,"correlationId":"3-1-17","stage":"receipt_received","outcome":"executed","reasonCode":"executed","retryCount":0,"elapsedMs":0,"suppressedCount":0}
```

## Sanitized failure example

```json
{"schemaVersion":1,"monotonicMs":5012,"worldEpoch":4,"networkRole":"client","side":"requester","channel":"consumable.authority","owner":"player-1","entity":"SummoningPotions:summon_wolf","action":"consume","requestRevision":4,"correlationId":"4-1-4","stage":"request_expired","outcome":"expired","reasonCode":"bridge_not_ready","retryCount":9,"elapsedMs":5012,"suppressedCount":0}
```

The examples intentionally contain the stable registry key but no `DataAsset` or `GraphClass` path.
