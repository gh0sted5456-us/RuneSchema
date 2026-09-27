# RuneSchema 0.7.6.1

This revision hardens player nameplate lifecycle and journal-to-recipe routing.

- Activity badges now close the native Dominion widget visibility gate when a
  pulse expires. Injected icons are collapsed and cannot be restored by the
  game's next distance/visibility update.
- Nameplate tracking retains both the player pawn and its widget. Periodic
  dead/gameplay-effect refreshes therefore revisit the pawn instead of
  accidentally treating the widget as a player.
- `/nameplates` and inline `/players.Nameplate` add `AlwaysFaceCamera` and
  `OnlyShowNearby`, backed by the native `UDominionWidgetComponent` fields.
- Journal `RecipeData` references use the authoritative `/recipes` resolver
  with the journal's owning ModID. Duplicate stale Unreal objects no longer
  make one valid authored recipe appear ambiguous.
- Runtime-created journal and lore identities are reasserted after reflected
  writes before ownership is recorded. Validation errors now name the invalid
  identity field instead of emitting one generic message.

No new AOB or storefront-specific hook is required. These changes use reflected
Dominion/UMG APIs and the existing shared Steam/GOG and WinGDK loader path.
