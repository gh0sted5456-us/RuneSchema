# RuneSchema 0.8.1.0 controller binary update

These matching files update an **existing complete 0.8.1.0 candidate installation**. This folder is not the full RuneSchema runtime package and is not an upgrade package for stable 0.8.0.3. The maintainer is assembling the complete runtime ZIP separately.

Close Dragonwilds/the dedicated server normally and choose **Exit Helpy** before copying the contents of `RuneSchema/` into the matching installed RuneSchema folder. Keep the existing plugin manifests, PAKs, Lua helpers, assets and defaults from the complete candidate. Preserve your mods, settings and AppData data. Helpy.exe and SDL3.dll stay in the runtime's `plugins/RuneSchema.Helpy/dll/` folder.

Included: compressed core DLL, matching Helpy application and bridge, compressed Vulkan adapter, pinned SDL3 runtime and consolidated license notice. The unchanged core is included to keep the binary set explicit. `SHA256SUMS.txt` and `verification.json` identify the exact files.

In **Settings → Helpy → Startup**, choose **Xbox / PS / Home button**, **View + Menu**, or **Disabled**. The keyboard shortcut remains available. **Reserve Guide button in game** requests Windows GameInput's exclusive foreground Guide policy. Steam can read controllers independently, so its Guide-button configuration may still need adjustment. Helpy does not change Steam/Game Bar settings.

Validation: 149 Helpy tests, seven diagnostic-reference tests, four documentation-channel tests, the strict combined documentation build and the packaging-license smoke test passed. All four virtual controller families passed the shortcut/navigation checks; the controller input regression passed 20 consecutive repetitions. Compressed DLLs passed UPX integrity and decompressed code/export/resource/import/unwind/relocation checks. Physical USB/Bluetooth Guide behavior and Steam exclusivity still need real-controller testing.

See the [controller guide](../docs/experimental/CONTROLLER-NAVIGATION.md) and [candidate status](../docs/experimental/PRE-RELEASE.md). Publishing this folder does not replace a stable release download or promote the candidate to main.