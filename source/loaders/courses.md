# Courses loader

**Folder:** `RuneSchema/mods/<ModName>/courses/`

Course definitions, patches, checkpoints, orbs, props, and zones.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/courses` to add course records or patch a course by ID. A course can
define a starter, finish, orb sets, stamina orbs, props, and a zone.

```json
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

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
