# Offline Helpy cache builder

Optional author tooling; never run by RuneSchema or Helpy at startup.

The downloadable single-file launcher is
[`resources/tools/build-helpy-cache.sh`](../../resources/tools/build-helpy-cache.sh).
The next Helpy maintenance build also downloads an authoring ZIP from Tools.
It contains the same launcher, standalone `README.md`, license and dependency
notices. `resources/tools/authoring-tools/index.json` identifies the current
content-addressed archive; Helpy downloads it only on request and never runs it.
Use Git Bash on Windows with the .NET 10 SDK. First use restores the pinned
NuGet dependencies; later runs reuse NuGet's normal package cache. The launcher
builds in a temporary folder, outside the mod, and removes that build folder
on exit. It does not fetch/execute the latest repository source at runtime.

See the [complete walkthrough](../../docs/modders/offline-helpy-cache.md).

## Development

```powershell
dotnet restore tools/helpy-cache/HelpyCache.csproj --locked-mode
dotnet build tools/helpy-cache/HelpyCache.csproj -c Release --no-restore
dotnet tools/helpy-cache/bin/Release/net10.0/HelpyCache.dll --self-test
python tools/helpy-cache/package.py
python tools/helpy-cache/package.py --check
python tools/helpy-cache/test_package.py
```

Regenerate the script, ZIP and index after changing the C# source, project,
dependency lock or standalone instructions. Keep build output out of the mod: JSON files in a mod's root
can be inputs to its portable fingerprint.

The explicit `helpy-base-cache-probe` CMake target verifies a bundle, or performs
a real GitHub download, against an installed RuneSchema root. It writes only
to its isolated output argument and is not a default network test:

```text
helpy-base-cache-probe RuneSchema-root isolated-output bundle-or-download game-product-version
```

## Dependencies and rights

These packages are restored by .NET, not copied into RuneSchema's runtime DLLs:

| Package | Version | Upstream license |
| --- | --- | --- |
| [CUE4Parse](https://github.com/FabianFG/CUE4Parse) | 1.2.2.202609 | Apache-2.0 |
| [AssetRipper.TextureDecoder](https://github.com/AssetRipper/TextureDecoder) | 2.6.2 | MIT |
| [SkiaSharp](https://github.com/mono/SkiaSharp) | 2.88.9 | MIT |
| Microsoft.Bcl.Memory | 10.0.10 | MIT |

Their transitive dependencies retain their own notices/licenses; see the NuGet
package metadata and `packages.lock.json`. The tool reads desktop block/raw
texture formats through AssetRipper; it does not use CUE4Parse-Conversion or
ImageSharp's encoding pipeline.

Supply your own matching `.usmap` and licensed local Oodle DLL. Neither is
redistributed by this script. Do not include game PAKs, mappings, decoder DLLs,
player saves or private diagnostic files when distributing the cache tool.
Generated artwork/text retains its original rights; a cache is not a rights grant.
