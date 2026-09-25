# Events loader

**Folder:** `RuneSchema/mods/<ModName>/events/`

Server-owned timed encounters and waves driven by spawn templates.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Events run server-owned waves from `/spawns` entries marked `EventOnly:true`.

```json
[
  {
    "Id": "night_attack",
    "TimeOfDay": "Night",
    "TimeoutSeconds": 900,
    "Waves": [[
      {"SpawnID":"night_raider","Location":[1000,2000,"$+10"]}
    ]]
  }
]
```

Create the spawn template first, then start or cancel the event from dialogue.
`$`, `$+offset`, and `$-offset` resolve Z against blocking ground. Event IDs can
scope quest kill credit.

## Working examples

- [Great Tree: five-wave tribe trial](../examples/GreatTree/events/85-GreatTree-TribeTrial.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
