# Helpy original glyphs

64 original monoline glyphs for map layers/resources, party stats, equipment,
and common actions. Licensed under this repository's MIT licence. These are
original RuneSchema artwork, not copied website or game artwork.

The canonical 24-unit paths live in
`source/helpy/source/include/Core/HelpyGlyphs.h`. The native renderer draws those
same paths directly, with the current Helpy accent colour and DPI scaling.
No artwork download or per-marker network request is needed.

Run `helpy-glyphs <output-folder>` to validate/export the individual SVGs,
`index.html`, and `atlas.svg`. `v1/atlas.png` is a rendered review sheet.
The SVGs have transparent backgrounds; the review sheet has a dark background.

The game's native power-level badge remains the primary party power-level
display. The original fallback glyph in this pack is not presented as native
game artwork. Glyphs for hunger/thirst/timers do not imply those measurements
have been verified or are available from the runtime.
