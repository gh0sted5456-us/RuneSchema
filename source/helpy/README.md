# RSMK 0.8.0.1 integration

Home > Quick settings exposes **Open Helpy at startup** directly, outside
Advanced. Turning it off keeps the desktop closed until F2 on the next launch;
it does not disable RuneSchema. The existing per-installation preference and
conflict-safe settings writer are unchanged.

Diagnostics uses registered code areas before text hints. New configuration,
plugin-startup and world-cleanup codes share the generated runtime/Helpy
catalogue and family-grouped website reference. Adding or reordering codes no
longer shifts the suggested references for older uncoded messages.

## Grouped navigation candidate

- **Players** contains Items, Clone, Recipes, and the optional Character Editor.
- **World** contains Map, Spawns, and the optional Journal. Party is a dark,
  bounded row list below the map: identity/vitals on the left, equipped icons
  on the right. Fresh native durability appears as an icon meter and exact
  hover values; unknown readings display `?`. Narrow rows wrap rather than
  shrinking icons. Detach Party opens its own resizable window;
  Dock below map returns it without resizing or covering the map.
- The sidebar remembers each group's last workflow. Home shows group tiles,
  not duplicate tiles for every nested tool. Optional tabs remain controlled
  by Settings > Workspaces; hiding a tab does not discard its draft.
- Existing carts, authoring drafts, inventory views, filters, native permission
  checks, and explicit delivery/export confirmations remain independent of
  navigation. Detached Map, Recipes, Item Bay and inventory surfaces still work.

The matching DLL is included in the October 23, 2026 experimental 0.8.0.1
package refresh. See the repository's release notes for the exact archive and
test scope; publication is not live gameplay/performance certification.

## Runtime and content integration

RuneSchema and RSMK now share version `0.8.0.1`, including their manifests and
Windows DLL metadata. Info (next to Credits and Legal) contains framework,
game and UE4SS versions; the centered footer keeps only the runtime badge,
readiness dot and F2 hint. Steam/Epic replaces the old Steam/GOG label.
Specific Steam or Epic branding requires launcher evidence; an ambiguous
Win64 install uses the shared badge. Epic branding is renderer-tested, not
a claim of live Epic gameplay or native-hook certification.

Item Bay > Icon cache (also in Settings) provides Import & map icon pack,
Pull game icons / update differences, Build & export game icons and Export
cached icons. Portable .rsmk-icons packs contain preview
pixels with exact asset paths, source identities and source/mod grouping.
Imports merge into AppData without manual assignment. Matching unchanged
pixels and mapping sidecars are not rewritten; only changed images invalidate
their own resident UI textures. Visible disk hits bypass engine captures.
Difference scans reuse valid cached files in bounded batches before requiring
a menu renderer, then capture missing entries. Discovery still validates the
registered content; packs do not install mods or full-resolution game textures.

The complete item-name/path manifest is saved after the initial alphabetical
discovery, separately from the pixels. Later launches validate live mounted
source fingerprints and runtime item paths before restoring that manifest.
Matching manifests serve search and A-Z browsing without another UObject
census; changed sources, different system locales and changed runtime path
lists rebuild it. Disk parsing, sorting and serialization run on a file worker.
This is display metadata only: grants, spawning and authoring still validate
native assets. An icon pack can eliminate picture capture, but the first
uncached item discovery and initial source verification still take time.

Live Log has independent RuneSchema and UE4SS file tabs, each retaining its
own search, severity/source filter, pause/follow controls and scroll position.
Live Log colors (also under Settings > Appearance) saves up to 32 literal,
case-insensitive keyword/source rules in ui.jsonc. Keywords override source
colors; first matching rule of each kind wins. Unmatched entries retain theme
severity colors. Exports protect both live log files and their rotation backups.

Item Service and the shared/detached Item Bay include All, Vanilla, Expansion,
Modded and Unknown source filters applied before paging. Mod items carry the
accent-aware RuneSchema logo and source-name tag; borrowed icon roots do not
establish item ownership. Native plugin sources come from installed content
descriptors/container paths, not item names. Source labels can update in place
without clearing an unchanged browse.

