# Rihla — Remaining Work (Canonical v6)

**Engine:** Unreal Engine 5.7.4
**Project:** Rihla
**Repository:** `ahmedalhosani1ae-prog/Rihla`
**Target:** TotK-inspired inventory, equipment, quick-select, player-resource, durability, and save systems.

This is the canonical planning document for the remaining Rihla work.

It replaces the earlier A–O planning documents and Canonical v5.

The project already has substantial inventory/quick-select work completed, plus the native C++ `PlayerStatsComponent` foundation.

The following systems are intentionally **not part of Rihla's planned feature set**:

* Building / Ultrahand.
* Ancient Relic construction.
* Cooking.
* Battery.
* Weapon fusion.
* Weapon infusion.
* Arrow fusion.
* Arrow infusion.
* Any other permanent or temporary item-fusion/infusion system.

The project should not introduce these systems later unless the design is deliberately changed.

---

# 0. Purpose and Non-Negotiable Architecture

Rihla uses the following ownership model:

* **`DT_ItemData` / `S_ItemInfo`** define what an item type is.
* **`S_ItemSlot`** stores state belonging to a specific inventory instance/stack.
* **`BPC_Inventory`** owns inventory state and all inventory mutations.
* **`PlayerStatsComponent`** owns player health and stamina.
* **`BPC_QuickSelect` / `WBP_QuickSelectMenu`** own quick-select presentation and selection flow, not inventory state.
* **`S_GameData`** owns persistent save state.

The following rules are mandatory:

1. **Static item data is never modified at runtime to represent a particular item instance.**
2. **Array indices are positional only, never identity.**
3. **Every unique inventory instance/stack carries a stable `ItemInstanceID` (`FGuid`).**
4. **Inventory mutations are authoritative and transactional.**
5. **UI requests gameplay operations; UI does not directly mutate inventory or player stats.**
6. **Effective item values are resolved through one shared path instead of being recalculated independently in multiple systems.**
7. **Transient state and persistent state are explicitly separated.**
8. **A feature must not introduce a second source of truth for data another existing system already owns.**
9. **No fusion or infusion state exists anywhere in the inventory architecture.**
10. **No cooking-generated item state exists in the inventory architecture.**
11. **No building system or build-specific inventory transaction exists.**
12. **Battery is not a player resource.**

---

# 1. Already Completed / Verified in the Current Project

The current project confirms that the following foundations are present and should not be rebuilt:

* Hotbar removal.
* Quick-select system creation.
* Equipment highlight system.
* Equipped items remain in their normal inventory grid slot.
* No separate equipment circles.
* Drop duplication bug fixed in the current gameplay flow.
* Phantom inventory-slot bug fixed in the current gameplay flow.
* Pause-on-open.
* Pause toggle behavior.
* Quick-select carousel visual polish.
* Quick-select Widget Animation scale/opacity.
* Smooth ScrollBox scrolling.
* Quick-select slot count reflects the number of relevant items.
* `S_ItemSlot` contains `ItemInstanceID`.
* `S_ItemSlot` contains durability and usage state.
* `BPC_Inventory` contains `FindSlotByInstanceID`.
* `BPC_Inventory` contains `EquipItemByInstanceID`.
* Equipment/save data use instance-ID-based references in the current implementation.
* Native `PlayerStatsComponent` exists and is attached to the Character.

## Native C++ foundation already delivered

### `PlayerStatsComponent`

The native C++ component already provides:

* Current/max health.
* Temporary health.
* Current/max stamina.
* Temporary stamina.
* Clamped resource mutation.
* Health/stamina change delegates.
* Death delegate.
* `ApplySavedStats`.
* `InitializeNewGameStats`.
* `SetMaxHealth`.
* `SetMaxStamina`.

The component is already attached to the Character.

Remaining work is gameplay/UI integration, resource-policy cleanup, and save validation.

---

# 2. Core Architecture Rules

## 2.1 Static item data

`DT_ItemData` and its associated item-information structures define what an item **type** is.

Examples:

* Item name.
* Description.
* Category.
* Base damage.
* Base armor.
* Base restoration values.
* Mesh/class references.
* Other permanent item-type properties.

Static item data must never be modified to represent one particular inventory instance.

---

## 2.2 Per-instance inventory state

`S_ItemSlot` stores state belonging to the actual inventory instance/stack.

Examples:

* `ItemInstanceID`.
* `ItemID`.
* Quantity.
* `CurrentDurability`.
* `TimesUsed`.
* Other future per-instance state that is genuinely required.

The following are **not** part of `S_ItemSlot`:

* Fusion state.
* Infusion state.
* Cooking overrides.
* Build-source state.
* Battery state.

---

## 2.3 Item type vs item instance

These meanings must never be mixed.

### `ItemID`

Identifies the **item type/static definition**.

Example:

```text
ItemID = Sword
```

All Swords use the same static item definition.

### `ItemInstanceID`

Identifies the **specific inventory instance/stack**.

Example:

```text
ItemInstanceID = 8F...
ItemID = Sword
Quantity = 1
```

