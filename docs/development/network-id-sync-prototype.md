# Pre-join registry ID synchronization prototype

Status: experimental source, disabled by default. Maintainer tests opt in with
`RUNESCHEMA_NETWORK_ID_SYNC=1`. There is no Quick settings switch and no default
enablement in this update.

## Authority and connection lifecycle

The server owns the order of shared RuneSchema-appended native registry entries.
An incoming client can adopt that order before native world loading and join.
Client-only entries remain after the shared range. Protected original game
entries are compared in order and never reordered.

The implementation updates both `NetIdToData` and `DataToNetIdMap`, checks their
round trips, audits the result against the authority and rolls back a failed
transaction. Plans cover all lanes before any write. Actor NetGUIDs and placed
building IDs remain game-owned; no character/world save or platform mapping is
rewritten.

Each incoming connection gets a separate plan. Reconnects use the client's
current order rather than assuming the previous authority's order. The server
and already connected clients are not reordered for a new player.

## Compatibility and transport

- The native welcome token remains limited to 48 KiB; decompressed JSON is bounded to 2 MiB with bounded nesting.
- Source rosters and canonical registry ordering are compressed before transport.
- Executable identity, native call-site bytes and the welcome vtable slot are verified before hooks are installed. Unsupported executables leave the feature unavailable.
- Mod-version labels and compatible client extras produce warnings rather than blanket rejection.
- Unverified native prefixes, identities/classes, ownership or unsupported lanes cause alignment to be skipped while retaining native entry handling. Malformed transport is a separate validation failure and is never applied.
- A missing class or cooked asset cannot be supplied by an ID list. Existing native gameplay checks and server authority remain active.
- Existing nonempty native redirects are preserved. A legacy peer has no pre-join alignment guarantee.
- Generated quest/journal entries are tracked when committed through RuneSchema's native registration adapter; unrelated appends are not claimed as verified ownership.
- No extra network port, per-frame discovery callback or new cooked bridge is required.

## Verification and remaining work

Contract tests cover malformed input, protected prefixes, changed IDs/classes,
client extras, repeated alignment, independent late-client plans and rollback.
Isolated authenticated Game Pass checks verified world entry, reconnect, one
native item replication and one recipe-ID notification after deliberate ID
permutations. These checks do not verify a complete crafting transaction.

Simultaneous populated authenticated-client gameplay, further late joins and
complete crafting interactions remain required before default enablement.
Private permutation helpers and verification guards are absent from production
source and installed DLLs. Test profiles, credentials, save files and private
runtime logs are not publication inputs.

The general data relay has its own [transport bounds](../PRESENTATION-TRANSPORT.md):
256 declared channels, 64 KiB complete messages by default and an optional 1 MiB
maximum. These do not enlarge the native 48 KiB welcome field.