Right-click View family browses matching equipment-set names while retaining
dye/trim/material qualifiers. Clear family restores ordinary browsing. This is
a name-derived browse grouping, not a native equipment-set gameplay contract.

Recent query windows use bounded least-recently-used retention, including empty
completed results. The native display-name cache retains up to 65,536 weak-
validated entries instead of clearing wholesale above 16,384. World/source
invalidation and stale-query fences remain in effect.

Dedicated desktop rendering remains available without the game's RHI. Source-
scoped imported icon packs can provide display-only client thumbnails for active
server sources, without changing live item definitions or server permissions.
Text/details remain usable when textures or their references are absent; see
`docs/servers/helpy-desktop.md` for import and runtime-validation limits.
Read-only catalogue windows no longer wait for the spawn loader's ready-world
callback. Grid/list/bay/details/cart labels share native-name-first display
fallbacks, so an explicitly empty Name does not hide a list row.

Item Service and the shared/detached Item Bay now browse A-Z by displayed name,
case-insensitively with canonical-path tie breaking. Native query results are
ordered before paging, not reshuffled as new pages arrive. The first query
builds its ordered index in bounded metadata-only slices; a bounded LRU retains
completed indexes for repeated searches. World/source/authored-item changes
invalidate the native index. No synchronous asset loads are used for sorting.

Cart recipients have individual X removal controls and Clear recipients.
Neither action changes staged items or quantities. World changes discard old
player selections, rosters and delivery receipts; disconnected targets are
pruned by path. Old-generation pushes cannot restore phantom players. An
interrupted delivery still requires inventory review before retrying, because
an unconfirmed grant must not be automatically replayed in another world.

The standalone API-1 desktop plugin lives here. `source/raw` owns the Unreal
game-thread bridge; this UI does not import UE4SS or call Unreal directly.
Build the normal RuneSchema targets to produce a matching core/plugin pair.
The authoritative plugin manifest, branding and default skin are in
`source/plugins/RuneSchema.Helpy`. Preserve users' existing settings on install.

Windows/MSVC standalone validation:

```powershell
cmake -S source/helpy -B build/helpy-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/helpy-tests
ctest --test-dir build/helpy-tests --output-on-failure
```

Tests include real Direct3D renderer fixtures with synthetic data, not a live
game. `source-cache.exe <RuneSchema-folder>` performs a read-only fingerprint
inspection and prints its key, file count and duration. Both copies of the
fingerprint implementation are parity-checked and independently tested.

Source identity uses game-relative paths, extended-length Windows paths and a
Win64/WinGDK lane. Authored content changes and removals still invalidate the
cache; installation aliases do not. Browse cancellation remains monotonic
across initial source inspection and world generation changes. Icon capture is
visible-demand only, with one outstanding capture, bounded queues/retries and
explicit reload recovery.

The 2026-10-14 native-worker crash remains under investigation. These fixes do
not establish its cause or demonstrate a gameplay crash fix. The combined
candidate also catches up already-shown world cells for configured spawns;
native save depletion, transient ownership and cleanup guards remain intact.
NPCs use the same visible-cell recovery boundary. Late class readiness continues
at a bounded cadence after the initial scan burst; actor construction remains
one-shot. Quest-linked NPCs default to native quest-NPC map and overhead art,
while explicit disabled/custom marker settings win. These location markers do
not fabricate a native quest availability or completion state.
Hover details are source-item-owned: leaving the item dismisses them immediately,
and lengthy details can be wheel-scrolled while hovering the source item.
Live icon display, silver-seam placement, teleport, save/reload and multiplayer
behavior require separate in-game validation.

Original UI handoff: Helpy 2.0.4 from the RuneSchema UX Adaptation project.
The imported UI follows its component licenses under `licenses` and `vendor`.

Main RuneSchema settings now prefer INI with a guarded two-launch JSONC
migration; plugin-owned settings remain JSONC. Source identity includes both
main settings formats. Plugin shutdown drains its filesystem cache worker
after stopping the UI thread and before DLL unload. Dedicated hosts may start
the separate desktop window in an interactive Windows session; non-interactive
services skip the GUI without stopping the server. See
`docs/SETTINGS-INI.md` and `docs/servers/helpy-desktop.md`.
