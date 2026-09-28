RuneSchema 0.7.6 builder

Run build.bat from this folder (or Build RuneSchema.bat from the repository
root) to build RuneSchema.

The source repository no longer carries assembled runtime templates, previous
release ZIPs, or UE4SS storefront archives. On first use the builder downloads
the pinned RuneSchema-BuildDependencies package from the experimental GitHub
release, verifies its SHA-256 hash, and stores it under ..\.cache\dependencies.
Later builds reuse that cache.

CMake continues to fetch the pinned UE4SS/source dependencies required to
compile RuneSchema over HTTPS. The build produces:
  - RuneSchema-<version>-Universal.zip
  - RuneSchema-<version>-Core.zip
  - optional Helpy plugin package

Output is written to ..\dist. Steam/GOG and Game Pass/WinGDK UE4SS runtime
archives are separate GitHub Release assets and are not copied into dist.

Use build.bat -PluginOnly to rebuild and package Helpy without compiling or
replacing the main RuneSchema DLL.

Signing is optional and never blocks package creation. A signing utility can
be downloaded from a GitHub HTTPS release only when RUNESCHEMA_SIGNER_URL and
RUNESCHEMA_SIGNER_SHA256 are both configured. A valid code-signing certificate
is still required. If no certificate/signing tool is available, the build stays
unsigned and continues.
