---
hide:
  - toc
---

# For server owners

<p class="rs-hub-lead">Set up RuneSchema for your community and keep the server and players’ mod lists in step. The server handles gameplay changes; share the matching PAKs and presentation files with players before they join, since RuneSchema does not download them automatically.</p>

<div class="rs-hub-links">
  <a href="dedicated-setup/">Set up the server<small>Install RuneSchema and enabled gameplay mods.</small></a>
  <a href="mod-parity/">Align clients<small>Know what must match and what is diagnostic.</small></a>
  <a href="save-maintenance/">Maintain world saves<small>Back up before changing persistent content.</small></a>
  <a href="../BRIDGE-DIAGNOSTICS/">Read bridge diagnostics<small>Trace a failed registration or connection.</small></a>
</div>

## A reliable first run

1. Install RuneSchema and the required mods on the server.
2. Give clients the cooked assets and presentation files their mods require.
3. Back up world and character data before changing the mod list.
4. Start the server and check `UE4SS.log`, then test one client joining, using content, leaving, and rejoining.

The dedicated-server world-save folder is `RSDragonwilds/Saved/SaveGames`. A co-op host is also a client, so it can run presentation components that a headless server does not. See the [platform and multiplayer guide](../COMPATIBILITY.md) for that distinction.
