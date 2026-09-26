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

### FAQ-COURSES-003 — What do $Patch and $Target do? {#faq-courses-003}

They let a course definition patch an existing authored course instead of
creating a separate course record.

### FAQ-COURSES-004 — Are course coordinates written in meters? {#faq-courses-004}

No. Course locations use Unreal centimeters.

### FAQ-COURSES-005 — Does the course zone need to cover the playable route? {#faq-courses-005}

Yes. Verify that the authored zone encloses the route players are expected to
run.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