Another Sword can have the same `ItemID` but a different `ItemInstanceID`.

---

## 2.4 Stable item identity

Array indices are positional only.

An index can change when:

* Items are removed.
* Items are sorted.
* Stacks are split.
* Stacks are merged.
* Other inventory mutations occur.

Every operation that means:

> "this exact item"

must use `ItemInstanceID`.

This includes:

* Equip.
* Unequip.
* Drop.
* Consume/use.
* Durability changes.
* Usage tracking.
* Hover/selection state.
* Pending replacement transactions.

Indices may still be used internally and transiently inside a function after resolving an instance ID to its current slot.

---

## 2.5 Item-instance lifecycle rules

### Creation

When an item instance/stack is first created:

* Generate one new valid `FGuid`.
* Store it in the slot.
* Never regenerate it simply because the slot moved.

### Split

When a stack is split:

* The original stack keeps its original `ItemInstanceID`.
* The new stack receives a new `ItemInstanceID`.
* The original stack retains its existing `TimesUsed` unless gameplay explicitly requires otherwise.
* The new stack starts with appropriate new-instance state.

### Merge

When two compatible stacks merge:

* One instance is chosen as the surviving instance.
* The surviving instance keeps its `ItemInstanceID`.
* The absorbed instance is removed completely.
* Do not generate a new ID for the merged stack unless the project explicitly decides that merging represents a new instance.
* `TimesUsed` follows the surviving instance.

Merge/split rules must be deterministic.

---

## 2.6 Equipment identity

`EquippedItemIDs` must store `ItemInstanceID` values.

Example:

```text
Sword A
ItemID = Sword
InstanceID = A

Sword B
ItemID = Sword
InstanceID = B
```

Equipment must be able to say:

```text
Equipped Instance = B
```

rather than merely:

```text
Equipped ItemID = Sword
```

This is required because identical item types can coexist.

---

## 2.7 Inventory authority

`BPC_Inventory` remains the authoritative owner of:

* `ItemSlots`.
* Adding/removing items.
* Stack operations.
* Equipment.
* Capacity.
* Item-instance state.
* Effective item-stat/effect queries.
* Usage tracking.
* Sorting/view generation.
* Inventory-side transaction validation.
* Save/load inventory state.

Other systems request inventory operations through `BPC_Inventory`.

They do not directly edit `ItemSlots`.

---

## 2.8 Player-resource authority

`PlayerStatsComponent` owns:

* Health.
* Temporary health.
* Stamina.
* Temporary stamina.

There is **no Battery resource**.

Other systems call the component.

They do not directly write those values.

---

## 2.9 Save authority

`S_GameData` is the authoritative persistent snapshot.

Every feature that introduces persistent state must specify how that state enters/leaves `S_GameData`.

Transient runtime state must not automatically become save data.

---

## 2.10 Effective-value authority

Where a value can differ between static data and the actual inventory instance, create one centralized effective-value query.

The project should eventually have a consistent family of functions such as:

* `GetEffectiveDamage(ItemInstanceID)`.
* `GetEffectiveDefense(ItemInstanceID)`.
* `GetEffectiveItemEffects(ItemInstanceID)`.

The exact implementation can be Blueprint or C++, but there must not be several independent copies of the same calculation.

Because fusion/infusion has been removed, `GetEffectiveDamage` does **not** apply any fusion or infusion bonus.

---

# 3. Foundational Prerequisites

These are architectural contracts rather than new gameplay features.

## 3.1 Verify/complete `ItemInstanceID` migration

Before implementing durability, sorting, or final save integration:

1. Ensure `ItemInstanceID : FGuid` exists in `S_ItemSlot`.
2. Generate it exactly once when creating a new instance/stack.
3. Ensure `FindSlotByInstanceID` exists in `BPC_Inventory`.
4. Ensure newly loaded legacy slots receive an ID if none exists.
5. Change equipment references to instance IDs.
6. Ensure UI-created/selected slots carry both `ItemID` and `ItemInstanceID`.
7. Audit existing functions for index-based identity assumptions.

---

## 3.2 Establish effective item-effect resolution

Create one shared resolution path:

```text
ItemInstanceID
    ↓
Find S_ItemSlot
    ↓
Read ItemID
    ↓
Get S_ItemInfo from DT_ItemData
    ↓
Apply valid per-instance overrides
    ↓
Return effective item effects
```

At minimum, the effect model can support:

* Health restore.
* Stamina restore.
* Temporary health.
* Temporary stamina.
* Future status/buff effects.

There must be one authoritative resolution path.

No fusion/infusion calculation is performed.

No cooking calculation is performed.

---

## 3.3 Runtime transaction rule

Inventory-changing operations must have a clear commit point.

Examples:

```text
Use:
validate → resolve effects → apply → consume → register use → commit

Swap:
validate → reserve → add new → commit old removal → drop old
```

Other systems must not observe a half-completed inventory transaction.

---

# 4. Dependency Order

The implementation order is:

