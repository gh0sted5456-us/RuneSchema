---
channel: experimental
promote_to: CONSUMED-OWNER-PRESENTATION.md
---

!!! info "Pre-release · 0.8.1.0"
    The native route compiles and its receipt/declaration policies have regression
    coverage. Connected owning-client delivery still needs shipping Steam,
    Game Pass and server acceptance. [Candidate and limits](PRE-RELEASE.md).

# Open a mod-owned interface after consumption

A mod owns its consumable, cooked interface component, UI, profile schema and
Apply/Cancel behavior. RuneSchema can route a successful registered consumption
to that character's owning client. It does not build a menu or write a profile.

## Registry declaration

Declare the normal consumed-item authority and its owner-only consumer together:

```json
{
  "Entries": [{
    "Id": "special_drink",
    "Kind": "GameplayAuthority",
    "Authority": {
      "Action": "ConsumedItemAuthority",
      "GraphClass": "/YourMod/Items/BP_SpecialDrink.BP_SpecialDrink_C",
      "DataAsset": "/YourMod/Items/ITEM_SpecialDrink.ITEM_SpecialDrink",
      "Function": "OnConsumeSuccess",
      "OwnerPresentation": {"Connection": "editor", "Key": "open-editor"}
    },
    "Consumer": {
      "Connection": "editor",
      "Class": "/YourMod/UI/BPC_EditorConsumer.BPC_EditorConsumer_C",
      "Function": "OpenForController",
      "AllowedKeys": ["open-editor"],
      "Audience": "Owner"
    }
  }]
}
```

These are example paths. Use the actual registered ItemData and its real
consumable class. A registered runtime ItemData clone can use its actual
`/Game/RuneSchema/<owner>/Items/<internal-name>.<internal-name>` path; its
canonical identity and successful native registration remain prerequisites.
An accepted path is not proof that an object exists or has the required type.

`OwnerPresentation.Connection` must match this entry's consumer. Its key must
be allowed by that consumer. `Audience: Owner` requires this consumption
declaration and cannot be used as an ordinary multicast presentation route.

## Cooked consumer callback

The consumer is a cooked ActorComponent Blueprint with this public, local
function or event. It must have exactly this input pin and no RPC or
output parameters:

```text
OpenForController(PlayerController: /Script/Engine.PlayerController)
```

For an owner-only consumption, RuneSchema creates a non-replicated component on
the **local consuming PlayerController**. It first reuses an existing component
of the registered class on that controller. The component's `GetOwner` returns
the controller, and RuneSchema passes that verified controller directly to the
callback. The mod can obtain its current pawn and
native character GUID from that controller; it need not trust a transmitted
actor path or choose a global last-used profile. The internal bounded JSON
notification contains `key`, `connection` and `characterGuid`, for example:

```json
{"key":"open-editor","connection":"editor","characterGuid":"00112233445566778899AABBCCDDEEFF"}
```

A mod's hotkey host should find that same class on the local controller and
create it only when absent. This lets potion and hotkey entry share one editor
and input/camera lifetime across pawn changes.

Use that actor context for preview and UI ownership. Show or focus the mod's
existing editor rather than creating duplicate windows. Profile persistence
belongs in the mod's explicit Apply operation. Neither receipt of this event
nor receipt of another player's cosmetic state is a profile commit.

## Admission and lifetime

RuneSchema pairs the registered native consume-success callback with successful
authoritative removal of one matching ItemData. Either native event order is
supported. An authority-requested callback uses the same removal admission;
an observed native callback is not invoked again merely because its bridge
request arrived later. This covers the local authority path as well as the
remote request path at the source-policy level.

Delivery uses the existing reliable `ClientRuneSchemaNotification` RPC on the
consuming pawn's networking component. The receiving client checks the local
controller, current pawn and actual GUID before dispatching its registered
consumer. Other clients do not receive a multicast open-editor event. Repeated
delivery receipts do not invoke the consumer again; out-of-order ready receipts
are tracked separately. Expired admission, destroyed owners and world changes
cannot be used to issue another presentation.

Dedicated servers never instantiate the UI component. Existing world-audience
presentation consumers retain their GameState ownership. No new polling hook,
Lua scheduler, RuneSchema native class library or separate framework DLL is
required. The normal registry/network bridge and matching client/server code
are required. Unavailable local consumers and expired owner context are
diagnosed. Enqueueing an RPC does not confirm that the interface appeared.

## Remaining verification

The portable tests exercise event ordering, missing removal, wrong owner/key,
late bridge acknowledgement, expiry, world reset and duplicate delivery.
Native compilation verifies the current C++ SDK calls. Those checks do not
verify rendering, real network ownership/replication timing, a mod's actual
consumable class, its runtime clone, or its cooked callback. Those need a
disposable shipping-game test before this route is published or installed.

This route does not establish frontend selection/creation GUID handoff,
Game Pass AppData SaveGame placement, general Blueprint mod-data networking
or a new companion-save file API.

`Consumer.Function` is explicit for owner-only routes and may use another local
function name with the same one-controller signature. Existing world-audience
consumers retain their five-argument `OnRuneSchemaPresentation` callback.
