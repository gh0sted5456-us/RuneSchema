# Shared networking content

This is RuneSchema-owned transport content, not an NPC mod dependency or a
replacement UE4SS host. The October 16 correction recooks the six existing
networking components without changing their mounted object paths.

The old shipped identity component lacked `IdentityPayload` and its notify
callback even though the editable asset and current authoring source had them.
A private Steam CL248854 menu-only comparison reproduces those two failures in
the old containers and passes the identity, primary registry RPC and world-state
checks with the replacement. No test constructs a gameplay actor or edits saves.
Dedicated/co-op and actual NPC presentation remain live acceptance work.

`CONTENT-RECEIPT.json` pins the verified complete PAK/IoStore triplet. Release
packaging checks both its byte lengths and SHA-256 hashes, preventing a cached
old container from silently replacing the current contract.

Editable content is retained under `EditablePlugin/RuneSchema`. Use the engine
5.6 editor-only module in `source/UnrealAuthoring/RuneSchemaBridgeAuthoring` and
call `RuneSchemaBridgeAuthoringLibrary.author_network_bridges()` explicitly from
an editor Python commandlet before cooking. Inspect with
`inspect_identity_contract()`: the scalar String must have native size 16,
Net/RepNotify and `OnRep_IdentityPayload`. Commandlets do not rely on the delayed
interactive-editor ticker. Cook `/RuneSchema/Networking` and package only those
six components using the native IoStore metadata plus the content-plugin
descriptor; do not distribute a replacement global container or editor DLL.

The explicit entry point is idempotent and its compiled contract check must
succeed before saving a release candidate. A new cook requires new native
validation and a refreshed receipt; updating a version label is not validation.