0. **Foundational cleanup / integration gates**
1. **Part A — Player Stats**
2. **Part B — Generic Quick Select**
3. **Part C — Icon Tabs**
4. **Part D — Container Popup**
5. **Part E — Eating / Item Use**
6. **Part F — Wardrobe Hover**
7. **Part G — Capacity Enforcement**
8. **Part H — Full Inventory Swap**
9. **Part I — Save/Load Expansion**
10. **Part J — Durability / Weapon Breaking**
11. **Part K — Sorting + Synchronization**

The following previous systems have been permanently removed from the roadmap:

* Building.
* Ultrahand.
* Ancient Relic construction.
* Cooking.
* Battery.
* Weapon fusion.
* Weapon infusion.
* Arrow fusion.
* Arrow infusion.
* Context-sensitive quick select for fusion/building.

Sorting remains last.

---

# 4.5. Current-Project Integration Gates

These should be completed before the affected systems are treated as production-ready.

## Gate 1 — Container save representation

`S_ContainerData` must not store live runtime `BPC_Inventory` references as persistent save state.

Persistent save data should represent:

* Stable container identity.
* Persistent container contents/state.

The runtime container actor/component resolves its runtime inventory from that persistent state after loading.

---

## Gate 2 — Complete identity audit

Array indices may still be used as temporary positional UI or lookup values.

They must not be used as the identity of an item being mutated.

Audit:

* Add.
* Remove.
* Drop.
* Equip.
* Unequip.
* Use.
* Transfer.
* Move.
* Durability damage.
* Sorting.
* Replacement/swap.
* Save/load.
* Quick-select selection.

Preferred pattern:

```text
UI/temporary index
    ↓
resolve current ItemInstanceID
    ↓
authoritative BPC_Inventory operation
```

---

## Gate 3 — GameMode/template cleanup

The current project still contains stale Third Person template paths/redirectors in configuration.

Update authoritative GameMode references to the current Rihla GameMode path.

Then remove obsolete redirectors/references only after project-wide reference checking.

Clean stale template naming where appropriate.

This is cleanup/integration work, not a gameplay rewrite.

---

## Gate 4 — Effective-item resolution

Create the shared effective-value/effect API before durability, item use, and sorting are finalized.

At minimum:

* `GetEffectiveDamage(ItemInstanceID)`.
* `GetEffectiveDefense(ItemInstanceID)`.
* `GetEffectiveItemEffects(ItemInstanceID)`.

No system should independently calculate these values.

---

# Part A — Player Stats

## Goal

Make `PlayerStatsComponent` the one authoritative source for player health and stamina.

## A1. Existing component

Do not create another stats component.

Use:

`PlayerStatsComponent`

attached to the player Character.

---

## A2. Units

### Health

* `1 Health unit = 1 quarter-heart`.
* `4 Health units = 1 full heart`.
* Example: `MaxHealth = 12` means 3 hearts.

### Stamina

* `1.0 Stamina unit = 1 full stamina-wheel segment`.
* Example: `MaxStamina = 3.0` means three full stamina segments.

There is no Battery system.

---

## A3. Existing API

The delivered C++ provides:

* `RestoreHealth`.
* `RestoreStamina`.
* `AddTemporaryHealth`.
* `AddTemporaryStamina`.
* `TakeDamage`.
* `DrainStamina`.
* Temporary-resource clear functions.
* Save application.
* `InitializeNewGameStats`.
* `SetMaxHealth`.
* `SetMaxStamina`.

Do not create duplicate Blueprint-only versions.

---

## A4. Damage

`TakeDamage`:

1. Consumes temporary health first.
2. Applies remaining damage to normal health.
3. Clamps to zero.
4. Broadcasts changes.
5. Marks the player dead when health is depleted.

Death animations, movement restrictions, camera behavior, respawn, and gameplay mode remain Character/gameplay responsibilities.

---

## A5. Temporary-resource policy

The component owns the resource values and mutation API.

Gameplay systems must define:

* Stack vs replace.
* Expiration.
* Removal.
* Death behavior.
* Restoration behavior.

Do not let individual item/effect systems invent conflicting rules.

---

## A6. Access

Use:

```text
Widget / Gameplay system
        ↓
Character
        ↓
PlayerStatsComponent
```

Avoid repeated global world searches.

---

## A7. Progression

Use:

* `SetMaxHealth`.
* `SetMaxStamina`.

Do not let arbitrary systems directly assign max values.

---

# Part B — Generic Multi-Category Quick Select

## Goal

Turn the existing quick-select system into a reusable category-driven system.

## B1. Widget

In `WBP_QuickSelectMenu` add:

`CurrentCategory : E_ItemCategory`

Properties:

* Instance Editable.
* Expose on Spawn.

Use generic names:

* `Items`.
* `ItemCount`.
* `HorizontalBox_Items`.

---

## B2. Filtering

Inside `InitializeQuickSelect`:

1. Iterate the current inventory/order representation.
2. Resolve each slot's `ItemID`.
3. Read `S_ItemInfo`.
4. Read `ItemCategory`.
5. Compare against `CurrentCategory`.
6. Add matching instances only.

