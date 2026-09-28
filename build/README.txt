RuneSchema 0.7.6 builder

Run build.bat from this folder (or Build RuneSchema.bat from the repository
root) to build RuneSchema.

CLEAN CONFIGURATION
-------------------
The builder avoids repeatedly configuring the large UE4SS CMake graph.

A SHA-256 configuration fingerprint is generated from RuneSchema CMake files
and the set of source .cpp files. If build/cache/universal/build.ninja exists
and that fingerprint has not changed, the builder skips the explicit CMake
configure step and goes directly to Ninja.

Adding, removing, or renaming a .cpp file changes the fingerprint and causes
one clean reconfigure. Editing a CMakeLists.txt does the same.

RuneSchema intentionally does not use CMake CONFIGURE_DEPENDS for its source
glob. This prevents Ninja's verify-globs step from repeatedly regenerating the
UE4SS/UnrealVTableDumper dependency graph during a normal build.

Pinned FetchContent dependencies are allowed to download on the first
configuration. Once the UE4SS checkout exists, later reconfigures use
FETCHCONTENT_UPDATES_DISCONNECTED=ON. Upstream CMake developer/deprecation
warnings are suppressed during configuration; real configure/build errors are
still shown.

BUILD DEPENDENCIES
------------------
The source repository does not carry assembled runtime templates, previous
release ZIPs, or UE4SS storefront archives. On first use the builder downloads
the pinned RuneSchema-BuildDependencies package from the experimental GitHub
release, verifies its SHA-256 hash, and stores it under
..\.cache\dependencies. Later builds reuse that cache.

CMake fetches the pinned source dependencies required to compile RuneSchema
over HTTPS. Generated packages are written to ..\dist:

  - RuneSchema-<version>-Universal.zip
  - RuneSchema-<version>-Core.zip
  - optional Helpy plugin package

Steam/GOG and Game Pass/WinGDK UE4SS runtime archives are separate GitHub
Release assets and are not copied into dist.

Use build.bat -Clean when you intentionally want to discard generated CMake
state and perform a fresh configure/build. The downloaded .cache dependency
bundle is retained.

Use build.bat -PluginOnly to rebuild and package Helpy without compiling or
replacing the main RuneSchema DLL.

Signing is optional and never blocks package creation.
