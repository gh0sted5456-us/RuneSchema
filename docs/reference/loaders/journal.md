# Journal loader

**Folder:** `RuneSchema/mods/<ModName>/journal/`

Creates journal and recipe-discovery entries.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

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

`AddTo` may target a full subcategory path or an unambiguous loaded category.
Groups may be created where the native category supports them.

Journal and lore cleanup uses the same Safe Clean rule on Steam/GOG and Game
Pass. Definitions load independently from character cleanup.

## Rules

- `Unlock` controls whether the entry is granted.
- Placement/grouping and unlocking are separate.
- Use full category paths when short names are ambiguous.
- Keep stable IDs after release.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