Filtering must be data/category based.

---

## B3. Selection identity

Every displayed quick-select slot stores:

* `ItemID`.
* `ItemInstanceID`.

The selected value passed to inventory is always:

`ItemInstanceID`.

---

## B4. Quick-select component

Use:

`OpenQuickSelect`

with:

`Category : E_ItemCategory`

Pass Category into `WBP_QuickSelectMenu.CurrentCategory`.

---

## B5. Input Actions

The exact final quick-select mappings should correspond to the categories you want readily accessible.

Initial intended mappings:

* Melee → Weapons.
* Defensive → Shields.
* Attachment/utility → Materials.

These are category shortcuts only.

There is no fusion/infusion mode.

There is no build mode.

There is no arrow-fusion mode.

---

## B6. Empty category

Before opening:

1. Query whether the requested category contains at least one valid selectable instance.
2. If zero:

   * Do not create the widget.
   * Do not pause.
   * Do not change selection.

Centralize this check.

---

## B7. Equip/use

Flow:

```text
Input
→ BPC_QuickSelect
→ WBP_QuickSelectMenu
→ ItemInstanceID
→ BPC_Inventory operation
```

Weapons/bows/shields use the existing equipment flow.

Consumable/usable categories use the authoritative inventory use flow.

Do not create another equipment implementation.

---

# Part C — Icon-Based Inventory Tabs

## Goal

Replace text-only category filters with category icons.

## C1. Icons

Create/source:

1. Weapons.
2. Bows.
3. Shields.
4. Armor.
5. Materials.
6. Meals/Consumables.
7. Utility/Miscellaneous if required.
8. Key Items.

The exact category count should match the final `E_ItemCategory` design.

## C2. `WBP_ItemFilter`

Add:

`CategoryIcon : Texture2D`

and an `Image` widget.

## C3. Setup

Set the icon from `CategoryIcon`.

Text may remain as:

* Tooltip.
* Fallback.
* Accessibility label.

## C4. Instances

Assign icons to the inventory filter instances.

## C5. Validation

Verify:

* Correct icon.
* Correct category.
* Existing filtering unaffected.
* Null icon does not break layout.

---

# Part D — Container Popup

## Goal

Make ordinary container interaction feel like a TotK-style item discovery/pickup rather than opening the full inventory.

## D1. Normal pickup

`BP_Container_Base` should not open `WBP_ContainerUI` for an ordinary pickup.

## D2. Popup

Use `WBP_ContainerDisplay` or a dedicated popup containing:

* Item icon.
* Name.
* Short description.
* Pickup/confirmation presentation.
* Animation.

## D3. Normal vs full

### Normal

```text
Container
→ Inventory Add
→ Pickup Popup
```

### Full

```text
Container
→ Inventory Full result
→ Full Inventory Popup
→ Player chooses replacement
```

## D4. Transaction

Do not permanently remove the container item until inventory insertion succeeds.

If insertion fails, leave the container item unchanged.

## D5. Legacy widget

Only delete `WBP_ContainerUI` after checking every reference.

---

# Part E — Eating / Item Use

## Goal

Make usable food/items affect the player through `PlayerStatsComponent`.

Cooking is not part of the system.

Food items come from normal item definitions and inventory entries.

## E1. Identity

`UseItem` should accept:

`ItemInstanceID`

as the identity of the item being consumed.

## E2. Effective effects

Resolve:

* Health restore.
* Stamina restore.
* Temporary health.
* Temporary stamina.

For each value:

```text
active slot override
    if override >= 0
else
static S_ItemInfo value
```

If the final implementation does not require per-instance overrides, use the static item values directly through the centralized effect resolver.

## E3. Apply

Call:

* `RestoreHealth`.
* `RestoreStamina`.
* `AddTemporaryHealth`.
* `AddTemporaryStamina`.

## E4. Transaction

The operation should be:

```text
Validate instance
→ Validate item/use type
→ Resolve effects
→ Validate required references
→ Apply effects
→ Consume intended quantity
→ RegisterItemUsed(ItemInstanceID)
→ Refresh UI
```

If validation fails, consume nothing.

## E5. Invalid cases

Invalid/non-usable/missing instances must fail cleanly with no mutation.

---

# Part F — Wardrobe Hover Information

## Goal

Display information for the exact inventory instance being hovered.

## F1. Slot identity

`WBP_InventorySlot` carries:

* `ItemID`.
* `ItemInstanceID`.

## F2. Hover events

Use:

* `OnMouseEnter`.
* `OnMouseLeave`.

Reuse `WBP_ItemSlotPreview` patterns where appropriate.

## F3. Hover data

Flow:

```text
ItemInstanceID
→ resolve slot
→ read ItemID
→ GetItemData
→ resolve effective instance data
→ populate panel
```

Display:

* Name.
* Description.
* Category.
* Effective damage.
* Defense.
* Durability where appropriate.
* Other relevant category-specific data.

Do not display fusion/infusion information.

