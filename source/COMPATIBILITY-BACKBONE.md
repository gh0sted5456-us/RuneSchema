# Compatibility

RuneSchema 0.7.5.28 supports Steam/GOG and Game Pass/WinGDK with one
storefront-aware runtime.

## Storefronts

### Steam / GOG

RuneSchema can use validated Steam/GOG native bindings where required and falls
back to UE4SS metadata or Unreal reflection where supported.

### Game Pass / WinGDK

RuneSchema uses the WinGDK runtime lane. Steam-only byte patterns are not used
on Game Pass.

Xbox Game Save data is provider-managed. RuneSchema does not treat the WGS
database as loose Steam JSON files.

## Mappings

RuneSchema looks for an optional `.usmap` in these locations:

1. `Mods/RuneSchema/dlls/mappings`
2. the UE4SS root
3. `ue4ss/mappings`
4. older RuneSchema mapping locations kept for compatibility

Mappings help type queries and diagnostics. Live Unreal reflection still
decides whether a runtime write is valid.

## Plugins

Plugins are optional.

A plugin with an incompatible native ABI can be skipped without disabling
RuneSchema core. Plugin PAK content that is otherwise valid can remain
independent from a native DLL.

Helpy is not required by the loader system.

## Multiplayer

Gameplay mutations remain server-owned. Clients need the cooked assets required
for anything they render.

`/registry` connects server actions with client presentation, but a registry
entry does not bypass server validation.

## Native feature fallback

Some features use game-build-specific native hooks. If a hook cannot be
validated for the current executable, RuneSchema leaves that feature off and
continues with unrelated loaders and services.

A game update may therefore temporarily affect one native feature without
breaking normal JSON authoring.

## Saves

SafeSave removes only content with recorded RuneSchema ownership.

Steam/GOG and Game Pass use different save paths and cleanup lanes. Do not copy
Steam save-editing instructions onto the Game Pass WGS provider.

For user recovery steps, see
[Manual Save Recovery](MANUAL-SAVE-RECOVERY.md).

## FAQ

### FAQ-COMPAT-001 — Is there a separate RuneSchema DLL for Steam and Game Pass? {#faq-compat-001}

No. RuneSchema 0.7.5.28 uses one storefront-aware runtime with separate native
lanes internally.

### FAQ-COMPAT-002 — Does Game Pass use Steam-only native byte patterns? {#faq-compat-002}

No. The WinGDK lane does not use Steam-only pattern scans.

### FAQ-COMPAT-003 — Is a USMAP required for RuneSchema to work? {#faq-compat-003}

No. It is optional. Live Unreal reflection still validates runtime writes.

### FAQ-COMPAT-004 — Can one incompatible plugin DLL disable RuneSchema core? {#faq-compat-004}

No. A native ABI mismatch can skip that plugin DLL without disabling the core
runtime.

### FAQ-COMPAT-005 — Is Helpy required for loader functionality? {#faq-compat-005}

No. Helpy is optional.

### FAQ-COMPAT-006 — Does /registry bypass server validation? {#faq-compat-006}

No. Registry entries connect authority actions with client presentation, but
server-owned actions are still validated by the server.

### FAQ-COMPAT-007 — Can a game update disable only one native feature? {#faq-compat-007}

Yes. If a build-specific hook no longer validates, RuneSchema can leave that
feature off while unrelated loaders and services continue.


For storefront detection, hook validation, WGS internals, and build-specific
details, see the [Developer Guide](DEVELOPER-GUIDE.md).
