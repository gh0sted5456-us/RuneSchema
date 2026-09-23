# RuneSchema 0.7.5.7

This release adds a second RSDW Base Builder import contract without changing
the existing native-piece default.

- `NativeBuildingPieces` imports every piece as a native, independently
  interactive building actor.
- `StaticAssembly` creates one RuneSchema-owned parent and batches matching
  cooked meshes into HISM components with their authored collision.
- `NativePieceIds` provides a safe hybrid: a static house or castle shell plus
  selected native doors, stations, storage, or other interactive pieces.
- Static assemblies preserve Base Builder local transforms under the requested
  center coordinate and layout yaw.
- The server and clients reconstruct the same non-replicated assembly from the
  shared mod definition. This avoids relying on unsupported HISM instance
  replication while retaining authoritative server collision.
- A missing class, mesh, or cooked BodySetup rejects only the affected piece
  and emits a `[DEGRADED][BUILDING-ASSEMBLY]` message.
- Shared cooked BodySetup data is never modified at runtime.

The owned-content save cleanup remains scoped to RuneSchema identities. The
broad save scan that caused slow startup and corrupted-load behavior is not
restored.