Do not display cooking effects.

Do not display battery information.

## F4. Hover race safety

`WBP_WardrobeUI` tracks the currently hovered `ItemInstanceID`.

A slot's `OnMouseLeave` clears the panel only if that instance is still the active hover.

## F5. Player stats

Use `PlayerStatsComponent` and its delegates for:

* Health.
* Stamina.

Refresh once when the widget becomes visible and react to later changes.

---

# Part G — Capacity Enforcement

## Goal

Enforce weapon/bow/shield capacity through one authoritative inventory-add path.

## G1. Central gate

All inventory entry points must use the same add logic:

* Ground.
* Containers.
* Rewards.
* Future systems.

## G2. Capacity semantics

Weapon/bow/shield capacity is occupied **inventory-instance/slot capacity**, not raw quantity.

Stackable item quantity/stack size is a separate concept.

## G3. Check order

Before mutation:

1. Resolve item type/category.
2. Check whether the category is capacity limited.
3. Attempt valid stack merging if applicable.
4. If no merge is possible, count occupied instances.
5. Compare against capacity.
6. Return a structured result.

Possible results:

* Success.
* Invalid item.
* Invalid data/class.
* Category full.
* Other failure.

## G4. Capacity ownership

UI never independently decides whether the category is full.

## G5. Upgrades

Create:

`UpgradeCategoryCapacity(Category)`

It:

* Increases current capacity.
* Clamps to maximum.
* Rejects upgrades at maximum.

Future NPC/currency systems call this function.

---

# Part H — Full Inventory Swap

## Goal

Safely replace one item when a limited category is full.

## H1. Popup

Display:

> Inventory full — drop [existing item] to make room?

Show:

* Existing item.
* Incoming item.
* Confirm.
* Cancel.

## H2. Player selection

Initially the player explicitly chooses the item to replace.

## H3. Atomic replacement

Treat replacement as one transaction:

```text
Validate incoming item
→ Validate old ItemInstanceID
→ Reserve old slot internally
→ Prepare/validate incoming instance
→ Commit incoming insertion
→ Commit old removal
→ Spawn/drop old item
```

The old item must not be permanently dropped before the new item has successfully entered inventory.

## H4. Complete item state

The transaction preserves the complete incoming instance state:

* ItemID.
* Quantity.
* Durability.
* ItemInstanceID.
* Other valid per-instance state.

There is no fusion state.

There is no infusion state.

There is no cooking state.

## H5. Failure

If the transaction cannot commit, restore the exact previous inventory state.

## H6. Cancel

Cancel changes nothing.

---

# Part I — Save / Load Expansion

## Goal

Make `S_GameData` the single authoritative persistent snapshot.

## I1. Save version

Add:

`SaveVersion`

to save data.

New versions must have deliberate migration behavior for older saves.

## I2. Inventory state

Ensure save/load preserves:

* ItemInstanceID.
* ItemID.
* Quantity.
* CurrentDurability.
* TimesUsed.
* Other persistent per-instance fields.

Do not maintain a stripped-down duplicate representation.

Do not serialize fusion/infusion data.

Do not serialize cooking overrides.

## I3. Equipment

Save/load `EquippedItemIDs` as instance IDs.

## I4. Capacity/progression

Persist purchased capacity upgrades.

## I5. Player resources

Define exactly which of these persist:

* Current health.
* Max health.
* Temporary health.
* Current stamina.
* Max stamina.
* Temporary stamina.

Battery is not saved because Battery does not exist in Rihla.

## I6. Build-world persistence

Not applicable.

Rihla has no planned persistent building system.

## I7. Save authority

One authoritative system constructs `S_GameData`.

Individual systems contribute their state; they do not create competing save objects.

## I8. Validation

Test:

1. Equip.
2. Damage durability.
3. Use items.
4. Upgrade capacity.
5. Change player stats.
6. Save.
7. Reload.
8. Verify all intended persistent state.

Also verify:

* ItemInstanceIDs remain valid.
* Equipment references still resolve.
* No duplicates appear.
* No item disappears.

---

# Part J — Durability and Weapon Breaking

## Goal

Make durability authoritative and instance-based.

## J1. Primary API

Create:

`DamageItem(ItemInstanceID, Amount)`

This is the authoritative durability mutation.

Optional wrapper:

`DamageEquippedItem(EquipmentCategory, Amount)`

resolves the equipped instance and calls `DamageItem`.

## J2. Durability

`DamageItem`:

1. Resolves the instance.
2. Validates it.
3. Subtracts durability.
4. Clamps to zero.
5. Detects break.
6. Refreshes/broadcasts state.

Combat never directly edits `CurrentDurability`.

## J3. Break transaction

On break:

1. Resolve exact instance.
2. Unequip it if equipped.
3. Remove it.
4. Clear equipment reference.
5. Invalidate stale quick-select/selection references.
6. Trigger VFX/SFX.
7. Refresh UI.

## J4. Combat integration

Melee calls `DamageItem` on the actual durability-consuming event.

Bow combat defines its own bow durability rules if bows use durability.

