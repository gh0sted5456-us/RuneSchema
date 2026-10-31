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
   is retained if the build fails. Read the report for partial discovery or
   unavailable icons before distributing it.

The advanced authoring preference can detect and install the required .NET
10 SDK when missing. It is an explicit tool job, not a background
installation check. The [offline cache guide](modders/offline-helpy-cache.md)
explains the standalone BAT route and dependencies. The current experimental
[authoring tools ZIP](https://github.com/gh0sted5456-us/RuneSchema/raw/refs/heads/experimental/resources/tools/authoring-tools/b357a6538135e98184702bbf2ef663aaf4171c69b6a980cc851c78cc8c1769d6.zip)
is also available directly from this branch.

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
