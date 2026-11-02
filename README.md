# RuneSchema

A runtime modding framework for **RuneScape: Dragonwilds**.

RuneSchema connects JSON/JSONC definitions and cooked PAK content to supported
game systems. **Helpy**, its companion desktop interface, provides item browsing,
a staging cart, mod management, map tools and diagnostics.

> **Pre-release testing · 0.8.1.0:** this branch contains the candidate source and
> [testing notes](docs/experimental/PRE-RELEASE.md). The complete runtime ZIP is
> being assembled separately by the maintainer. GitHub's source archives are not
> installable runtime packages. Existing stable downloads remain available.

**[Documentation website](https://gh0sted5456-us.github.io/RuneSchema/)** ·
**[Downloads](https://github.com/gh0sted5456-us/RuneSchema/releases/latest)** ·
**[Report a bug](https://github.com/gh0sted5456-us/RuneSchema/issues/new/choose)**

## Get started

| I want to… | Guide |
| --- | --- |
| Install RuneSchema and use Helpy | [Player guide](https://gh0sted5456-us.github.io/RuneSchema/users/) and [installation](https://gh0sted5456-us.github.io/RuneSchema/INSTALLATION/) |
| Create or adapt a mod | [Creator guides and worked examples](https://gh0sted5456-us.github.io/RuneSchema/modders/) |
| Run a dedicated server | [Server guides](https://gh0sted5456-us.github.io/RuneSchema/servers/) |
| Understand a warning or error | [Troubleshooting](https://gh0sted5456-us.github.io/RuneSchema/TROUBLESHOOTING/) and [diagnostic codes](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/) |
| Check what changed | [Downloads and source-update status](https://gh0sted5456-us.github.io/RuneSchema/CURRENT-RELEASE/) |

The website contains requirements, configuration, compatibility notes, authoring
walkthroughs and the API/reference guides. Use the **Releases** download for an
installable package; branch source can contain changes newer than that package.

## Source and contributions

`main` contains the maintained source and documentation. `experimental` receives
integration updates; its guides identify defaults, setup and testing status.
`pre-release` holds an assembled candidate for testing before promotion. Source
publication does not replace a release ZIP, move a tag or promote stable guides.

For this candidate, use the [guides in this branch](docs/experimental/index.md).
The public website continues to combine the stable and experimental channels.

See the [Experimental feature guides](https://gh0sted5456-us.github.io/RuneSchema/experimental/)
for upcoming behavior, setup and testing status. Those guides are separate from
the normal documentation until the features are promoted to main.

See [Contributing](CONTRIBUTING.md) for source contributions. For bug reports,
include the RuneSchema version, game platform, reproduction steps and relevant
diagnostic codes. Review logs before sharing them.

Contributing with an assistant? Read **[If Using AI](IF-USING-AI.md)** for the
investigation, build verification and review expectations.

## Community and license

Developed and maintained by the **RSDW Modding Community**. See
[community credits](AUTHORS.md).

Copyright (c) 2026 RSDW Modding Community. Covered original code and documentation
are available under the [MIT License](LICENSE). See [license scope](LICENSING.md)
and [third-party notices](THIRD_PARTY_NOTICES.md).

PalSchema foundation by Okaetsu. UE4SS and other dependencies retain their
original copyright and license notices.
