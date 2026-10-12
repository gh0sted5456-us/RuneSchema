# RuneSchema for server owners

A dedicated Dragonwilds server owns gameplay outcomes. Players still need the
matching cooked content and presentation files for a mod they use or see.
RuneSchema on the server reads its own enabled mod list and bridges; a client
does not download a missing PAK from the server automatically.

## Set up in this order

1. [Install RuneSchema on the dedicated server](dedicated-setup.md).
2. Put the required gameplay mods, PAKs, and bridges on the server.
3. [Align the client and server mod sets](mod-parity.md).
4. Back up the world and character data before changing the mod list.
5. Start the server, then read its `UE4SS.log` before inviting players.
6. Test one client joining, using content, leaving, and joining again.

The same Windows dedicated-server save format uses the server's
`RSDragonwilds/Saved/SaveGames` folder. Keep a copy of that folder before
uninstalling a mod that added world buildings or other persistent content.
[World-save maintenance](save-maintenance.md) explains the cleanup boundary.

## Server-specific boundaries

- The server mounts enabled cooked PAKs and performs authoritative gameplay
  registration. It does not start client-only visual `ModActor` presentation.
- A co-op host is also a client. Unlike a headless server, it may run the
  client presentation part of an enabled mod.
- RSNetworking and the shared registry bridge are included in the RuneSchema
  ZIP, but a mod must still own the complete gameplay assets and use a
  supported route. [Optional bridges](../OPTIONAL-BRIDGES.md).
- The log can diagnose missing paths and registry mismatches; it cannot make
  a client acquire content it does not have. [Bridge diagnostics](../BRIDGE-DIAGNOSTICS.md).

For store-specific character-save behavior, use the
[platform and multiplayer guide](../COMPATIBILITY.md).
