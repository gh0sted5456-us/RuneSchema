---
channel: experimental
promote_to: SERVER-MOD-LIST.md
---

!!! info "Experimental · local candidate"
    This feature is being tested locally. It is not part of an uploaded package.
    See [experimental features](index.md) and [stable downloads](../CURRENT-RELEASE.md).

# View a world's RuneSchema mods

With the networking plugin enabled on both host and client, the host sends its
enabled RuneSchema mod names, IDs, versions and optional download-page links
after the player connects.
Open **Helpy → Mods → Server comparison** in the left category pane to browse
or search the complete list. **RuneSchema mods** manages your own installation;
**UE4SS mods** is a separate read-only inventory.
A notice with the RuneSchema logo says **Entering a RuneSchema world** after a
validated list arrives. Its border and accent show **green / Versions match**
when the compared versioned entries align, **amber / Review versions** for
different, missing, disabled or ambiguous versioned entries, and the normal
information color while checking or when no versioned entries can be compared.
Click it to open that tab. It uses the existing
diagnostic notification setting; receiving the list does not open Helpy automatically.

The report shows **Host** and **Client** versions side by side. **Client only**
lists installed mods the host did not advertise. **Copy comparison report**
copies the complete comparison as plain text, independent of the search filter.
Unversioned host/shared entries are annotated and excluded from the alignment
totals and notice color. A present client mod with no version is also excluded.
A versioned host mod missing on the client is reported as missing. Client-only
entries never count against host alignment; unversioned ones are annotated too.
An all-unversioned list stays neutral instead of claiming a match.

The installed-version badge compares the host's author-supplied version with
the connected installation's ID.txt/mod.json metadata. **Match** is green;
**Different**, **Missing**, **Disabled**, **Unknown** and multiple local copies
remain distinct. Empty versions never count as a match. Author ModID lets
renamed folders match; older hosts fall back to their advertised folder ID.
This is a version-label comparison, not a registry/content compatibility test.
Local metadata is read once when needed and on **Refresh installed**, rather
than scanning mod folders for every frame. The connection notice requests one
asynchronous metadata read when needed; the comparison is rebuilt only when
the host list, runtime generation or installed snapshot changes. It compares
installed metadata, including saved next-launch settings, not unsaved order drafts.

The native **WorldModdedWarning** can include the complete list in its existing
scrollable body when that warning is still open and its supported fields are
available. A layout without that scroll container receives a short preview.
RuneSchema preserves
the game's original warning and button actions. If the warning has already
closed, the complete list stays available in Helpy; receiving it does not force
Helpy open. The pre-connection warning cannot show data that the host has not
sent yet. Clients without RuneSchema keep the game's normal warning.

## What the list means

- It follows the host's enabled order for the current process, including mods
  that use only JSON or scripts. Disabled mods are omitted. Changes scheduled
  for the next launch do not change the advertised active list.
- Names and versions come from the mod's metadata, with the folder identifier
  as the name when metadata is missing or unreadable. An enabled entry does not
  certify that every loader in that mod succeeded.
- The list is informational. It does not install mods, grant server access,
  remap IDs or bypass registry checks. Versions are displayed as supplied.
- Only public mod identity fields are sent. The list contains no local paths,
  saves, account details, server PIN or mod files.

## Link to a mod's download page

Add a `Website` line to the mod's existing **ID.txt**, pointing to its public
download page. For example, RuneSchema's own entry can use:

```text
ModID: RuneSchema
Name: RuneSchema
Website: https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=files
```

The host reads this during mod discovery. **Helpy → Mods → Server comparison** shows
a **Download** link beside that entry. Clicking it opens the supplied page in
the player's browser; downloading and installing remain the player's choice.
Installed mods also retain **right-click → Open mod page**. The existing
`mod.json` website override still applies when present.

Links are optional. Mods without one remain listed. Only bounded HTTP/HTTPS
links with a nonempty host are accepted; local-file and command links cannot
be opened through these actions. The plain native warning lists names and
versions and directs players to Helpy for the clickable links.

## Connection and limits

The bridge uses its existing reliable owning-player RPCs and bounded data relay.
It sends one authority snapshot to each joining player and keeps it for late
joiners. This adds no folder scan or periodic mod-list poll during play.
The supported list contains up to 1,024 mods and fits within a 1 MiB transfer;
each wire packet remains below the existing 4 KiB action limit. Larger lists
arrive in several packets, so the full view appears after assembly and validation.
Leaving a world clears the old list. A client cannot publish the authority list.

If no list appears, check that both sides have the candidate core and networking
plugin enabled, then reconnect normally. Older hosts do not advertise this
capability. **No list received** does not mean the host has no mods.

## Diagnostics

- [RS-NET-004](../ERROR-CODES.md#rs-net-004): a validated server list arrived.
- [RS-NET-005](../ERROR-CODES.md#rs-net-005): the enabled list exceeded its
  bounds or could not be prepared. Mod loading continues; investigate the
  accompanying detail rather than treating this as a registry rejection.
