# RuneSchema project rules

- Keep unreleased runtime code on `experimental` unless the user asks to promote it.
- Document experimental features under `docs/experimental/`, with a visible
  experimental notice and a `promote_to` frontmatter path for each feature guide.
  Add its navigation entry to `build/docs-experimental-nav.json`.
- The public site combines `main`'s normal guides with `experimental`'s dedicated
  area. Do not replace stable instructions with experimental instructions.
  Shared diagnostic anchors cover both channels and must remain backward compatible.
- Promotion includes documentation: merge the feature guide into its `promote_to`
  destination, remove its experimental notice, reconcile existing instructions and
  links, and remove the duplicate experimental guide/navigation entry. Preserve
  historical changelogs and useful existing anchors. Update release information
  only when the package is actually published.
- Follow [the documentation channel and promotion procedure](docs/DOCUMENTATION-CHANNELS.md).
- Keep repository landing pages concise and link to the website for walkthroughs.
  Exclude local attachments, ImGui state, builds and private runtime data from commits.
