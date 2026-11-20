---
channel: experimental
promote_to: AUTOMATION-CHANNEL.md
---

!!! info "Experimental"
    The automation channel is unreleased integration work. It is off unless you
    turn it on, and its request format follows Helpy's desktop bridge, which may
    change between releases. [Experimental overview](index.md).

# Local automation channel

The automation channel lets a program you run yourself drive the game the way
Helpy's desktop window does: item deliveries, spawns, teleports, player and
inventory reads, server tools and RuneSchema settings. Use it for your own
scripts or assistant tools on the same PC, such as an MCP server you run for
your dedicated server.

It is off by default. When it is on:

- Only programs running as the same Windows user on the same PC can connect.
  Connections from other PCs are refused.
- One program is connected at a time. Helpy's own window keeps its own
  connection.
- A request does exactly what the same request from Helpy's desktop window
  would do. Server permissions, allowlists and item-grant policies still apply
  to requests that go to a remote server.

## Turn it on

1. Create `settings/automation.json` in the RuneSchema folder of the game or
   dedicated server (`<game>\RSDragonwilds\Binaries\Win64\ue4ss\Mods\RuneSchema`).
2. Put this in it:

   ```json
   {"enabled": true}
   ```

3. Start the game or server, or restart it. The UE4SS log shows
   `Automation channel ready`.

To turn it off, set `"enabled": false` or delete the file. Then restart the game.

## Connect

The pipe is named `\\.\pipe\RuneSchema.Automation.<pid>`, where `<pid>` is
the game's or server's process ID. Check that `GetNamedPipeServerProcessId`
returns that process ID before sending anything.

Each message is a 4-byte little-endian length followed by that many bytes of
UTF-8 JSON, in both directions. Send one request and read its reply before
sending the next.

```json
{"Op": "Call", "Capacity": 1048576, "Request": {"Action": "Environment"}}
```

The reply carries `Code` (`0` is success), `Size` and, on success, `Body`:
the `runeschema.tools` result. `{"Op": "Poll"}` is answered with `{"Code": 0}`.

## Actions that change the game

Game actions go through the same single queue Helpy uses:

1. Submit the request with your own unique `_RequestId`:
   `{"Action": "Submit", "Request": {"Action": "GiveItems", "_RequestId": 9001, ...}}`.
   `Accepted: false` means another action is still running. Wait and try again.
2. Read the receipt with
   `{"Action": "Completed", "RequestId": 9001}`. `Found: false` means it has not
   finished yet. Each receipt can be read once.

Use request IDs that won't collide with Helpy's, for example values with the
top bit set.
