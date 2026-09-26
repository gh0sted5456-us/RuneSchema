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

## Simple rules

- Events run server-owned waves from `/spawns` definitions marked `EventOnly:true`.
- Create the spawn template before the event that uses it.
- Start or cancel events through supported dialogue actions; ground-relative Z forms resolve against blocking ground.

## FAQ

### FAQ-EVENTS-001 — Do EventOnly spawn templates place permanent actors by themselves? {#faq-events-001}

No. `EventOnly:true` marks a spawn definition as a template for `/events`;
it is not a permanent world placement.

### FAQ-EVENTS-002 — Can an event be used to scope quest kill credit? {#faq-events-002}

Yes. Event IDs can be used to scope kill credit for quest objectives.

### FAQ-EVENTS-003 — Does an event spawn definition need to exist first? {#faq-events-003}

Yes. Create the referenced `/spawns` template before the event that uses it.

### FAQ-EVENTS-004 — How do ground-relative event locations work? {#faq-events-004}

`# Events loader

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

## Simple rules

- Events run server-owned waves from `/spawns` definitions marked `EventOnly:true`.
- Create the spawn template before the event that uses it.
- Start or cancel events through supported dialogue actions; ground-relative Z forms resolve against blocking ground.

## FAQ

### FAQ-EVENTS-001 — Do EventOnly spawn templates place permanent actors by themselves? {#faq-events-001}

No. `EventOnly:true` marks a spawn definition as a template for `/events`;
it is not a permanent world placement.

### FAQ-EVENTS-002 — Can an event be used to scope quest kill credit? {#faq-events-002}

Yes. Event IDs can be used to scope kill credit for quest objectives.

, `$+offset`, and `$-offset` resolve Z against blocking ground.

### FAQ-EVENTS-005 — How are events started or cancelled? {#faq-events-005}

Use the supported dialogue actions for event start and cancellation.

## Working examples

- [Great Tree: five-wave tribe trial](../examples/GreatTree/events/85-GreatTree-TribeTrial.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
