---
channel: experimental
promote_to: HELPY-WIKI.md
---

!!! warning "Experimental · 0.9.0.7"
    Requires the matching RuneSchema core, Helpy and `RuneSchema.Wiki` plugin from
    the same build. In-game acceptance of the overlay page is pending.

# Dragonwilds Wiki in Helpy

The `RuneSchema.Wiki` plugin adds a **Wiki** tile and sidebar entry to Helpy. It
shows the [RuneScape: Dragonwilds Wiki](https://dragonwilds.runescape.wiki/) inside
Helpy's window and its game overlay, so you can look things up without leaving the
game.

## Open a page

- In Helpy, choose **Wiki**, type a name in **Search the wiki** and press Enter.
  An exact page title opens that page; anything else shows the wiki's search results.
- In the game console, type `rs wiki adventurer's tunic`. Helpy opens on the Wiki
  tab at that page and the console closes. `rs wiki` alone opens the wiki's main page.

The page has **Back**, **Forward**, **Home**, **Reload** and **Open in browser**.
Links to other websites open in your own browser, and only when you click them.

## Where the page shows

| Helpy mode | Wiki page |
| --- | --- |
| Separate window | Yes |
| Game overlay (Helpy's own window over the game) | Yes |
| Native overlay (drawn by the game's renderer) | No: choose **Open in your browser**, or switch overlay mode in Settings |

The page needs the Microsoft Edge WebView2 Runtime, which Windows 10 and 11 normally
include. Helpy keeps the page's cookies and cache in its own state folder, separate
from your browser.

## Turn it off

Disable `RuneSchema.Wiki` in `RuneSchema/settings/plugins.txt`
(`RuneSchema.Wiki : 0`). The tile, sidebar entry and console command go away; `rs wiki`
then reports that the plugin is not enabled.

## Attribution

Wiki text is from the RuneScape: Dragonwilds Wiki under
[CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/). Helpy shows the
live site; it does not copy pages.

## How it works

`RuneSchema.Wiki` is a `Host: "helpy"` plugin with two halves:

- `RuneSchema.Wiki.dll` in Helpy registers the tab and its tile icon, and draws the
  page through the host's `WebPage` callback, confined to `https://dragonwilds.runescape.wiki`.
- `RuneSchema.Wiki.Game.dll` in the game registers the `RuneSchema.Wiki.lookup`
  service. `rs wiki` calls it; it sends an owned `Lookup` plugin event that opens
  the plugin's own route. Nothing is sent unless the reply buffer already fits, so a
  sizing call never opens anything.

Other plugins can use the same host features: see
[Grand Exchange integration](GRAND-EXCHANGE.md#navigation-api).