There is no arrow fusion or arrow infusion system.

---

# Part K — Sorting and Synchronization

> **This remains last by design.**

## K1. Usage

Add:

`TimesUsed : Integer`

to `S_ItemSlot`.

Create:

`RegisterItemUsed(ItemInstanceID)`

Only this function increments the counter.

## K2. Definition of use

Count successful gameplay actions.

Examples:

* Equip.
* Eat/consume.
* Other explicitly defined use actions.

Do not count:

* Hover.
* Preview.
* Menu opening.
* Highlight.
* Selecting without committing an action.

## K3. Sort mode

Create:

`E_SortMode`

Values:

* Category.
* MostUsed.
* Damage.
* Armor.
* Food.

`CurrentSortMode` may live in `BPC_Inventory` as shared runtime view state, but it is **not persistent inventory state** unless deliberately saved later.

## K4. Storage vs display

`ItemSlots` remains authoritative storage order.

Never reorder it merely to change presentation.

Create:

`SortedItemInstances : TArray<FGuid>`

as a derived display-order cache.

It is:

* Rebuildable.
* Non-authoritative.
* Not persistent.
* Not an alternative inventory database.

## K5. Sorting

`SortItems`:

1. Reads authoritative `ItemSlots`.
2. Resolves effective values.
3. Applies selected primary criterion.
4. Applies deterministic secondary criteria.
5. Produces only ordered `ItemInstanceID` values.

## K6. Deterministic tie-breaking

Use:

1. Primary criterion.
2. Category priority when required.
3. Stable `ItemInstanceID`.

Never use an array index as a persistent tie-breaker.

## K7. Category

Use an explicitly defined stable `E_ItemCategory` priority.

Do not depend on:

* Localized names.
* Alphabetic text.
* DataTable row order.

## K8. Damage

Use:

`GetEffectiveDamage(ItemInstanceID)`.

There are no fusion or infusion bonuses.

## K9. Armor

Use actual effective defense/armor.

Non-armor items need deterministic placement.

## K10. Food

Use a clearly defined food/healing/effect ranking.

Cooked meal overrides do not exist.

## K11. UI refresh

When sort changes:

1. Update `CurrentSortMode`.
2. Rebuild `SortedItemInstances`.
3. Refresh main inventory from instance IDs.
4. Refresh Quick Select from the same ordered IDs.
5. Resolve current data by `ItemInstanceID`.
6. Preserve equipment highlighting.
7. Preserve current selection by `ItemInstanceID`.

## K12. Quick-select filtering

The flow is:

```text
Authoritative ItemSlots
        ↓
SortedItemInstances
        ↓
category filter
        ↓
Quick Select
```

The Quick Select widget does not maintain an independent inventory database.

---

# 5. Cross-System Contracts

## Inventory

`BPC_Inventory` owns:

* Inventory state.
* Instance identity.
* Stack operations.
* Equipment.
* Capacity.
* Durability.
* Usage counters.
* Effective item queries.
* Inventory transactions.
* Sort/display ordering.

It does **not** own:

* Player health.
* Player stamina.
* UI.
* Building.
* Cooking.
* Fusion.
* Infusion.
* Battery.

---

## Player resources

`PlayerStatsComponent` owns:

* Health.
* Temporary health.
* Stamina.
* Temporary stamina.

There is no Battery system.

---

## Quick Select

`BPC_QuickSelect` owns:

* Opening/closing.
* Category resolution.
* Selection flow.

It does not own inventory state.

There is no fusion/build context system.

---

## Quick-select widget

`WBP_QuickSelectMenu` owns:

* Display.
* Category filtering presentation.
* Selection interaction.

It does not become an inventory database.

---

## Static item database

`DT_ItemData` owns static item definitions.

It is never modified to represent per-instance state.

---

## Save system

`S_GameData` owns persistent snapshot data.

---

# 6. Critical Validation Rules

## Inventory

Test:

* Add.
* Remove.
* Drop.
* Equip/unequip.
* Two identical item types.
* Stable ItemInstanceID.
* Stack split.
* Stack merge.
* Full category.
* Empty category.
* Failed transaction.
* Invalid ItemID/data/class.

## Quick Select

Test:

* Zero items.
* One item.
* Many items.
* Correct category.
* Correct ItemInstanceID.
* Equip.
* Use.
* Close/reopen.
* Empty categories do not open.
* Sorting preserves selection by instance.

## Containers

Test:

* Normal pickup.
* Full category.
* Replacement choice.
* Confirm.
* Cancel.
* Add failure.
* Incoming item remains if replacement fails.
* Old item never disappears early.

## Food / Item Use

Test:

* Health restoration.
* Stamina restoration.
* Temporary health.
* Temporary stamina.
* Invalid use.
* Failed use consumes nothing.
* Successful use consumes exactly the intended quantity/instance.

## Durability

Test:

* Valid durability damage.
* Invalid ItemInstanceID.
* Durability clamping.
* Weapon break.
* Equipped weapon break.
* Equipment reference cleanup.
* Quick-select reference cleanup.
* UI refresh.
* Save/load durability.

