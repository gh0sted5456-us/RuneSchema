# World effects and building identity integration — 2026-10-29

Base: `1d3338969909cae3a009b244ae17cd147c369330` (0.8.0.3). This work does not move that release tag or install a production DLL.

## Incoming source

TOTEMS supplied the generic BuildingPolicy changes in `Totems-1.1.0-RecoveredSource.zip`, SHA-256 `c6f5bc520dca31236daa2f0760dbed7cd6833f9e99048df67897666d4d418bfa`. The two modified headers matched their base Git blobs exactly before applying the tracked diff. The four new headers were imported separately. Mod assets, definitions, private native fixture drivers and installed DLLs were not imported into the framework package.

The handoff's 229 dedicated-server checks apply to its original DLL, hash `df0edc8998ea0941d01721e4be00aa48b0e98cbf42f2cbd28ffa83ee548927e3`. They were not rerun against this integrated candidate. Its receipts explicitly exclude rendering and Game Pass gameplay.

Integration adds FName-first event filtering, one-time actor discovery, cached building managers, inactive-policy work gates, scalar/quantity tests, provider-pair validation, executable-range checks and bounded instruction decoding. Gameplay definitions remain cooked in the mod.

## Placement mismatch

Siege's observed old dedicated registry contained 847 native + 42 added buildings; its client contained 847 + 60. The missing 18 definitions were the new TOTEMS variants. The reconstructed client trebuchet ID 874 resolved to an old-server No Build Totem. The reconstruction uses the client's complete log after relaunch, not the failed network packet itself.

The existing startup native-ID synchronization does not cover this later world-building table. The new guard exchanges bounded ordered-identity fingerprints on the owned compatibility channel and validates placement independently of connection entry. It permits a certified common native prefix, blocks an unverified/different modded index, clears peer evidence on world reset, bounds peer/diagnostic state and does not rewrite either registry. Late joins use their own owned handshake.

## Validation boundaries

`building-network-guard` covers the 847/889/907 divergence, equal-size reordered lists, native-prefix allowance, unavailable IDs, missing evidence and malformed telemetry. `world-effect-rules` covers strongest scalar selection, native quantity scaling, rounding, overflow and invalid values. The core DLL must also compile with the existing SDK; generated diagnostic headers and website anchors must agree.

Before deployment, verify the native `BuildModeComponent:Server_SpawnBuilding` parameter/owner contract, first and late client joins, mismatched-list rejection, matched Siege placement, world re-entry and native-prefix placement on an isolated server/client pair. Re-run the private world-effect probes against this exact candidate separately. No live user server restart, production installation or release publication is part of this handoff.

Local checks completed: core DLL build, Helpy DLL build, all 250 core tests, 112 Helpy tests excluding the interactive server-desktop test, seven diagnostic tests, diagnostic emission/catalogue audit, and strict website build/link audit across 111 pages. A focused six-test building/registry subset also passed after integration review. Optional adapter binding errors retain the provider's other policies; farm callbacks recheck authority after host-to-client world transitions. These checks do not replace the live validation above or measure gameplay FPS/network latency.
