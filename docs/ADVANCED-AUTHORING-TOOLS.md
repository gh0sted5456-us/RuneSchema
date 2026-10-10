# Advanced Authoring & Tools

RuneSchema 0.7.7.7 gives the in-game authoring page one full-width, top-to-bottom
workflow. It can help you find a loaded record, prepare an editable JSONC
starter, check its references, and export it. Opening the page, searching, and
building a preview do not install a mod or change the game data.

## Open the tools

Open RuneSchema Settings, then **Authoring & Tools**. If the tools are off,
enable **Mod authoring tools** under **General > Runtime** and restart. When
the game is ready, choose **Activate Tools for this session**. Activation is
temporary; it does not become a permanent setting.

The **Workflow stage** menu contains Mod Authoring, Presets, Traces, Inspector,
Results, and Save Cleanup. **Previous stage** and **Next stage** move through
that order. Recent results are at the bottom of the page. Save Cleanup is last
because it is a separate offline operation, not part of creating a mod.

## Create an editable starter

1. Choose **Mod Authoring** in Workflow stage.
2. Choose a **Content type**, or use **All Loaders** for the complete loader list.
3. Choose the loader and **Format**. **Basic starter** is the default. Use
   **$Patch** to change supported existing content, **$Clone** to create a
   supported copy, or **Fields reference** for research only. Unsupported
   formats are disabled for that loader.
4. Enter search text and select a result. The page says whether it searches
   installed authored files or loaded game names and paths. An inactive mod's
   authored file can appear in search results; that does not activate the mod.
5. Select the fields you need and choose **Build preview**. For a new item
   clone, set the mod folder and new name and generate a permanent persistence
   ID once. Keep that ID unchanged after release.
6. Review the draft. Use **Check changes and dependencies** to check the
   proposed changes and loaded references. Resolve reported problems before
   using **Export JSONC**.
7. Put the exported file in the matching folder of your own mod, then restart
   and test it on a backed-up character.

**Export editable reference JSONC** and **Fields reference** are research
outputs, not installable mods. The structural schema for the selected loader
can be exported from **Structural schema**. Under **All Loaders**, **Export
structural loader schemas** provides the full set.

Items, recipes, journal entries, quests, NPCs, spawns, and events have focused
content choices. Player and nameplate pages include their own guided controls;
the Items page also points to Helpy's Item Lab for in-world item authoring.

## Know what each stage is for

| Stage | Use it to |
| --- | --- |
| Mod Authoring | Find records and export loader definitions. |
| Presets | Run supported diagnostic presets. |
| Traces | Narrow a live behavior or event path. |
| Inspector | Examine a target when a path or field is uncertain. |
| Results | Review completed tool output. |
| Save Cleanup | Perform a separate offline save operation only when you intend to. |

Authoring tools cannot make an incorrectly cooked PAK valid, and a preview is
not proof that a mod works in a world. Follow the [first-mod guide](AUTHORING-GUIDE.md)
and test first entry, return to menu, re-entry, and a full restart.
