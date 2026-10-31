---
channel: experimental
---

# Experimental RuneSchema

Try the upcoming features here. The normal player, creator and server guides
describe **main**; this area follows the **experimental** branch.

**Current experimental source: 0.8.1.0.**
[Source on GitHub](https://github.com/gh0sted5456-us/RuneSchema/tree/experimental)
· [Itemized changelog](CHANGELOG-0.8.1.0.md)
· [Current stable download](../CURRENT-RELEASE.md).

A source update does not replace a release ZIP. Keep the core DLL, Helpy bridge,
application and bundled content from a matching build. Preserve your settings,
mods and saves when updating. Start with [experimental installation](INSTALLATION.md).

## What you can try

| Area | Features and walkthrough |
| --- | --- |
| Helpy desktop | Offline cached browsing, separate runtime instances, tray controls and runtime-labelled shortcuts. [Desktop guide](HELPY-STANDALONE.md). |
| Clearer menus | Category column, grouped right-pane tabs, consistent switches, wrapping descriptions and independent scrolling. Home's Quick Settings uses the same controls. [Settings walkthrough](HELPY-STANDALONE.md#startup-and-the-system-tray). |
| Native game overlay | The normal Helpy interface rendered through the game bridge, with mouse capture, scale controls and controller navigation. [Renderer requirements and setup](NATIVE-OVERLAY.md). Vulkan startup enrollment is opt-in. |
| Faster interaction | Immediate isolated bridge updates, bounded notification bursts and no save/launch reads in unrelated settings tabs. [Performance and lifecycle](HELPY-STANDALONE.md). |
| Authoring and repair | Installed-mod cache builds, ZIP/7z creation and conversion, progress controls and an offline base-file repair tool. [Tools walkthrough](AUTHORING-AND-REPAIR.md). |
| Server requests | A world-specific PIN enables validated self-service item delivery and map travel through the authority. [Server setup](HELPY-STANDALONE.md#server-access-with-a-world-pin). |
| Cooked content | Broader registry enrollment and cooked table extensions, with JSONC fallbacks and overrides. [Cooked declarations](COOKED-PAK-REGISTRY-MANIFEST.md) and [table extensions](COOKED-TABLE-EXTENSIONS.md). |
| World policies | Optional placement, material, farming and world-effect adapters with independent availability checks. [World policies](BUILDING-WORLD-POLICIES.md). |
| Skills | A documented investigation and current boundaries; this is not a promise of a complete skills registration API. [Skills investigation](SKILL-REGISTRY.md). |
| Diagnostics | Overlay readiness, presentation and failure codes, with messages in RuneSchema.log. [Shared code reference](../ERROR-CODES.md#legacy-family-rs-ovl). |

## Testing status

The maintained local candidate passed **255 core tests and 143 Helpy tests**.
Two graphics focus checks passed on a serial retry. UI captures cover 1280×900,
960×700 and 760×560; native graphics fixtures cover DX11, DX12, OpenGL and Vulkan
on the development NVIDIA GPU. These are separate from live gameplay acceptance.

The desktop overlay has had user testing, including exclusive fullscreen and a
hybrid Intel/NVIDIA laptop. Continue testing interaction speed, focus, controller
input and your own rendering setup. Current adapter limits and remaining checks
are listed in the [native overlay guide](NATIVE-OVERLAY.md).

## Give feedback

Include your build, Steam/Game Pass/server runtime, renderer and steps to
reproduce. Use [Helpy's copy-and-paste bug report](HELPY-BUG-REPORTS.md).
Review any logs before sharing them.

When a feature is promoted to main, its guide is merged into the normal
documentation and its duplicate experimental entry is removed. See
[documentation channels and promotion](../DOCUMENTATION-CHANNELS.md).
