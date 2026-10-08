# Steam/GOG and Game Pass native lanes

RuneSchema ships one `main.dll`. The storefront determines which native bindings
may run; JSON loaders and the plugin API are shared. These notes describe the
current Unreal 5.6.1 / Dragonwilds 100.4.0.0 work. RVAs are relative to the
game executable image, **not** stable addresses or offsets to copy to another
game build.

| | Steam / GOG (`Win64`) | Game Pass (`WinGDK`) |
|---|---|---|
| Selection | Executable path contains `Win64`, Steam, or GOG and has no Windows package identity. | `WinGDK`/package executable path or Windows package identity; UE4SS signature folder is a last-resort clue. |
| Native lane | `steam-native` | `gamepass-native` |
| Core bindings | Steam AOB and call-site maps. | Separate WinGDK AOB and call-site maps; never try the Steam `GetObjectsOfClass` ABI. |
| `UDataTable::Serialize` | Steam AOB `48 89 5C 24 18 57 48 81 EC E0 01...`; UE4SS virtual fallback. | WinGDK AOB `48 89 5C 24 10 57 48 81 EC D0 01...`; same validated virtual fallback. |
| `FMemory::Free` | AOB begins `48 85 C9 74 2E 53...` with wildcard relative call. | Similar prefix, but the verified call displacement is different. |
| `FName::ToString_Wchar` | Resolve the target of AOB call site `E8 ?? ?? ?? ?? BE 01 00 00 00...`. | Direct AOB begins `48 89 5C 24 10 48 89 74 24 18 57...`. |
| Object enumeration | Steam call-site AOB for `GetObjectsOfClass`. | UE4SS hash tables/object array fallback; a similar-looking WinGDK iterator has an incompatible callback ABI. |
| Journal JSON save interface | Steam AOB contract for reader, writer, JSON helpers, and shared references. | Read writer/reader from the validated JournalComponent CDO interface; verify helper calls inside them. |
| Character saves | Steam path and native character format. | Xbox Game Save route; never edit as if it were a Steam file. |

Storefront detection is in `raw/include/Runtime/Storefront.h`. The exact core
patterns are in `raw/include/SDK/DragonWildsSignatures.h`; full journal
patterns are in `raw/include/Loader/JournalPersistenceContract.h` and
`raw/include/Loader/JournalWinGDKContract.h`. The AOB prefixes above are
identifiers for review, not complete signatures.

## Verified WinGDK journal and recipe anchors

These RVAs were measured for the WinGDK image in this work. RuneSchema uses
signatures or validated runtime relationships, not bare RVAs, for dispatch.

| Native routine | WinGDK RVA | Runtime validation |
|---|---:|---|
| Journal hierarchy insert | `0x76C86F0` | `JournalHierarchyInsertWinGDK` AOB plus executable target checks |
| Journal hierarchy builder | `0x6EA5DA0` | `JournalHierarchyBuilderLayoutWinGDK` AOB and layout checks |
| Journal category helper 1 | `0x713DD80` | Category AOB; target at call-site offset `0x12` |
| Journal category helper 2 | `0x713DDF0` | Category AOB; target at call-site offset `0x12` |
| Journal category helper 3 | `0x713DE60` | Category AOB; target at call-site offset `0x12` |
| Journal JSON writer | `0x6EA1980` | JournalComponent CDO interface slot 1, prologue and helper calls |
| Journal JSON reader | `0x6EA1BF0` | JournalComponent CDO interface slot 2, prologue and helper calls |

The JSON bridge validates calls relative to the WinGDK reader at `+0xD7`
(field lookup) and `+0x18E` (string conversion), and relative to the writer
at `+0x1CD` (set array), `+0x1F0` (shared release), and `+0x7C`
(name-string construction). Duplicate call sites must agree. The native
`UnlockedEntries` field lookup requires a **16-byte-aligned** key view; the
2026-09-27 Game Pass crash at executable RVA `0x17BFFB4` exposed this ABI
requirement. The bridge aligns its 16-byte JSON view, array, and shared
reference types explicitly.

The JournalComponent persistence interface is read at object offset `0xC0`.
The reflected `UnlockedJournalEntries` and `UnreadJournalEntries` arrays are
checked at offsets `0xC8` and `0xD8`. If these layout checks fail after an
update, RuneSchema leaves the native adapter unavailable instead of guessing.

Steam journal routines are resolved by their separate AOB contract at runtime;
we do not pin Steam RVAs because they have not been recorded and verified for
this exact executable build. Do not treat any WinGDK RVA as a Steam equivalent.

## Save behavior shared across lanes

`/journal` and `/lore` keep their existing transient save behavior. RuneSchema
`/recipes` use the native persistent `RecipesUnlocked` set once the live
RecipeData object has a valid PersistenceID. Recipe delivery is not gated by a
lagging Safe Clean registry snapshot. If a supplying mod is later deleted,
Safe Clean prunes orphaned recipe PersistenceIDs from character progress.
Journal/lore continue to use the storefront-validated save bridge to omit
temporary unlocks.

Quests use the game's native quest registration and save path. Character
customization and quest save behavior are the two explicit persistence
settings. Removing a mod must be handled by registry-based cleanup of only
unresolved RuneSchema quest identities; no general player-save ledger is
required.

When updating either executable, verify storefront selection, the core AOB
map, journal interface and layout checks, character/world re-entry, and a full
restart. A successful menu load alone does not establish lane parity.
