# Journal loader

**Folder:** `RuneSchema/mods/<ModName>/journal/`

Recipe discovery and authored journal entries.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/journal` for recipe and discovery entries.

```json
{
  "RS_MyRecipe": {
    "Type": "Recipe",
    "DisplayName": "My Recipe",
    "RecipeData": "RECIPE_MY_ITEM",
    "ItemData": "/Game/MyMod/Items/ITEM_MyItem.ITEM_MyItem",
    "PageDescriptions": [{"Description":"First page."}],
    "Unlock": true,
    "AddTo": {
      "SubCategory": "/Game/UI/JournalData/JOURNAL_SC_Recipe.JOURNAL_SC_Recipe",
      "Key": "RS_MyRecipe"
    }
  }
}
```

`AddTo` can target a full subcategory path or an unambiguous loaded category.
Groups can be created where the native category supports them. Verify the
entry, pages, icon, unlock, save, and reload.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
