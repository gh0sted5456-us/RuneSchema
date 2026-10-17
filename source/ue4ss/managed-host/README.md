# RuneSchema managed-package UE4SS host — alpha candidate

RETIRED — 2026-10-15. Do not build, distribute or install this custom host.
RuneSchema must use an unmodified UE4SS.dll. Build and bundle entry points now
reject this route. The implementation below is retained as historical work,
not the supported installation procedure. Previously generated combined ZIPs
are obsolete; their presence in dist does not make them approved candidates.

Not yet in-game validated. This is an optional matching-host update, not a
standalone UE4SS installation. Keep your existing UE4SS loader/proxy, settings,
support files and other mods. Back up the installed UE4SS.dll and RuneSchema
before testing; stop the game/server first. Overlay the bundle's `ue4ss` contents
onto the existing `ue4ss` directory. No package-specific `mods.txt` edits needed.
Do not overwrite existing RuneSchema settings or mods when doing a manual merge.

The matching host and framework are supplied together so complete packages can
stay at `Mods/RuneSchema/mods/<Owner>/ue4ss/<PackageName>/`, including configs,
Scripts, DLLs and dependencies. Enable/disable the parent through RuneSchema.
Restart after changes. Existing same-name UE4SS packages are rejected. Explicit
host `mods.txt` disabled entries remain disabled. No hot unload is implemented.

The extension adds one C export to pinned UE4SS
`f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44`. The public class layout and old exports
are unchanged. A generated translation-unit overlay queues startup registration,
uses actual UE4SS CppMod/LuaMod classes, and orders teardown with native children
before RuneSchema. It does not edit the dependency checkout or hand-call native
mod entry points from RuneSchema. Native ABI compatibility is still required.

Build from the RuneSchema alpha source using `build/build.ps1 -ManagedUE4SSHost`.
The normal build without this flag continues to produce a framework-only update.
Host binaries are not UPX-compressed; the two framework DLLs use the canonical
verified UPX packaging path. The bundle manifest records the file hashes.

UE4SS retains its own MIT license (included under licenses), authorship and
third-party dependencies. Keep the notices supplied with your original UE4SS
installation. RuneSchema/Helpy licensing does not replace those notices.
