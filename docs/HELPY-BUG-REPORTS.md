# Bug reports in Helpy

Report a bug is available by default in navigation and on Home, including at the
game menu. No world or character is required. Draft text remains while switching
pages in that Helpy session; it is not automatically written to disk or uploaded.

The form includes title, category, what happened, reproduction steps, expected
behavior, and optional additional text or a manually pasted log excerpt. The
optional runtime section includes only platform and RuneSchema/Helpy/game/UE4SS
version strings. It does not collect logs, files, saves, mod lists, usernames,
account IDs or install paths. The exact report can be previewed or copied.

GitHub issues are public. Review the content for sensitive information. Review &
submit on GitHub opens a title/body-prefilled issue for gh0sted5456-us/RuneSchema.
The user must sign into GitHub and press Submit new issue there. Opening the
browser is not confirmation that an issue was posted. No GitHub tokens, bot
credentials, OAuth flow or background posting are included in this version.

Reports whose encoded URLs exceed 6000 bytes use Copy report & open GitHub:
the complete body is copied, never silently truncated, and the user pastes it
into GitHub before submitting. Copy report also supports manual submission if
the default browser cannot be opened. GitHub's title/body query parameters are
documented in [Creating an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/creating-an-issue#creating-an-issue-from-a-url-query).

Categories are preserved in the issue body, rather than relying on permissions
to apply repository labels. A future direct-submit bot requires a separately
secured backend and explicit user review/consent; never ship a shared write token
inside Helpy or RuneSchema. No external reports are created by automated tests.
