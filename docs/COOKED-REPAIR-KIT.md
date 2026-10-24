# Cooked repair items — boundary and native evidence

Status: cooked-only implementation requested on 2026-10-23; functional gameplay
verification pending. No repair test files were installed.

## Boundary

The mod PAK owns complete CanUseItem/UseItem behavior, eligibility, prices, stack
limits, icons, purchases, costs, station routing and consumption. RuneSchema
provides normal loading/registration only. There is no repair-specific runtime
action, filter callback contract, interceptor or service.

The earlier RepairPersonalEquipment/FilterFunction prototype was removed. Do not
author declarations for it. Colors fixtures v1/v2 have false-only event bodies:
they are inert ABI fixtures, not functional or release-ready items.

## Native Blueprint surface observed

Current Steam/Game Pass headers expose:

- DominionPlayerController.GetInventory() and GetLoadout(), both BlueprintPure.
- UsableItemInterface.CanUseItem(PlayerController, out GameplayTag) -> bool.
- UsableItemInterface.UseItem(PlayerController, Inventory, SlotIndex) -> bool,
  BlueprintAuthorityOnly.
- InventoryComponent.GetItemSlots(), GetItemFromSlot(SlotIndex).
- InventoryComponent.RemoveFromSlot(SlotIndex, Count, Instigator) -> bool.
- Item.BP_GetItemData(), GetGuid(), GetStackSize(), GetDurability(),
  GetMaxDurability(), IsUnbreakable(), ShouldDestroyWhenBroken().
- Item.ModifyDurability(Amount), using native change notifications.
- Actor.HasAuthority() for the owning player pawn.

These are callable declarations, not complete gameplay proof. Use the consuming
controller's actual native bag/loadout, never nearby actors or storage.
Deduplicate item instances/GUIDs. Preserve ItemData, appearance and masterwork
properties. Plan and check eligible damage before mutation, use native durability
deltas, then consume exactly one unit from the verified original kit stack after
confirmed repair. No-damage/failure must not consume a unit. Rollback, replay and
server behavior require live verification.

No native Item.SetDurability was found in this capture. Do not author against
that assumed function. RepairEquipmentOfNearbyPlayers is not a personal-kit API.

## Vanilla benches — separate requirement

RecipeData.RepairItemReplacements substitutes repair ingredients, not item
identity. Static native inspection found an exact output-ItemData/count-one
check in bench repair. An unchanged donor recipe cannot repair a dyed variant.
Do not temporarily mutate shared vanilla recipes.

The bounded CL254097 native trace found that the ordinary repair UI needs a
learned recipe to publish positive costs and enable repair; a locked recipe is
not a complete solution. Use a distinct stable-ID, exact-output/count-one cooked
recipe preserving the native donor's cost and repair fields. Ordinary cooked
registration can declare the full path in `NativeRegistries.Recipes` and
`LoaderDefinitions.Recipes` with `Unlock:true`, without `AddTo` or a new runtime
repair action. `Unlock:true` becomes saved character learning, not transient
availability. Cook `bIgnoreAutomaticUnlocks` and `bIgnoreNotification` into the
recipe, then verify the cooked values rather than inventing JSON suppression keys.

A mod-owned typed station appender must preserve existing native/third-party
`ValidRecipes`, use exact station table/row membership and append uniquely only
after initialization. First matching output wins in the traced selection path;
check competing recipes. Registration does not automatically execute this
initializer and no dedicated-server ModActor startup is assumed. Cache loaded
batches rather than synchronously reloading thousands of recipes on each retry.
Client/server item and recipe identities must complete normal native/network
enrollment with matching content. Preserve native prefixes and registry/shard
budgets. This source/contract review and the editor fixture cases do not certify
a successful in-game repair. Keep the original benches and native cost policy;
do not mutate donor outputs or add a repair-only alias service.

## Evidence and release gates

Evidence: artifacts/cooked-repairs-20261023/{Steam,GamePass}.
Native headers: installed ue4ss/UHTHeaderDump/Dominion/Public, captured 2026-10-23.
Game Pass package: 100.102.0.0. All 1225 RecipeData exports and selected station/
donor exports match across platforms. Master Staff uses RECIPE_Weapon_Staff_Yew.

Before release, verify cooked-only standalone and real client/server execution,
zero/broken and no-damage items, exact stack-one removal for counts 1/2/10,
inventory/loadout overlap, scoped exclusions, replay, rollback, replication,
save/reload and ordinary vanilla-bench repairs. Broken destroy-on-break items
need explicit native-safe handling. Container parsing or Blueprint compilation
alone does not demonstrate successful repair.
