# RuneSchema licensing and ownership

## Community and publication

RuneSchema began within the RSDW Modding Community and is developed and
maintained as a community project. Credited members are **Jonesing4Space,
NuLLZz, Snorkles, and CHP**; the consolidated roster and historical credit are
in [AUTHORS.md](AUTHORS.md). Official repository and distribution work are
published under **gh0sted5456-us** at
https://github.com/gh0sted5456-us/RuneSchema.

RSDW Modding Community owns its original RuneSchema work, as declared by the
project owner. Community attribution does not itself transfer a member's
copyright or establish a license for that member's contribution.

## MIT grant and its scope

The [MIT License](LICENSE) applies to the original software, accompanying
documentation, schemas, and example definitions in this repository to the
extent that RSDW Modding Community owns or has authority to license them.
The grant permits use, modification, copying, redistribution, sublicensing,
and commercial use of that covered work, subject to preserving the copyright
and permission notices. Source publication and individual approval of forks
are not required by MIT.

Third-party material remains under its applicable original license or other
permission. A root MIT file, repository location, filename, or project credit
does not override those terms. See [Third-party notices](THIRD_PARTY_NOTICES.md).
Where coverage differs, use the license attached to the relevant component.

## Existing contributions

Community credits include the original RuneSchema work by Snorkles. Preserve
applicable author notices and the license or permission attached to the
revision incorporated. Neither this roster nor the root license supplies a
missing grant for a contribution the community does not own or have authority
to license. Any such permission must be established before that contribution
is represented as MIT-covered or redistributed.

## Names, logos, and game or engine content

The software license is not a trademark license and does not authorize false
claims that a fork is an official RSDW Modding Community release. Factual
attribution and descriptions of compatibility are not prohibited by this
statement. Independently licensed artwork, third-party assets, game data, and
Epic Games engine material are not relicensed by the software's MIT grant.

The code license does not grant rights held by Jagex, Epic Games, Microsoft,
or other third parties. Applicable game and engine terms remain separate;
no non-commercial restriction is being added to the MIT-covered code.

## Distribution

Our release packaging combines `LICENSE`, `AUTHORS.md`, `LICENSING.md`,
`CONTRIBUTING.md`, `THIRD_PARTY_NOTICES.md`, and every collected dependency
notice into the single file `settings/licenses/RuneSchema-Licenses.txt`.
That document includes complete notice texts, component names, original source
paths and hashes, and upstream links. This is the project's packaging practice,
not an additional condition on the MIT grant.
Downstream obligations are those in the applicable component licenses.
Preserve required upstream copyright and permission notices rather than
replacing their authors with the project name. An acknowledgment or an
upstream URL alone is not a substitute for the required notice text.

The supported `Build RuneSchema.bat` launcher finalizes the generated Core,
Universal, and Helpy ZIPs through `build/package-licenses.ps1`. The same helper
is used for the test-bed source and example packages. Direct users of the
internal `build/build.ps1` must run `build/package-licenses.ps1 -FinalizeBuild`
after a successful build and before distributing its output.

The helper includes the checked-in license bundle and collects license/notice
files present in the local dependency and build caches. Collection is not a
complete legal audit: maintainers must review component-specific obligations,
including source-offer requirements for any redistributed GPL tools and
restrictions on game/engine material. Do not distribute a bare DLL without its
accompanying notices. Do not apply RuneSchema's root license to a standalone
UE4SS bundle; preserve UE4SS's own notices and those of its dependencies.

New packaging rules do not modify existing GitHub Release assets, Nexus
uploads, historical tags, or other branches. Check and correct those packages
separately before redistributing them.

## Contributions

New contributions follow [CONTRIBUTING.md](CONTRIBUTING.md). Licensing a
contribution does not itself transfer its copyright to the organization.
