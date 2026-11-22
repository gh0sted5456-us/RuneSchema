---
channel: experimental
promote_to: modders/PLUGIN-SERVER-REQUESTS.md
---

!!! warning "In development · 0.9.0.7"
    Requires matching RuneSchema core on the player's game and on the server. Live
    multiplayer acceptance is pending. Grand Exchange transfers stay disabled until their own
    tests pass.

# Plugin server requests and item grants

A plugin's in-game half can ask the server it is connected to, or its own game when it hosts
or plays alone, to run the **same plugin's server half**. That server half can then put items
into the sending player's inventory or the plugin's own storage on that character, or into a
world chest, or remove an exact stack, through RuneSchema's capability-gated services.

This is how Grand Exchange delivers on any server that has Grand Exchange installed:

Grand Exchange keeps purchases in the app's withdrawals box. The player moves them to app storage
or to the game Vault, and presses **Sync** to move things between the app and the game; nothing
moves on its own. Sync, on a server:

1. The Grand Exchange window tells GE's in-game half what to move.
2. GE's in-game half calls `runeschema.server.request`.
3. RuneSchema sends it to the server on its owning-player bridge, the same channel as Helpy's
   own server requests, and attaches the sending player's character.
4. The server's GE half checks the withdrawal, then calls `runeschema.item.grant` to put the
   items into that character's Grand Exchange storage, a component GE owns on the player's
   controller. The Vault prop in the world only marks where players open it; it holds nothing.
5. The answer returns to GE's in-game half, which tells the Grand Exchange window.

The other direction, a deposit from the game Vault into the app, is the same request: GE's
server half captures the deposited stack with `runeschema.item.snapshot` and removes it with
`runeschema.item.destroy`, both with `"Scope": "Owned"` and the storage's identity. See
[Exact item destroy](ITEM-DESTROY.md).

If the server doesn't have the plugin, the answer says so (`Reason: "NotInstalled"`), and GE tells
the player to use the Vault in single player or from the main menu.

RuneSchema never grants, removes or changes anything because a player asked. Only the
plugin's server half decides, so a server without that plugin refuses. These requests don't
use Helpy's PIN, allowlist or "allow client item grants" settings, because Helpy access isn't
what authorizes them. Grand Exchange's server half accepts only a withdrawal the Bazaar
signed for that character, once, before it expires.

## Declare the capabilities

Add these to the plugin's manifest. For a `Host: "helpy"` plugin, use `GameCapabilities`,
because they belong to its in-game half:

| Capability | Lets the in-game half call |
| --- | --- |
| `server.request` | `runeschema.server.request` |
| `item.grant` | `runeschema.item.grant` |
| `item.destroy` | `runeschema.item.destroy` (see [Exact item destroy](ITEM-DESTROY.md)) |

The server needs the plugin installed and enabled, with `ServerCompatible: true` so a
dedicated server loads its in-game half.

## Send a request: `runeschema.server.request`

Call it on the game thread with a reply buffer of at least 4096 bytes. A smaller buffer
returns `RS_PLUGIN_BUFFER_TOO_SMALL` and sends nothing.

```json
{"Action": "VaultWithdraw", "Request": "1f0c6a52-7d2e-4b8a-9c41-0e6f3a2b9d11", "Body": {"Ticket": "..."}}
```

- `Action` is a token of letters, digits, `.`, `_` and `-` (up to 64).
- `Request` is a lower-case UUID that you keep in your own journal.
- `Body` is a JSON object of up to 3000 bytes.

The reply depends on where the game is authoritative:

- **Host or single player:** `{"Local": true, "Result": {...}}`. The server half already ran.
- **Client of a server:** `{"Local": false, "Revision": n}`. The answer arrives later at your
  service **`<YourId>.server-result`**, called on the game thread with a null reply buffer.

`RS_PLUGIN_NOT_FOUND` means there is no local player or bridge yet. `RS_PLUGIN_FAILED` means 64
requests are already waiting for answers.

Both answer shapes have the same form:

```json
{"Action": "VaultWithdraw", "Request": "1f0c6a52-...", "Ok": true, "Result": {...}}
{"Action": "VaultWithdraw", "Request": "1f0c6a52-...", "Ok": false, "Error": "GrandExchange is not installed on this server."}
```

A failed answer also has `Reason`:

| Reason | Meaning | What to do |
| --- | --- | --- |
| `NotInstalled` | The server has no `<YourId>.server`. Nothing ran. | Tell the player this server can't do it. |
| `NotReady` | The character or the item-ID sync isn't ready yet. Nothing ran. | Retry in a moment. |
| `Invalid` | RuneSchema refused the request itself. Nothing ran. | Fix the request. |
| `Refused` | Your server half ran and failed, or its reply couldn't be delivered. It may have acted. | Ask your server half about the `Request` before trying again. |

`RS_PLUGIN_NOT_FOUND` from the call itself means there's no RuneSchema bridge to a server, for
example on a server without RuneSchema. If no answer arrives, keep your own timer and treat the
request as unknown: a server half may have acted even though the answer was lost.

