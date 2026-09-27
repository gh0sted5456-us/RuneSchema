# RuneSchema 0.7.6.4

Use `RuneSchema-0.7.6.4-Universal.zip` for RuneSchema core plus the bundled
optional plugins. Use `RuneSchema-0.7.6.4-Core.zip` when only RuneSchema core is
needed.

The RuneSchema DLL is shared by Steam/GOG and Game Pass. Install the UE4SS
runtime made for your storefront, then install either RuneSchema package.

This release aligns journal and lore save cleanup across both storefronts.
RuneSchema waits until the game has loaded the character, removes only missing
content previously recorded as RuneSchema-owned, verifies the result, and lets
the game save normally. Xbox Game Save files are not edited directly.

The release also includes the current loader, recipe, building, equipment,
registry, character-creation, Helpy, logging, and performance improvements
documented on the RuneSchema website.
