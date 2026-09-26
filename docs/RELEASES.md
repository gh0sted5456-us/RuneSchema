# RuneSchema 0.7.5.28

**Current public release**

RuneSchema 0.7.5.28 is the current stable documentation target.

## Highlights

- one universal RuneSchema DLL for Steam/GOG and Game Pass/WinGDK;
- storefront-specific native lanes remain isolated;
- Plugin API 1 remains current;
- journal finalization uses one-pass reference indexes and cached objects;
- SafeSave keeps Steam/GOG and Game Pass ownership handling separate;
- current authoring, loader, Unreal + RuneSchema, and compatibility guides are
  written against 0.7.5.28.

## Reference performance

The September 25 optimization-cycle baseline recorded journal finalization at
about **306 ms on Steam/GOG** and **410 ms on Game Pass/WinGDK** after the
journal indexing change.

See [Startup Performance](STARTUP-PERFORMANCE.md) for the full breakdown.

## Downloads

Release packages are stored in the repository:

- [RuneSchema 0.7.5.28 release folder](https://github.com/gh0sted5456-us/RuneSchema/tree/main/release/0.7.5.28)
- [Universal runtime](https://github.com/gh0sted5456-us/RuneSchema/blob/main/release/0.7.5.28/RuneSchema-0.7.5.28-Universal.zip)
- [Core-only runtime](https://github.com/gh0sted5456-us/RuneSchema/blob/main/release/0.7.5.28/RuneSchema-0.7.5.28-Core.zip)

Historical implementation notes remain in the repository but are not part of
the public documentation navigation.
