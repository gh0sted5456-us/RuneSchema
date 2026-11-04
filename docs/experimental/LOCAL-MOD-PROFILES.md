---
channel: experimental
promote_to: LOCAL-MOD-PROFILES.md
---

!!! info "Pre-release · 0.8.1.0"
    The opt-in API is included in 0.8.1.0. Shipping Blueprint dispatch and
    platform acceptance still need live verification; storage-policy tests are
    separate. [Candidate and limits](PRE-RELEASE.md).

# Local mod profiles

A mod can register its own cooked ActorComponent and use two local Blueprint
functions to read and commit a JSON profile. The mod owns the payload schema,
migrations, editor and Apply/Cancel behavior. RuneSchema supplies character
identity checks and durable AppData storage. A mod can continue using native
Unreal SaveGame objects instead when those meet its needs; these are separate
storage choices.

This API stores mod-owned character data. Exact native inventory reconstruction,
world persistence and automatic item restoration require their own adapters;
they are not enabled by registering a profile.

## Registration

Add an enabled mod's registry declaration:

```json
{
  "Entries": [
    {
      "Id": "body-profile",
      "Kind": "LocalProfile",
      "Profile": {
        "Class": "/YourMod/Runtime/BPC_Profile.BPC_Profile_C"
      }
    }
  ]
}
```

RuneSchema derives the namespace from the registry owner and this entry's `Id`.
Keep both stable across updates. Each component class has one namespace;
duplicate class bindings are rejected. The component must belong to the actual
local PlayerController. RuneSchema does not create or activate the component
merely because it declares storage.

`LocalProfile` entries contain only `Id`, `Kind` and `Profile`. They are local
capabilities and do not change network-content fingerprints or owner parity.
The exported `registry.schema.json` includes this declaration.

## Blueprint function contract

Define these **impure, local, non-RPC Blueprint functions** on that component.
Parameter names, directions and types must match exactly. Each function has six
parameters and no return-value pin.

| Function | Inputs | Outputs |
| --- | --- | --- |
| `RuneSchemaReadProfile` | `PlayerController`: Engine.PlayerController | `Success`: bool; `Found`: bool; `Revision`: int64; `Payload`: string; `Error`: string |
| `RuneSchemaWriteProfile` | `PlayerController`: Engine.PlayerController; `ExpectedRevision`: int64; `Payload`: string | `Success`: bool; `Revision`: int64; `Error`: string |

Give the original function bodies safe defaults: `Success=false`, numeric values
zero, `Found=false` where present, and an error such as "RuneSchema local profile
bridge unavailable". The original bodies must not write files or silently claim
success. RuneSchema intercepts registered calls through the Blueprint script VM,
including direct Blueprint-to-Blueprint calls, and fills the caller's actual
output parameters.

The supplied controller must equal the component's `GetOwner`, be local and
share its world. Its current pawn must belong to it and have a nonzero native
character GUID. Callers cannot supply a target GUID, namespace or filesystem
path. Dedicated servers, remote controllers, class defaults, stale actors and
unbound frontend drafts cannot read or write profiles through this API.

The frontend selection/creation GUID handoff is still under investigation.
Keep a character-editor draft unbound until the game supplies a verified
identity; never substitute a name or last-used character.

## Read, edit and Apply

1. Call `RuneSchemaReadProfile` for the component's current local controller.
2. If `Success=true` and `Found=false`, use the mod's defaults. The revision is
   zero and the payload is empty.
3. If found, decode the payload using the mod's schema and retain its revision.
4. Edit a separate draft. On explicit Apply, serialize a JSON object and call
   `RuneSchemaWriteProfile` with that retained `ExpectedRevision`.
5. Only a successful commit advances the revision. On a conflict, read the
   current profile and let the mod reconcile the draft. Do not force a retry
   using a guessed revision.

Payloads contain up to 256 KiB of UTF-8 JSON, with bounded nesting. Schema-version
labels inside them are informational to RuneSchema and are preserved. The mod
decides whether and how to migrate an older payload. There is no arbitrary-path
legacy-import endpoint.

Received cosmetic/network state must update the corresponding visual actor
only. It must not call this local profile writer. Server PINs and generic
authority messages do not expose profile storage operations.

## Storage and failure behavior

Profiles live beneath
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core/companion/`, grouped by a SHA-256
of the registry namespace and the actual character GUID. The internal envelope
records the namespace, GUID, monotonically increasing revision and payload hash.
The API returns only the mod's payload and revision.

Writes take an exclusive file lock, compare the existing revision, stage and
flush the complete file, atomically publish it, then verify the published bytes.
A stale revision, damaged identity/hash, redirected path or malformed payload
reports failure and preserves the existing profile. A failed stage leaves the
previous file intact. If publication cannot be verified, read the profile before
retrying because the commit may have completed. The bridge does not poll,
autosave or migrate a profile simply because a mod is mounted.

This storage is independent of Xbox WGS and the game's native character files.
Native Game Pass and Wine/Proton behavior still require platform testing; a
Windows storage test is not cross-platform acceptance.
