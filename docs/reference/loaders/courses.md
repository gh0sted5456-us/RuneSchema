# Courses loader

## Step-by-step workflow

Start with the example closest to your idea. Replace its sample paths and IDs
with values from your game, then use the
[testing checklist](../../modders/testing-and-compatibility.md) to try it out before sharing.

1. Plan starter, finish, checkpoints/orbs and the playable area in Unreal centimeters.
2. Create `courses/10-camp-route.jsonc` from the mock-up below; replace the ID and coordinates.
3. Populate the actual route and enclosing zone. Use `$Patch`/`$Target` only when deliberately changing an existing authored course.
4. Restart and run the route from start through completion; inspect missing assets and out-of-zone checkpoints.

**Related guides:** [Spawns](spawns.md) places separate world content; [players](players.md) controls player presentation, not course geometry.


**Folder:** `RuneSchema/mods/<ModName>/courses/`

Course definitions, patches, checkpoints, orbs, props, and zones.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Use `/courses` to add course records or patch a course by ID. A course can
define a starter, finish, orb sets, stamina orbs, props, and a zone.

```jsonc
[
  {
    "Id": "mymod_course",
    "StarterLocation": [1000, 2000, 300],
    "FinishLocation": [2000, 2000, 300],
    "Orbs": []
  }
]
```

Use `$Patch` with `$Target` for an existing authored course. Keep points in
Unreal centimeters and verify the zone encloses the playable route.

## Simple rules

- Use `/courses` to add a course record or patch an authored course by ID.
- Keep coordinates in Unreal centimeters.
- Verify that the course zone encloses the route players can actually run.

## FAQ

### FAQ-COURSES-001 — Can I patch an existing authored course instead of replacing it? {#faq-courses-001}

Yes. Use `$Patch` with `$Target` for an existing authored course.

### FAQ-COURSES-002 — What units do course locations use? {#faq-courses-002}

Unreal centimeters. Keep starter, finish, orb, prop, and zone placement in the
same coordinate convention.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
