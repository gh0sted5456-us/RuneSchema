# Player nameplate test

This server-and-client test keeps ordinary player names while alive. The first
player gets a small permanent white-square role badge. On death, either base is
replaced with the same built-in texture at 1.5 scale. Three seconds after the
replicated death state clears, each player returns to their own base nameplate.

The deliberately plain built-in texture avoids requiring a custom pak for the
first runtime test. Replace it with a cooked mod texture after the lifecycle is
confirmed.
