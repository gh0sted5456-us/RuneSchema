# Vendors loader

**Folder:** `RuneSchema/mods/<ModName>/vendors/`

Reusable RuneSchema stores, categories, stock, pricing, and gates.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Vendors define reusable stores bound from NPCs or dialogue.

```json
{
  "Id": "shop",
  "MerchantName": "Supply Shop",
  "Repairable": false,
  "Items": [{
    "Category": "Supplies",
    "Item": "/Game/Gameplay/Items/ITEM_Log.ITEM_Log",
    "Currency": "/Game/Gameplay/Items/ITEM_Coin.ITEM_Coin",
    "Price": 2,
    "Count": 10,
    "Order": 10
  }]
}
```

Bind the store with an NPC `VendorID`, a dialogue vendor action, or a recipe
contribution. Ordering is within each category. Confirm purchase authority,
stock refresh, reconnect, and save/reload.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
