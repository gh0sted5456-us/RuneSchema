RuneSchema 0.7.5.21 builder

Run build.bat from this folder (or double-click it) to build both storefronts.
The script checks GitHub connectivity, fetches pinned UE4SS and all other CMake
dependencies over HTTPS, builds one storefront-agnostic RuneSchema DLL, compresses
the release DLLs with the bundled UPX, and writes one drag-and-drop Universal ZIP
to ..\dist. RuneSchema detects Steam/GOG versus Game Pass/WinGDK at runtime.
The optional ..\plugins\Universal directory contains the same built DLL payload.
The build also writes a RuneSchema-0.7.5.21-Core.zip with no plugins, proving
that RuneSchema core and local loaders do not depend on Helpy or Networking.
The distinct UE4SS runtime archives remain in ..\runtime and are copied to ..\dist.
Use `build.bat -PluginOnly` to rebuild and package Helpy without compiling or
replacing the main RuneSchema DLL. Its version and console announcement come
from the plugin's own plugin.json manifest.

Signing is optional and never blocks package creation. A signing utility can
be downloaded from a GitHub HTTPS release only when RUNESCHEMA_SIGNER_URL and
RUNESCHEMA_SIGNER_SHA256 are both configured. A valid code-signing certificate
is still required. If no certificate/signing tool is available, the build stays
unsigned and continues.