## Player Stats

Test:

* Health clamping.
* Temporary-health absorption.
* Death.
* Revive.
* Stamina drain.
* Temporary stamina.
* Max-health changes.
* Max-stamina changes.
* Save/reload.

## Save

Test:

* Equipment.
* Durability.
* Capacity upgrades.
* Player stats according to defined save rules.
* ItemInstanceIDs.
* Save-version migration.
* No duplicates.
* No stale equipment references.
* No disappearing items.

---

# 7. Things That Must NOT Be Done

Do not:

* Store runtime-generated item values in `DT_ItemData`.
* Use `ItemID` as the identity of a specific inventory instance.
* Use array index as persistent item identity.
* Let UI directly mutate `ItemSlots`.
* Let Quick Select maintain a second inventory database.
* Let combat directly edit durability.
* Let multiple systems increment `TimesUsed`.
* Let individual UIs implement independent sorting.
* Enforce capacity only in container UI.
* Drop the old replacement item before the new one is committed.
* Consume an item before its use transaction can commit safely.
* Save `SortedItemInstances` as authoritative inventory data.
* Save `CurrentSortMode` unless intentionally chosen as player preference.
* Modify base item damage at runtime.
* Add fusion or infusion fields to item instances.
* Create a permanent fusion/infusion system for weapons.
* Create an arrow fusion/infusion system.
* Create a cooking system.
* Create a Battery resource.
* Create a building/Ultrahand system.
* Create Ancient Relic inventory/build transactions.
* Add a second player-resource component.
* Allow inventory UI to become authoritative gameplay state.

---

# 8. Permanently Removed Systems

The following systems are intentionally outside the scope of Rihla's current design.

## Building / Ultrahand

Removed entirely.

Do not implement:

* `BuildComponent`.
* Build gizmos.
* Buildable actors.
* Build input actions.
* Snap systems.
* Build-source transactions.
* Ancient Relic construction.
* Persistent structures.
* Build-world save data.

If an existing `BuildComponent` is still present in the project, it is no longer part of the planned final architecture and can be removed after checking project references.

---

## Cooking

Removed entirely.

Do not implement:

* `BP_CookingPot`.
* `DT_Recipes`.
* Cooking ingredient selection.
* Cooking transactions.
* Generic cooking rules.
* Cooked-item overrides.
* Generated meal effects.

Food/meals can still exist as normal item types and can still be consumed through `UseItem`.

---

## Battery

Removed entirely.

Do not implement:

* Battery UI.
* Battery resource.
* Battery drain.
* Battery restoration.
* Battery upgrades.
* Battery save data.

`PlayerStatsComponent` contains only health and stamina resource systems.

---

## Fusion / Infusion

Removed entirely.

Do not implement:

* `FusedMaterialID`.
* Fusion bonuses.
* Fusion transactions.
* Unfusing.
* Weapon infusion.
* Material attachment.
* Permanent weapon modification.
* Fusion-based effective damage calculations.
* Fusion UI.
* Fusion save data.

`GetEffectiveDamage(ItemInstanceID)` resolves the item's actual damage from its valid item/instance data without fusion or infusion.

---

## Arrow Fusion / Infusion

Removed entirely.

Do not implement:

* Primed arrow materials.
* Arrow material selection.
* Arrow fusion.
* Arrow infusion.
* Temporary arrow-material state.
* Arrow material consumption.
* Arrow fusion UI.

Bows and arrows remain normal gameplay/inventory systems.

---

# 9. Definition of Done

The remaining system is structurally complete when:

* Player resources have one authoritative component.
* Player resources consist of health and stamina only.
* Inventory has one authoritative instance-state owner.
* Every inventory instance has stable identity.
* Static and per-instance data are cleanly separated.
* Equipment references exact item instances.
* Item use is instance-based and transactional.
* Quick Select is category-driven and instance-aware.
* Empty categories do not open.
* Container pickup and full-inventory replacement are transactional.
* Capacity is enforced centrally.
* Eating/item use uses effective item effects.
* Effective damage is centralized.
* Durability changes only occur because of actual gameplay events.
* Weapon breaking correctly removes the exact instance.
* Save/load persists all intended persistent state.
* Save versions can be migrated safely.
* Inventory and Quick Select share one derived ordered representation.
* Sorting never reorders authoritative inventory storage.
* No system creates a conflicting second source of truth.
* No building system exists.
* No cooking system exists.
* No Battery system exists.
* No fusion system exists.
* No infusion system exists.
* No arrow fusion/infusion system exists.

---

# 10. Current Audit Snapshot

