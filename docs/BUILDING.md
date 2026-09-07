# Building and size accounting

Every release requires a full rebuild. Use tools/build-cached.ps1 with the configured MSVC/source cache; it defaults to work/dependency-core-build. It rebuilds the six retained static libraries, builds and tests RuneSchemaJsonCore independently, then rebuilds RuneSchema with an explicit library list. Never package incremental release objects. The target uses /MD and explicit /O1 /Os Shipping optimization.

tools/build-dependencies.ps1 rebuilds Zycore, Zydis, SafetyHook, efsw, fmt and ImGui from local source snapshots using isolated object/output directories. It retains per-library logs and input/output hashes. UE4SS.dll is not rebuilt or replaced; its existing import library is retained and imports must be verified against the installed runtime. C++ ABI compatibility with that host remains a separate requirement. Local generated reference projects and source/header caches are prerequisites; the source ZIP does not bundle third-party code or binaries.

The standalone core can be configured with cmake -S core -B <fresh-output> -DRUNESCHEMA_JSON_INCLUDE_DIR=<nlohmann-json/include>, then built with --clean-first and tested with ctest. It requires no UE4SS headers or libraries. File/value parsing and patch processing live in src/Core; FVector, FRotator and FName conversion remains in Utility/JsonHelpers.cpp. No second runtime DLL is needed: the core links statically into RuneSchema.

The repository-wide CMake path still follows UE4SS's upstream target propagation and may list additional transitive libraries. The verified reduced-link shipping pipeline is tools/build-cached.ps1. Keep those two routes distinct when reporting validation; do not claim an untested full CMake dependency graph matches the reduced local link command.

Only the RuneSchema DLL is replaced. Keep the tested UE4SS runtime and verify every imported symbol against that runtime before installation. The source package does not bundle dependency binaries. Existing compiler/cache paths in build scripts are local setup requirements.

C++ comments and separate Markdown files are not compiled into the DLL. Moving notes improves source readability and packaging, not runtime memory or binary size. Runtime log/help strings, executable instructions, metadata and linked library code can affect the DLL. Removing a log call can also remove its formatting instructions. Measure the rebuilt file rather than counting removed comment bytes.

The runtime ZIP contains dlls/main.dll and a short installation README. Guides, implementation notes and version history are available in the documentation ZIP and source package. PDB symbols are preserved separately for crash analysis. The validation report records file hashes, DLL size, compatible imports and the source boundary audit.

This candidate gates core initialization on UE4SS readiness. Validate queue/concurrency boundaries, source sequencing, compilation and imports before packaging; live gameplay/boot validation remains separate.
