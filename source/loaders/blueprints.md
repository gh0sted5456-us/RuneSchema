# Blueprints loader

**Folder:** `RuneSchema/mods/<ModName>/blueprints/`

Supported reflected defaults on existing loaded classes and components.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/blueprints` for supported reflected defaults on an existing loaded
class or component. It does not create Blueprint classes.

```json
[
  {
    "/Game/Path/BP_Target": {
      "Data": { "Duration": 10.0 }
    }
  }
]
```

Confirm every field against live reflection. Restart after changing a class
default. Use a cooked Blueprint in a PAK when a new class is required.

## Working examples

- [Fixed Menu: character-creation labels](../examples/FixedMenu/blueprints/character_creation_text.jsonc)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
