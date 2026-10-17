# Shared quest-scroll content

The runtime distribution is `source/plugins/RuneSchema.QuestItems`: a content-only
plugin with one Blueprint and one inert cloneable ItemData template. Its verified
container hashes are locked by `CONTENT-RECEIPT.json` and checked when packaging.
It contains no authored quest, reward, JobBoard dependency or extra native DLL.
The code/content authored for this helper uses RuneSchema's MIT license; the
referenced Dominion classes/game content are not redistributed here.

`EditablePlugin/RuneSchemaQuestItems` preserves the editable `.uplugin` and two
uncooked assets. `SourceReproduction` preserves the authoring adapter and scripts
from the Quest Board content handoff. These are source/reference inputs, not
standalone game plugins to copy into a player's game directory. The scripts
expect the maintained Dragonwilds/Dominion authoring-mirror project and its
Unreal 5.6 editor tooling; adapt original workspace paths before running them.
No editor-only authoring module or mirrored Dominion DLL is shipped at runtime.

The shared UseItem graph has only entry/result, linked as an action returning
false, with `BlueprintPure=false`; it preserves the native const signature.
RuneSchema intercepts it for explicitly bound scrolls before consumption. Inert
editor invocation and native container decoding passed, but real world use,
save/reload, controller/hotbar and dedicated/co-op acceptance still need tests.

See `docs/QUEST-SCROLLS.md` for cooked and schema-only authoring. Never reuse the
template's PersistenceID for a clone, or embed quest rewards in this shared base.
