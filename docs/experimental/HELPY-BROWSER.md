---
channel: experimental
promote_to: HELPY-BROWSER.md
---

!!! warning "Experimental · 0.9.0.7"
    Requires Helpy from the same build and the Microsoft Edge WebView2 Runtime. The browser is
    off until you turn it on. In-game acceptance of the ad blocker is pending.

# Web browser in Helpy

Helpy can show a **Browser** tab beside your plugins (Grand Exchange, Wiki), so you can watch a
video or read a guide without leaving the game. It works in Helpy's window and in Helpy's
in-game overlay.

## Turning it on

Open **Settings** and find **Web browser**:

| Setting | Default | What it does |
| --- | --- | --- |
| Web browser | Off | Adds the **Browser** tab and its Home tile. Turning it off closes the page and removes the tab. |
| Block ads (uBlock Origin Lite) | Off | Downloads uBlock Origin Lite the first time you turn it on and loads it into the browser. |

Dedicated servers don't have the browser.

## Using it

The Browser tab has **Back**, **Forward**, **Home**, **Reload** and an address bar. Type a web
address (`youtube.com`, `https://…`) or some words to search DuckDuckGo, then press Enter. Links
that open a new window open in the same page instead.

In the in-game overlay, click the page to type into it; click the address bar to type there
instead. Hovering the **i** on the top bar shows the browser's credits with every plugin's notice.

What the browser doesn't do, so pages can't reach outside it:

- It opens only `http` and `https` addresses. Other links (files, apps) are ignored.
- Downloads are cancelled, and site requests for your camera, microphone, location or
  notifications are refused.
- Pop-ups open only after a click. Script dialogs don't appear.
- It never saves passwords or form entries.
- It has its own profile, separate from plugin pages, kept in Helpy's state folder under
  `web-browser`. Sites you sign in to stay signed in there.

## Ad blocking

Ad blocking uses [uBlock Origin Lite](https://github.com/uBlockOrigin/uBOL-home) by Raymond Hill
and contributors, licensed under GPL-3.0. Helpy doesn't include it and doesn't change it:

1. When you turn **Block ads** on, Helpy asks GitHub for the latest uBlock Origin Lite release
   and picks its Edge build (or its Chromium build).
2. It downloads that file from the project's own release page and checks it against the
   SHA-256 checksum GitHub publishes for it. A file that doesn't match is never used.
3. It unpacks the file into Helpy's state folder under `web-browser-extensions`, then loads it
   into the browser and reloads the page. The installed marker is updated after WebView2 accepts
   the extension. A failed replacement restores the previously installed version.

After that, turning ad blocking on uses the copy already downloaded, with no network request.
**Check for update** in Settings downloads a newer release when there is one. Turning ad blocking
off removes it from the browser; removal errors remain visible for retry. Downloaded versions
stay available for recovery and for other running Helpy instances. Installation and marker writes
are serialized across Steam and Game Pass Helpy processes.

If Settings says your WebView2 Runtime can't load the ad blocker, update the Microsoft Edge
WebView2 Runtime and restart Helpy.

## Problems

| What you see | What to do |
| --- | --- |
| "The web browser is off." | Turn on **Web browser** in Settings. |
| "Helpy could not start its web view." | Install or repair the Microsoft Edge WebView2 Runtime. |
| A download message in Settings | Check your connection and choose **Try again**. GitHub limits how often it can be asked; wait a few minutes if it says so. |
