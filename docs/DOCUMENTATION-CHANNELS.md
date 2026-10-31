# Documentation channels and promotion

The website has two areas:

- **Normal guides:** documentation from `main`, for its released capabilities.
- **Experimental:** feature guides from `experimental/docs/experimental/`,
  published at [Experimental](experimental/index.md).

An experimental push updates that area without replacing main's player,
creator or server instructions. The shared diagnostic reference covers both
channels so codes in new logs keep working public links. A documented code is
not a claim that every build can emit it.

## Add an experimental feature

1. Write its guide under `docs/experimental/` and show an experimental notice.
2. Add `channel: experimental` and `promote_to: PATH.md` to its frontmatter.
   `promote_to` names the normal documentation destination after promotion.
3. Add the page to `build/docs-experimental-nav.json`. Link to normal guides
   for shared concepts instead of repeating all installation/authoring basics.
4. Describe the actual behavior, defaults, opt-ins, tests and remaining limits.
   Separate implemented features from investigations.
5. Update the experimental overview and changelog. Keep the GitHub README short
   and point to these guides for details.

## Promote a feature to main

Promotion includes the code, matching package and documentation review:

1. Merge the feature's instructions into its `promote_to` destination. Reconcile
   the existing guide; do not simply append a second, contradictory procedure.
2. Remove its experimental notice and frontmatter from the promoted guide.
   Update the normal navigation, links, defaults and relevant walkthroughs.
3. Remove the duplicate experimental feature guide and its navigation entry.
   The experimental overview can link to the normal guide as promoted.
4. Preserve historical changelogs and useful existing URL anchors. A historical
   experimental changelog may remain clearly labelled as history.
5. Update `CURRENT-RELEASE.md` only when its matching downloadable package is
   actually published. Merging source is not the same as replacing release assets.
6. Build and audit the combined site before publishing. Verify stable guides,
   experimental navigation and diagnostic links together.

For example, the experimental desktop guide names `HELPY-STANDALONE.md` as its
destination, while the authoring/repair walkthrough is merged into
`ADVANCED-AUTHORING-TOOLS.md`. These destinations describe documentation routing;
they do not automatically promote runtime behavior.

## How the public site is assembled

The Pages workflow checks out `main` and `experimental` separately.
`build/prepare-docs.py` copies main's documentation, replaces only the dedicated
experimental area, and adds its navigation. The generated diagnostic page and
JSON catalogue are the shared exception and retain their existing public anchors.
Both source commit IDs are recorded in
`assets/data/documentation-channels.json` on the site.

Public deployment runs from main, matching the repository's Pages environment.
After experimental validates its combined site, it requests a main documentation
rebuild using the workflow's built-in GitHub token. That rebuild reads the latest
experimental guides without merging experimental runtime code into main. It does
not loop: only an experimental run requests the follow-up.

Pages deployments use one shared concurrency group; branch builds cancel only
older builds of the same branch.
Local builds of experimental can still use its ordinary `mkdocs.yml`; the
combined build is the public-site acceptance check.
