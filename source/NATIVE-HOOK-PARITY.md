# Native hook parity audit

RuneSchema uses one `main.dll`, but native code is selected through separate
Steam/GOG and Game Pass/WinGDK lanes. A lane never makes an unverified native
call merely because another storefront exposes a routine with a similar byte
pattern.

## Activation rules

An executable hook is eligible only when all of the following agree:

1. detected storefront lane;
2. PE timestamp and image size;
3. the complete captured bytes at every hook site;
4. the complete captured bytes at every nonzero resume address;
5. the callback's register contract.

If any check fails, only that capability is inactive. RuneSchema, unrelated
loaders, other entries in the same mod, and plug-ins continue loading.

## Audited matrix

| Native capability | Steam/GOG | Game Pass 100.4.0.0 | Safe fallback |
| --- | --- | --- | --- |
| Core engine signatures | Verified profile | Verified WinGDK profile | UE4SS/reflection where available |
| Object enumeration | Verified native `TArray` routine | UE4SS hash tables/object array; native callback iterator rejected | Yes |
| Shadowveil equipment actions | Five verified sites | Five verified sites at `0x6E48153`, `0x6E48171`, `0x6E48224`, `0x6E48192`, `0x6E481BA` | Only Shadowveil binding is inactive |
| Surge equipment behavior | Eight verified sites | Incomplete trace; deliberately inactive | Ordinary equipment effects still load |
| Journal/lore registration | Reflected registry and event hooks | Same reflected registry and event hooks | Entries remain active |
| Journal native JSON save cleanup | Verified Steam adapter | Reader/writer pair traced, helper ABI incomplete; deliberately inactive | Journal content and unlock delivery continue |
| Appearance event bridge | Full signature contract required | Full signature contract required | Consumer reports unavailable; other systems continue |
| Merchant stock refresh | Reflected hooks first; native cache optional | Reflected hooks first; native cache optional | Scheduled replicated-state refresh |
| Quest, dialogue, event, time-of-day and spawn hooks | Reflected `UFunction`/RuneSchema API | Same | Feature-scoped warning |

## Game Pass ground truth

The audited WinGDK image is version `100.4.0.0`, timestamp `0x9924253F`, image
size `0x0DB11000`. Journal writer and reader were traced at RVAs `0x6EA3590`
and `0x6EA3800`. They are recorded as evidence only; RuneSchema does not invoke
the native save adapter until every JSON helper and ownership rule is verified.

## Update policy

Game updates normally change the PE identity and cause native capabilities to
stay inactive. Add a new immutable profile after tracing that build; do not
replace an older profile or loosen byte validation. This preserves rollback
support and prevents Steam signatures from affecting Game Pass or vice versa.
