# RuneSchema project rules

- Read [Contributing](CONTRIBUTING.md) and [If Using AI](IF-USING-AI.md).
  Read ignored `AGENTS.local.md` when present; never publish its private contents.
- Use `experimental` for unreleased integration work and `pre-release` when
  the maintainer requests a candidate. Promotion to `main` is separate.
- Put feature guides in `docs/experimental/`, with a status notice and
  `promote_to` path. Register them in `build/docs-experimental-nav.json`.
- The website combines normal guides from `main` and the dedicated area from
  `experimental`. Keep shared diagnostic anchors backward compatible.
- Promotion includes merging the guide into its destination, reconciling links
  and removing the duplicate experimental entry. Preserve useful history.
- Follow [documentation channels](docs/DOCUMENTATION-CHANNELS.md). Keep landing
  pages concise, retain licenses and publish only verified matching packages.
- Exclude personal instructions, credentials, settings, saves, logs, caches,
  attachments, ImGui state and build output from contributions.
