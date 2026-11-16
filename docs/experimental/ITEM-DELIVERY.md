---
channel: experimental
promote_to: ITEM-DELIVERY.md
---

!!! info "Experimental"
    Item Delivery is included in the [0.8.1.0 pre-release](PRE-RELEASE.md).
    Live grants and character save acknowledgement still need verification.

# Item Delivery

Open **Settings → RuneSchema → Item Delivery** on the host. Choose **Off**, **Deliver Once** (the default), or **Always Deliver**, save the settings, and restart Dragonwilds. Always Deliver runs once per actual join. A respawn keeps the same delivery session.

Place an `item-delivery.json` file at the root of an enabled RuneSchema mod. Each key is a stable delivery ID. Each list contains full ItemData object paths. Replace the example paths with paths from your installed content.

```json
{
  "starter-kit": [
    "/Game/Example/ITEM_First.ITEM_First",
    "/Game/Example/ITEM_Second.ITEM_Second"
  ],
  "returning-player-kit": [
    "/Game/Example/ITEM_Third.ITEM_Third"
  ]
}
```

The authority waits for possession, a native character GUID and the character's loaded inventory. It gives one of each configured item. Deliver Once tracks the mod folder, delivery ID, and item path for each character. Adding an item to an existing delivery makes that item eligible on the next join.

Unresolved paths and full inventory slots do not block other items. Full items can be retried on the next join. `RS-ITEM-002` identifies unresolved paths in RuneSchema.log.

Receipts live under RuneSchema's platform AppData runtime in `saved/progress/item-delivery/`. RuneSchema writes a hold before granting and records completion after a matching successful character save. If a grant or save cannot be confirmed, the held entry stays on disk across restarts for review.
