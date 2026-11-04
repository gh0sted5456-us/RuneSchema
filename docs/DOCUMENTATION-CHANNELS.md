# Documentation channels and promotion

| Branch | Documentation |
| --- | --- |
| `main` | Normal guides for the stable release. |
| `experimental` | Feature guides in the website's dedicated Experimental area. |
| `pre-release` | Matching candidate guides in that branch; not a separate website channel. |

The shared diagnostic catalogue covers all channels. Existing code anchors must
keep working; documenting a code does not mean every build emits it.

## Add a feature guide

1. Write `docs/experimental/<guide>.md` with a visible status notice.
2. Set `channel: experimental` and `promote_to: <destination>.md` in frontmatter.
3. Add it to `build/docs-experimental-nav.json` and update the overview/changelog.
4. Document behavior, defaults, setup and remaining limits. Link to shared
   instructions instead of duplicating them.

## Promote to main

Merge the guide into its `promote_to` destination and reconcile existing text,
links, navigation and defaults. Remove the experimental notice and duplicate
entry. Preserve historical changelogs and useful anchors. Update
`CURRENT-RELEASE.md` when the matching package is published, then audit/build
the combined site.

Publishing a candidate or a source commit does not replace stable downloads or
promote its guides. Verify the maintainer's complete ZIP before attaching it to
a prerelease; automatic source archives and binary follow-ups are not full installs.

## Website build

`build/prepare-docs.py` combines main's normal guides with experimental's dedicated
area and shared diagnostic reference. It records both source commits in
`assets/data/documentation-channels.json`.

The Pages workflow deploys from main. A validated experimental update requests
a main rebuild without merging runtime code. Deployments share a concurrency
group; branch builds cancel only older builds from the same branch.