An answer is delivered only if this game sent that request to that plugin within the last two
minutes. A lost answer is never retried for you. Ask your server half about the `Request` if
you need to know what happened.

## Serve requests: `<YourId>.server`

Register a service named `<YourId>.server` in your in-game half. Only plugin `<YourId>` can
register `<YourId>.server` or `<YourId>.server-result`. RuneSchema calls it on the game thread
of the authoritative game:

```json
{"Plugin": "GrandExchange", "Action": "VaultWithdraw", "Request": "1f0c6a52-...", "Body": {...},
 "From": {"Character": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071", "Remote": true}}
```

`From` comes from RuneSchema: it's the character of the player whose bridge sent the request.
A `Body` field can't change it. Reply with one JSON object of up to 2700 bytes.

The sending game also reports, alongside the request, whether its own Helpy delivery flags
allow deliveries to that character, with your plugin's own pause left out. A server can't read
a remote player's flags, so `item.grant` uses this report for a grant while serving the
request (see below). It isn't passed to your server half.

A remote request also needs the player's connection to have finished item-registry
synchronization, just like Helpy's own server deliveries.

## Grant items: `runeschema.item.grant`

```json
{"Request": "6b1e...uuid", "Item": "/Game/Items/Ore/Copper.Copper", "Count": 5,
 "Target": {"Container": {"Index": 81234, "Serial": 17}}}
```

```json
{"Request": "6b1e...uuid", "Item": "/Game/Items/Ore/Copper.Copper", "Count": 5,
 "Target": {"Character": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071", "Scope": "Inventory"}}
```

```json
{"Request": "6b1e...uuid", "Item": "/Game/Items/Ore/Copper.Copper", "Count": 5,
 "Target": {"Character": "5A3F0C21B6D84E7F9A1C2B3D4E5F6071", "Container": {"Index": 90411, "Serial": 3}}}
```

- **Container target:** a world inventory component, such as a chest's, named by the
  `ObjectIdentity` your plugin read for it. Anything a player owns (on their controller,
  player state or character) is refused here.
- **Character target:** a connected character's `Inventory` or `Personal` container, or with
  `Container` instead of `Scope`, one exact inventory component that character's controller
  owns, such as your plugin's own storage. RuneSchema checks it is live, in that character's
  world, owned by exactly that controller and not the equipment loadout, and checks again on
  every count and just before the grant.

### Who a Character target can be

A grant to a character is a delivery. Which deliveries are allowed depends on whether your
server half is serving that character's own request:

| Situation | Allowed target | Pauses that apply |
| --- | --- | --- |
| Inside your `<YourId>.server` call, granting to `From.Character` | That character only | The player's switch and every other mod's pause. **Your plugin's own pause is skipped.** |
| Inside your `<YourId>.server` call, any other character | Refused | |
| Outside a request, a character local to this game (single player, the host's own) | That character | All, including your own |
| Outside a request, another player's character | Refused | |

- **Your own pause** is the Helpy flags mod whose id is your plugin id lower-cased (plugin
  `GrandExchange`, flags mod `grandexchange`). Plugins whose id has other characters, such as a
  `.`, have none to skip.
- **Where it is checked.** On the authoritative game against its own flags folder (world
  pauses, and every pause on the host's PC), with your pause skipped when serving. For a remote
  player, also against the report their game sent with the request: if their switch is off or
  another mod paused them on their PC, the grant is refused with that reason.
- **Why your own pause is skipped.** A mod pauses ordinary Helpy deliveries to the characters
  it manages. A delivery its own server half makes, after its own checks, is the one delivery
  that pause isn't meant to stop. Grand Exchange makes one only for a withdrawal the Bazaar
  signed for that character.
- A refusal is cached for its `Request` UUID like any result. Use a new UUID to try again
  after the player turns deliveries back on.
- Treat a player's own installed plugins as theirs: a plugin id isn't signed, and the pause
  isn't a defence against code the player installed. What a server trusts is that server's
  own plugins.
- `Count` is 1 to 10000 of one item definition. The game adds new items at full durability
  with `InventoryComponent:AddItemByData`, the call Helpy's own grants use.
- It runs only on the game thread where the game is authoritative.
- Give a reply buffer of at least 4096 bytes. A smaller one returns
  `RS_PLUGIN_BUFFER_TOO_SMALL` and does nothing.

The result has `State` and `Ok`:

- **`Confirmed`:** the target's count of that item grew by exactly `Count`.
- **`Refused`:** nothing was invoked. `Phase` and `Error` say why, for example that the target
  can't accept that quantity.
- **`Unknown`:** the game was called but the count didn't match, or the call failed. Don't
  retry it as a new request.

The first result for a `Request` UUID is replayed for the rest of the session, and the same
UUID with a different body is refused. Like `item.destroy`, this cache is in memory only:
keep your own durable journal and reconcile after a restart.

`Confirmed` is a live count, not proof the game saved it. Don't treat it, or an answer
reaching the player, as a save receipt.
