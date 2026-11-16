---
channel: experimental
promote_to: MOD-CONSOLE.md
---
# Cooked mod console

**Experimental — implementation candidate; native acceptance pending.** This is
separate from the installed build. Do not treat compilation as live verification.

An enabled mod can expose a local host command on its existing gameplay
GameState component. RuneSchema routes the command; the mod owns all gameplay,
preset validation, authority checks and its reply. No component is created by a
console request.

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "WorldComponents": ["/Game/Mods/Example/BP_Host.BP_Host_C"],
    "ConsoleCommands": [
      {"Version": 1, "Id": "Control", "Class": "/Game/Mods/Example/BP_Host.BP_Host_C"}
    ]
  }
}
```

`Class` must be a concrete ActorComponent owned by the enabled mod. IDs are
unique within the owner, case-insensitively, with at most 96 ASCII letters,
digits, dots, underscores or hyphens. `.` and `..` are invalid. Change declarations
before restarting the game. Conflicting declarations reject dispatch.

Create an impure Blueprint function named `RuneSchemaConsoleCommand` on that
exact class, with no return-value pin, RPC, Exec or native function flags:

| Pin | Direction | Type |
| --- | --- | --- |
| PlayerController | Input | Engine PlayerController object |
| Arguments | Input | String array |
| Success | Output | Boolean |
| Reply | Output | String |

Input arrays may use the compiler's const reference flags. Mutable in/out pins
are rejected. Outputs must be ordinary nonreference, nonconst output parameters.
Set `Success` to false and an explanatory `Reply` before validating arguments.
Replies are limited to 1,024 UTF-8 bytes.

Run `rs mod Example:Control argument`, or use the `runeschema` alias. Arguments
are whitespace-separated: at most eight, each at most 64 UTF-8 bytes. Quotes,
escapes, NUL and control characters are unsupported. Total input is bounded to
1,023 UTF-16 code units. There is no arbitrary class or function-name dispatch.

Version 1 requires a local authoritative gameplay host or standalone session.
Remote clients are rejected before the mod handler, with no forwarding or PIN
bypass. This does not expose dedicated-server RCON. The active GameState must
contain exactly one instance of the declared class, with matching owner/world.
`RS-CMD-001` reports unavailable declarations, authority or reflected contracts.

The dispatcher shares the existing local console hook; it adds no polling loop.
Multiplayer forwarding and live registration changes are outside this contract.