| Area                           | Status | Notes                                                                                       |
| ------------------------------ | ------ | ------------------------------------------------------------------------------------------- |
| ItemInstanceID data model      | 🟢     | Present in `S_ItemSlot`.                                                                    |
| Instance-based equipment       | 🟢     | Current equipment flow contains instance-ID references.                                     |
| PlayerStatsComponent           | 🟢     | Native component exists and is attached to Character.                                       |
| Health                         | 🟢     | Native resource foundation exists.                                                          |
| Stamina                        | 🟢     | Native resource foundation exists.                                                          |
| Battery                        | ⚪      | Removed from final design.                                                                  |
| BuildComponent                 | ⚪      | Building removed from final design.                                                         |
| Build input assets             | ⚪      | No longer required.                                                                         |
| Build snap collision           | ⚪      | No longer required.                                                                         |
| Container save representation  | 🔴     | Runtime inventory-component references must be removed from persistent save representation. |
| Effective item resolution      | 🟡     | Per-instance foundation exists; centralized resolver still needs implementation.            |
| Durability mutation API        | 🟡     | Data exists; authoritative `DamageItem(ItemInstanceID, Amount)` still needs implementation. |
| Fusion                         | ⚪      | Removed from final design.                                                                  |
| Infusion                       | ⚪      | Removed from final design.                                                                  |
| Arrow fusion                   | ⚪      | Removed from final design.                                                                  |
| Cooking                        | ⚪      | Removed from final design.                                                                  |
| Save/load expansion            | 🟡     | Core instance-aware data exists; complete persistence validation remains.                   |
| Context-sensitive Quick Select | ⚪      | Fusion/build-specific context behavior removed.                                             |
| Generic Quick Select           | 🟢/🟡  | Existing category system remains and needs final cleanup/integration.                       |
| Sorting/synchronization        | ⏸️     | Correctly deferred to final stage.                                                          |
| Legacy template cleanup        | 🟡     | Old GameMode/template references and redirector paths remain.                               |

The project does **not** need a full inventory-system rewrite.

The correct strategy is to keep the current inventory architecture, finish the identity/effective-value contracts, clean up the remaining integration gates, and then implement the remaining features in dependency order.

---

# 11. Final Feature Set

The final Rihla inventory/gameplay foundation consists of:

### Inventory

* Instance-based inventory.
* Stable `ItemInstanceID`.
* Stack management.
* Stack splitting.
* Stack merging.
* Item dropping.
* Item transfer.
* Equipment.
* Equipment highlighting.
* Capacity limits.
* Capacity upgrades.
* Full-inventory replacement.
* Inventory sorting.

### Equipment

* Weapons.
* Bows.
* Shields.
* Armor.
* Instance-based equipment references.
* Durability.
* Weapon breaking.

### Quick Select

* Three quick-select menus.
* Category-driven filtering.
* Dynamic slot counts.
* Instance-aware selection.
* Empty-category protection.
* Equipment/use integration.
* Selection preservation through sorting.

### Player Resources

* Health.
* Temporary health.
* Stamina.
* Temporary stamina.
* Damage.
* Healing.
* Death.
* Resource upgrades.

### Items

* Static item definitions.
* Per-instance state.
* Item usage tracking.
* Effective damage.
* Effective defense.
* Effective item effects.
* Consumable/food usage.

### Inventory UI

* Icon-based tabs.
* Item hover information.
* Equipment highlighting.
* Player health/stamina display.
* Quick-select UI.
* Inventory-full replacement UI.
* Container pickup popup.
* Pause-on-open.

### Containers

* Normal item pickup.
* Pickup popup.
* Capacity checking.
* Inventory-full replacement.
* Transaction-safe pickup.

### Durability

* Instance-based durability.
* Central durability damage API.
* Weapon breaking.
* Equipment cleanup.
* Quick-select cleanup.

### Save/Load

* Instance-aware inventory saving.
* Equipment saving.
* Durability saving.
* Usage tracking saving.
* Capacity progression saving.
* Player resource saving according to defined rules.
* Save versions and migration.
* No stale runtime object references.

---

# 12. Final Implementation Principle

Rihla should grow by adding systems around the existing architecture, not by bypassing it.

The intended data flow is:

```text
DT_ItemData / S_ItemInfo
        ↓
defines item type
        ↓
S_ItemSlot
        ↓
stores specific instance state
        ↓
BPC_Inventory
        ↓
owns and mutates instance
        ↓
Effective Item Queries
        ↓
UI / Quick Select / Food / Combat
```

Player resources:

```text
PlayerStatsComponent
        ↓
Health / Stamina
```

Persistence:

```text
Runtime authoritative systems
        ↓
S_GameData
        ↓
SaveVersion + persistent state
```

Presentation:

```text
Authoritative runtime state
        ↓
Inventory / Quick Select / Wardrobe UI
```

The key rules remain:

> **ItemID answers "what is it?"**
> **ItemInstanceID answers "which exact one is it?"**
> **BPC_Inventory answers "what is in the inventory?"**
> **PlayerStatsComponent answers "what resources does the player have?"**
> **S_GameData answers "what persistent state should survive a reload?"**

Rihla's final architecture should remain deliberately focused: a robust instance-based inventory and equipment system with quick-select, player health/stamina, item use, capacity, durability, sorting, and reliable persistence—without introducing building, cooking, battery, fusion, or infusion systems that would create unnecessary architectural complexity.
