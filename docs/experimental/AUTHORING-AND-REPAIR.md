---
channel: experimental
promote_to: ADVANCED-AUTHORING-TOOLS.md
---

# Authoring, cache creation and repair

!!! info "Experimental Â· 0.8.1.0"
    These workflows use the experimental Helpy application. See the
    [experimental overview](index.md) and [stable downloads](../CURRENT-RELEASE.md).

## Update an installed mod's cache

1. Open **Mods**, right-click the installed mod and choose **Create/Update Cache**.
2. Helpy opens **Tools â†’ Utilities â†’ Authoring tools**, focused on that mod in
   the **Installed** tab. Confirm the folder and source name.
3. Close the assigned Dragonwilds game or dedicated server before starting the
   offline build. Other unrelated private installs are not the selected source.
4. Choose **Start cache build**. The console reports extraction, discovery,
   icon processing and publication. **Pause**, **Resume** and **Stop** control
   the worker; pausing or stopping is separate from closing the panel.
5. A successful build publishes the mod's **cache/** folder. The previous output
   is SHA-256 verified in the assigned platform's AppData cache-builder backups
   before replacement; old cache folders are not retained beside the mod.
   A failed build retains the current cache. Read the report for partial discovery or
   unavailable icons before distributing it.

**Back to Mods** returns to the page that opened the cache builder, preserving
its draft and search. Tools uses the same category-and-content panes as Settings;
the cache input and progress panes stack when the content area is narrow.

For several mods, use **Mods → Installed mods → Cache enabled mods**. Enabled
installed mods with cooked PAK/LogicMods inputs are queued in the displayed
order. One worker runs at a time, and each completed bundle is offered to Helpy
before the next job. JSON-only, disabled and missing entries are not built.
The progress pane reports completed, failed and skipped jobs. Pause applies to
the active worker and the queue; Stop cancels the remaining queue at safe
checkpoints. Switching pages does not cancel a build. Keep Helpy open for queue
advancement; an active worker can finish while hidden, but the next job waits
until the application is visible again. Keep the assigned game closed for offline authoring.

The advanced authoring preference can detect and install the required .NET
10 SDK when missing. It is an explicit tool job, not a background
installation check. The [offline cache guide](modders/offline-helpy-cache.md)
explains the standalone BAT route and dependencies. The current experimental
[authoring tools ZIP](https://github.com/gh0sted5456-us/RuneSchema/raw/refs/heads/experimental/resources/tools/authoring-tools/b357a6538135e98184702bbf2ef663aaf4171c69b6a980cc851c78cc8c1769d6.zip)
is also available directly from this branch.

The local follow-up reads cooked culture-invariant display text as well as
localized/source text. An unresolved localization key is not treated as a
display name. Rebuild an older mod cache once to replace names written by the
older exporter. The Dye validation produced 2,241 records and 2,176 icons with
zero `ITEM_` display-name fallbacks, including **Ranger Tunic (Dyed Black)**.
There were 15 reported skips; the cache remains a partial presentation index.
The builder uses the pinned provider's localization API before export
([CUE4Parse source](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/FileProvider/AbstractFileProvider.cs)).

## Create or convert a package

1. In the same tool, choose **Creation** and drop a RuneSchema ZIP/7z, loader
   folders, a UE4SS mod folder or cooked PAK inputs onto the input pane.
2. Fill in the mod metadata for **id.txt**. Keep a stable mod identity; the
   package name is not a replacement for its source fingerprint.
3. Review the proposed structure. Native UE4SS DLL mods stay at UE4SS level;
   a warning explains the limits of nesting them under RuneSchema.
4. Start the build and review the console/report. Successful output includes
   the cache, id.txt and the consolidated RuneSchema layout, zipped into Downloads.

Packages use **Binaries/** and **Content/** roots. The Content/Paks/~mods
marker helps installers preserve the intended path. A packaged cache supports
fast presentation; it does not prove that every gameplay contract is registered.

## Repair RuneSchema base files

Open **Tools â†’ Utilities â†’ Repair**, with the assigned game/server and other
Helpy instances for that installation closed, then choose **Repair**.

The console first shows available log context, then checks the required base
files against a verified GitHub release matching the installed version. It
replaces missing or changed base files in full and retains backups and a receipt.
User mods, settings, saves and caches are outside its replacement scope.

If a matching release package has not been published, repair reports that and
leaves the installation alone. It does not substitute the latest stable build
for an experimental version. See [repair details](HELPY-STANDALONE.md#repair-runeschema).
