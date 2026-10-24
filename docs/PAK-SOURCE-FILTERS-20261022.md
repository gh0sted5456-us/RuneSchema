# Source filter identities

The item source selector retains its broad categories and also offers individual mod/plugin providers. Mod choices use the same ID.txt/mod.json nickname/name metadata as the mod list, with personal nicknames taking precedence. Indexed container owners, native plugins, registered mod names and current catalogue rows supply choices; the current page alone does not determine which mods can be selected.

Filtering uses stable `mods/<owner>` / `plugins/<owner>` tokens, never friendly labels or PAK filename guesses. The native catalogue, fallback asset search and Helpy's local filtering all enforce that identity. Conflicting or unknown suppliers do not match a named provider. The encoded control token is validated separately from user search text and composes with category/family/loadout filters. Existing broad filter preferences still load.

Display names travel as bounded presentation metadata in the source manifest. Renaming does not alter source fingerprints, catalogue validity, asset ownership or gameplay permissions. Provider choices are calculated only while the source dropdown is open.

## Outstanding furnace contract

The Dye_Quests furnace still fails registration in the installed Steam build. Its native smelter class lacks the verified save-exclusion contract accepted by managed BuildingProps. The scoped ShouldSkip adapter supports reflected implementations; it cannot intercept a native-only interface implementation that bypasses ProcessEvent. RF_Transient alone is not proof of SPUD exclusion.

Do not remove the guard or report this issue fixed. A verified native exclusion adapter, or a cooked buildable subclass implementing the SPUD ShouldSkip contract, is needed before a world-save/load/removal test can establish safe support. This follow-up makes the refusal identify the concrete buildable class and distinguish a missing hook from a missing class contract; it does not suppress the error or spawn unsafe actors.
