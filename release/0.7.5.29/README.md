# RuneSchema 0.7.5.29 artifacts

Processing-station clone safety, PersistenceID-based content relocation,
JSON-owned trace jobs, and the retained-frame Helpy collection interface.

Use the Universal archive for the normal installation, or Core for a
plugin-free runtime. Install the UE4SS archive matching the storefront.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `RuneSchema-0.7.5.29-Core.zip` | 3420583 | `1ECD855D03AC93C7C5970BB6F62B32EA3CCAFA925FFE3D8177923953ED10A4AF` |
| `RuneSchema-0.7.5.29-Universal.zip` | 3895540 | `CD4D6EC7DE70189456D177F63CEA2BDC6D59712465DB39598A13807CCF5B4CF3` |
| `UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip` | 8542528 | `95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0` |
| `UE4SS-3.0.1-f6d5f942-Steam-GOG.zip` | 8541911 | `106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123` |

The processing adapter is experimental. Back up a test save before placing a
runtime-clone output into a timed station queue. A clone is admitted only when
RuneSchema verifies its ItemSubsystem registration, PersistenceID, and rooted
lifetime; a failed verification rejects that placement without disabling the
remaining station recipes.

SafeSave tracks a RuneSchema definition by PersistenceID. Moving the same
identity between mod folders or runtime packages updates its descriptive
owner/path metadata instead of treating it as removed content. Legacy
RuneSchema object paths receive a type-checked stable-name relocation fallback.
Any recipe with an unresolved, ambiguous, or wrong-type object reference is
quarantined before station placement and automatic unlock.
