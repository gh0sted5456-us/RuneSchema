# Mod achievements

Any mod, UE4SS or RuneSchema, can define achievements that players earn while they play. When one unlocks, Helpy raises an
award card labeled "RuneSchema Gamer Points" from the bottom of the game. The native HUD presents the card
while the game keeps focus and input. The RuneSchema mark opens to reveal the achievement and points, then
reassembles before closing. With Windows animations disabled, the card shows the award directly.
The points go to the player's RuneSchema
profile (Helpy > Profile > Mod achievements). On a server running RuneSchema and Helpy, other players can see
those points in the Player profile window.

## Where the file goes

Your mod does not need to be a RuneSchema mod. The **Helpy Achievements** plugin
(`Helpy/plugins/RuneSchema.Achievements`, installed with RuneSchema) looks for the file in both kinds of mod:

| Mod type | File |
| --- | --- |
| UE4SS mod | `ue4ss/Mods/<your mod>/achievements.json`, or `cache/achievements.json` |
| RuneSchema mod | `RuneSchema/mods/<your mod>/achievements.json`, or `cache/achievements.json` |

A UE4SS mod counts while it is enabled in `mods.txt` or has an `enabled.txt`. A RuneSchema mod counts while
it is enabled on the Mods page. Rebuilding a RuneSchema mod's Helpy cache keeps `cache/achievements.json`.
If a mod has both files, the one in its own folder is used.
RuneSchema mods with the file show an **Achievements** badge on the Mods page.

Players earn achievements while RuneSchema and Helpy are installed: the game events come from RuneSchema's
core, and Helpy tracks progress. If the plugin is removed, or turned off with `RuneSchema.Achievements : 0` in
`RuneSchema/settings/plugins.txt`, achievements are off and points already earned are kept.

Achievement IDs are scoped to the mod folder (`ue4ss/<folder>` or `mods/<folder>`), so two mods can use the
same `id` without clashing.

## Format

```json
{
  "format": 1,
  "achievements": [
    {
      "id": "goblin-slayer",
      "title": "Goblin Slayer",
      "description": "Defeat 25 goblins.",
      "points": 10,
      "conditions": [
        { "event": "kill", "subject": "/Game/Mods/Goblins/*", "count": 25 }
      ]
    },
    {
      "id": "chief",
      "title": "Chief Toppled",
      "description": "Defeat the goblin chief and craft his totem.",
      "points": 25,
      "hidden": true,
      "conditions": [
        { "event": "custom", "function": "/Game/Mods/Goblins/BP_Chief.BP_Chief_C:OnDefeated" },
        { "event": "craft", "subject": "/Game/Mods/Goblins/DA_Recipe_Totem.DA_Recipe_Totem" }
      ]
    }
  ]
}
```

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Unique within your mod; 1–64 letters, digits, `-`, `_` or `.`. **Never rename it**: progress is keyed by it. |
| `title` | yes | Up to 64 visible characters. |
| `description` | no | Up to 200 characters. |
| `points` | no | 0–100 (default 0). One mod may award at most 1000 in total. |
| `hidden` | no | Show "Hidden achievement" until it is unlocked. |
| `match` | no | `"all"` (default): every condition must be met. `"any"`: one is enough. |
| `conditions` | yes | 1–8 conditions. |

A mod may define up to 256 achievements. The file may contain comments.

### Conditions

| `event` | Counts | `subject` is |
|---|---|---|
| `kill` | Kills confirmed by the game and credited to you, including your summons | The victim's class, e.g. `/Game/Mods/Goblins/BP_Goblin.BP_Goblin_C` |
| `craft` | Crafts, times the craft count | The recipe used |
| `recipe-unlocked` | Recipes newly unlocked | (none) |
| `building-unlocked` | Buildings newly unlocked | (none) |
| `building-placed` | Buildings you place | The building piece |
| `quest-updated` | Each update to a quest you have | The quest |
| `skill-xp` | XP gained | The skill data asset |
| `item-used` | Items used from your inventory | (none) |
| `death` | Your deaths (counted when you respawn) | (none) |
| `custom` | Calls to a Blueprint function in your mod | Use `function` instead: `/Game/...BP_X.BP_X_C:FunctionName` |

Optional fields on every condition:

- `subject`: an exact path, or a prefix ending in `*` (`/Game/Mods/Goblins/*` matches every goblin class). If you
  leave it out, any subject counts.
- `count`: how much to accumulate (default 1).
- `atLeast` (`skill-xp` only): unlock when the skill's total XP reaches this value. Use it instead of counting XP
  gained.
- `where`: up to 8 `"Parameter.Path": value` pairs. They must equal the captured function parameters, e.g.
  `{"UpdatedQuest.bComplete": true}`. Use Helpy's Player Trace with parameter capture to see the field names your
  event reports.

### Custom events

Add a function to your Blueprint (for example `OnDefeated`) and call it when the moment happens. Then reference it
as `"event": "custom", "function": "/Game/Mods/Goblins/BP_Chief.BP_Chief_C:OnDefeated"`. Helpy asks RuneSchema to
watch only the functions that enabled mods reference, up to 64 in total.

A custom event counts on the machine where the function runs. In multiplayer, make it run on each player's
machine that should earn the achievement, for example as a Multicast or Client event. A function that runs only
on the server credits only the host. Calls that belong to another player's pawn never count for you.

## Rules worth knowing

- Progress is **profile-wide**: every character on the RuneSchema profile adds to the same achievements.
- Points are fixed when an achievement unlocks. Changing `points` later, or disabling the mod, never changes
  points already earned.
- Only the local player's actions count. On a server, the host relays your confirmed kills to your game.
- Progress is stored on the player's PC (`runeschema.profile/achievements.json`). Shared points are what that
  player's Helpy reports, and Helpy labels them as not verified by the server.
- A problem in `achievements.json` never stops the mod from loading. Helpy lists the first error under
  Profile > Mod achievements.
- Players can turn off the popup or its chime in Diagnostics > Notifications. Achievements still unlock.

## What other players see

When both sides run RuneSchema and Helpy with achievements support, Helpy shares with other players on the
server:

- the player's total points and unlock count;
- the five highest-scoring mods;
- the last three unlocks.

The summary is at most 1 KiB. It travels next to the public profile, so older Helpy builds keep showing names and
links and ignore the points.
