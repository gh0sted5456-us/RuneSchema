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

## Simple rules

- Vendors are reusable store definitions; bind them from an NPC, dialogue action, or supported recipe contribution.
- Stock ordering is evaluated within each category.
- Purchase authority, stock refresh, reconnect, and save/reload should all be tested.

## FAQ

### FAQ-VENDORS-001 — Does defining a vendor automatically place a merchant in the world? {#faq-vendors-001}

No. The vendor is a reusable store definition. Bind it from an NPC `VendorID`,
a supported dialogue vendor action, or a recipe contribution.

### FAQ-VENDORS-002 — Is item Order global across the whole vendor? {#faq-vendors-002}

No. Ordering is within each category.

### FAQ-VENDORS-003 — Can an NPC and dialogue both reference the same vendor definition? {#faq-vendors-003}

Yes. Vendor definitions are reusable and can be bound from supported NPC or
dialogue flows.

## Working examples

- [Great Tree: Crag's treasury with a night-only category](../examples/GreatTree/vendors/85-Crag.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
