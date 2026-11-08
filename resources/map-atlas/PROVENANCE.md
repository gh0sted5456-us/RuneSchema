# Helpy map atlas inputs

The surface geometry comes from the measured Dragonwilds persistent level
/Game/Maps/World/L_World. NativeRegions.json is the input snapshot. Its
SHA-256 is b5996ea8f88ceaa9c9949c93f1f60895c2938ec4acad82bc1ce699cb73a7649e.
The compiled region data preserves its XY coordinates to 0.01 cm. The
mainland raster comes from RuneSchema's existing native world map cache.
Its manifest records the image bounds, orientation and SHA-256.

Scorned-Art.png crops the visible Scorned Wilderness inset directly from
the verified native game raster and masks adjacent mainland art. Artwork
© Jagex. The cropped artwork is separate from the MIT-licensed source.

Created using intellectual property belonging to Jagex Limited under the terms of Jagex's [Fan Content Policy](https://legal.jagex.com/docs/policies/fan-content-policy).

The native asset, mask source and byte hashes are in
Scorned-art-provenance.json. The published outline is used only for display
cropping and to identify the illustrated inset. Native mainland polygons
take precedence where they overlap the illustration.

The artwork inset has a different layout from the native world geometry.
Helpy opens the surface atlas on game artwork. The shared Coordinate overlay
switch adds surveyed outlines, pins and position selection in Overview,
Mainland, Scorned Wilderness and each named surface area. The native mainland
raster and its region crops use the same world frame. The Scorned illustration
uses the game's display layout, and its pins follow the surveyed outlines. Dungeon floors and fog boundaries remain unverified in live play.

## Rasthin's Rest travel

The cooked Ashenfall persistent level contains the native
TeleportationTarget named ScornedWilderness_Entrance. Its root capsule is
at X -705215.561676, Y 770592.612080, Z 13144.788236 in Unreal centimeters.
Helpy offers this point from the Scorned Wilderness tab and portal marker.
The host checks the live named target against the survey before streaming
terrain and verifying a walkable landing and character capsule clearance.
The game then runs its named Dragon Gate travel with its loading screen.
The arrival point is in Rasthin's Rest. The game owns raid entry and story
interaction.
