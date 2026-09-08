# Rihla — Remaining Work (Canonical v6)

> **Engine:** Unreal Engine 5.7.4
> **Project:** Rihla
> **Design target:** A TotK-inspired exploration/action game, but **not a TotK clone**.
> **Core identity:** Exploration, combat, traversal, equipment progression, cooking, quests, materials, and meaningful world interaction.

---

# 1. Project Vision

Rihla is built around the following gameplay loop:

```text
EXPLORE
   ↓
Discover locations
Find materials
Find equipment
Meet NPCs
Discover quests
   ↓
FIGHT
   ↓
Collect rewards
   ↓
COOK / USE ITEMS
   ↓
UPGRADE EQUIPMENT
   ↓
UNLOCK / IMPROVE ABILITIES
   ↓
Reach new areas
   ↓
Explore farther
```

The game should feel inspired by modern open-world action-adventure games while developing its **own systems and identity**.

Rihla does **not** use:

* Building / Ultrahand
* Battery
* Weapon fusion
* Weapon infusion
* Arrow fusion
* Arrow infusion
* Permanent item fusion
* Temporary item fusion
* Cooking-generated fusion states

Instead, progression comes from:

* Equipment upgrades
* Materials
* Currency
* Quests
* Cooking
* Player abilities
* Exploration
* Combat mastery
* World interaction

---

# 2. System Ownership

| System                      | Authority                                   |
| --------------------------- | ------------------------------------------- |
| `DT_ItemData`               | Static item definitions                     |
| `S_ItemInfo`                | Static item definition data                 |
| `S_ItemSlot`                | Individual inventory instance/stack state   |
| `BPC_Inventory`             | Inventory state and inventory mutations     |
| `PlayerStatsComponent`      | Health and stamina                          |
| Currency system             | Player currency                             |
| Equipment Upgrade System    | Equipment progression/upgrades              |
| Cooking System              | Recipes, cooking and cooked-item generation |
| Quest System                | Quest state, objectives and rewards         |
| `WorldInteractionComponent` | Generic world interaction capabilities      |
| Grapple system              | Grapple traversal                           |
| Combat system               | Parry, dodge, attacks and combat behavior   |
| `BPC_QuickSelect`           | Quick-select state/presentation             |
| `WBP_QuickSelectMenu`       | Quick-select UI                             |
| Inventory widgets           | Display and interaction requests            |
| `S_GameData`                | Persistent save snapshot                    |
| Static database             | Static item definitions only                |

---

# 3. Non-Negotiable Architecture Rules

## 3.1 Static vs Instance Data

Never modify static `DT_ItemData` data at runtime to represent an individual item's state.

Static data describes:

```text
Item Type
```

Instance data describes:

```text
This specific item/stack
```

---

## 3.2 Stable Item Identity

Every unique inventory instance/stack must have:

```text
ItemInstanceID : FGuid
```

`ItemID` identifies the item type.

`ItemInstanceID` identifies the exact inventory instance.

Never use array index as persistent identity.

---

## 3.3 Array Indices

`ItemSlots` array indices are positional only.

Never save or reference:

```text
Inventory Slot Index = Item Identity
```

Sorting, moving, splitting, merging and UI rebuilding must never break item identity.

---

## 3.4 UI Authority

UI requests gameplay operations.

UI must NOT directly mutate:

* `ItemSlots`
* Currency
* Health
* Stamina
* Durability
* Equipment state
* Upgrade levels
* Quest state

Gameplay systems perform the mutation and then notify/refresh UI.

---

## 3.5 Transactional Mutations

Important gameplay operations must follow:

```text
Validate
↓
Prepare
↓
Commit
↓
Refresh
```

Never destroy/remove the original state before the replacement/new state has successfully committed.

---

# 4. Final `S_ItemSlot` Design

`S_ItemSlot` represents an inventory instance/stack.

Required persistent fields:

```text
ItemInstanceID : FGuid
ItemID
ItemQuantity
CurrentDurability
TimesUsed
```

Equipment-specific progression may also require:

```text
UpgradeLevel
```

or another dedicated equipment progression representation.

Additional fields may be added only when they represent **real per-instance state**.

## Explicitly forbidden:

```text
Fusion state
Infusion state
Cooking fusion state
Build-source state
Battery state
Ultrahand state
```

Cooked food should be represented as a legitimate item/instance with its own valid item data and effects rather than creating a fusion system.

---

# 5. Item Identity Rules

## `ItemID`

Represents:

> What kind of item is this?

Examples:

```text
Iron Sword
Apple
Wooden Shield
Healing Herb
```

## `ItemInstanceID`

Represents:

> Which exact physical inventory instance is this?

Example:

```text
Iron Sword
ItemID = Sword_Iron
ItemInstanceID = {GUID}
```

If two identical swords exist:

```text
Sword A
ItemID = Sword_Iron
ItemInstanceID = GUID_A

Sword B
ItemID = Sword_Iron
ItemInstanceID = GUID_B
```

They are separate instances.

---

# 6. Split / Merge Rules

## Split

Original stack retains its ID.

New stack receives a new ID.

```text
Original:
GUID_A
Quantity 10

Split 4:

Original:
GUID_A
Quantity 6

New:
GUID_B
Quantity 4
```

## Merge

One instance survives.

The absorbed instance ID is destroyed.

The result must be deterministic.

Never create duplicate authoritative instances.

---

# 7. BPC_Inventory Responsibilities

`BPC_Inventory` owns:

* `ItemSlots`
* Add item
* Remove item
* Stack management
* Split
* Merge
* Drop
* Transfer
* Equipment
* Unequipment
* Equipment references
* Inventory capacity
* Capacity upgrades
* Per-instance state
* Durability
* Usage tracking
* Effective item values
* Item use transactions
* Sorting source data
* Inventory-side validation
* Save/load inventory state

The inventory remains the **single authoritative owner of inventory instances**.

---

# 8. Equipment

Supported equipment:

* Weapons
* Bows
* Shields
* Armor

Equipped equipment must be referenced using:

```text
ItemInstanceID
```

Never `ItemID` alone.

Example:

```text
EquippedWeaponInstanceID
EquippedBowInstanceID
EquippedShieldInstanceID
```

or the existing `EquippedItemIDs` structure, provided it stores `ItemInstanceID` values.

---

# 9. Equipment Progression & Upgrades

Equipment progression replaces the role that fusion/building might otherwise have played in progression.

Equipment can have:

```text
UpgradeLevel
```

Example:

```text
Traveler's Sword
Upgrade ★★
Damage: 18
Durability: 42/50
```

Upgrades can modify:

* Damage
* Defense
* Durability
* Attack speed
* Draw speed
* Other explicitly supported equipment stats

The exact stats depend on equipment category.

---

# 10. Equipment Upgrade System

Create a centralized equipment upgrade system.

Suggested responsibility:

```text
Equipment Upgrade System
```

It should be able to:

```text
CanUpgradeEquipment(ItemInstanceID)
GetUpgradeRequirements(ItemInstanceID)
GetUpgradeResult(ItemInstanceID)
TryUpgradeEquipment(ItemInstanceID)
```

Upgrade flow:

```text
Validate instance
↓
Validate equipment
↓
Check upgrade level
↓
Check materials
↓
Check currency
↓
Reserve/consume requirements
↓
Increase upgrade state
↓
Refresh effective item values
↓
Refresh UI
```

Failed upgrades must consume nothing.

---

# 11. Materials

Materials are no longer used for fusion.

Materials can instead be used for:

* Equipment upgrades
* Cooking
* Quests
* Selling
* NPC requests
* Exploration rewards
* Other explicitly defined progression systems

Example:

```text
Iron Ore ×5
Monster Fang ×3
250 Currency
↓
Upgrade Traveler's Sword
```

This gives the Materials category a meaningful purpose without fusion.

---

# 12. Effective Item Values

All systems must use centralized effective-value queries.

Required APIs:

```text
GetEffectiveDamage(ItemInstanceID)
GetEffectiveDefense(ItemInstanceID)
GetEffectiveItemEffects(ItemInstanceID)
GetEffectiveDurability(ItemInstanceID)
```

These functions must account for legitimate progression such as:

```text
Base Item Data
+
Equipment Upgrade State
+
Other explicitly supported permanent item state
```

There is:

```text
NO FUSION
NO INFUSION
```

Damage must never be independently recalculated by different systems.

---

# 13. Player Stats

`PlayerStatsComponent` owns:

## Health

```text
CurrentHealth
MaxHealth
TemporaryHealth
```

Health units:

```text
1 = quarter-heart
4 = full heart
```

Therefore:

```text
MaxHealth = 12
```

means:

```text
3 hearts
```

## Stamina

```text
CurrentStamina
MaxStamina
TemporaryStamina
```

Stamina units:

```text
1.0 = one full stamina-wheel segment
```

Example:

```text
MaxStamina = 3.0
```

means:

```text
3 stamina segments
```

There is **NO Battery system**.

---

# 14. PlayerStats APIs

Existing/required APIs:

```text
RestoreHealth()
RestoreStamina()

AddTemporaryHealth()
AddTemporaryStamina()

TakeDamage()
DrainStamina()

ClearTemporaryHealth()
ClearTemporaryStamina()

ApplySavedStats()
InitializeNewGameStats()

SetMaxHealth()
SetMaxStamina()
```

`TakeDamage()`:

```text
Temporary Health
↓
Normal Health
↓
Clamp
↓
Broadcast
↓
Death when depleted
```

---

# 15. Currency

Rihla has a dedicated player currency system.

Currency is a player resource, not an inventory item.

Required functionality:

```text
GetCurrency()
AddCurrency()
CanAfford()
SpendCurrency()
TrySpendCurrency()
```

Currency can be obtained from:

* Quests
* Enemies
* Chests
* Selling items
* Exploration
* NPC rewards
* Other gameplay rewards

Currency can be spent on:

* Equipment upgrades
* Shops
* Items
* Services
* Quest-related purchases
* Other progression systems

All currency changes must be authoritative and transactional.

---

# 16. Cooking

Cooking is restored as a major gameplay system.

Cooking allows ingredients to become useful consumables.

Possible results:

* Health restoration
* Stamina restoration
* Temporary health
* Temporary stamina
* Temporary buffs
* Other explicitly supported food effects

Cooking should NOT be implemented as item fusion.

---

# 17. Cooking Architecture

Recommended flow:

```text
Player
↓
Cooking Station
↓
Select Ingredients
↓
Validate Ingredients
↓
Determine Recipe/Result
↓
Consume Ingredients
↓
Create Cooked Item
↓
Add Result To Inventory
↓
Refresh UI
```

The cooking system owns:

* Recipes
* Ingredient validation
* Cooking results
* Food effects
* Cooking transactions

The inventory owns the actual resulting item instance.

---

# 18. Cooking Recipes

Recipes can be:

* Discovered naturally
* Learned from NPCs
* Learned through quests
* Found in the world
* Experimented with by the player

Recipe data should remain static where possible.

Cooked item state belongs to the resulting inventory instance.

No fusion state should be added to `S_ItemSlot`.

---

# 19. Generic Quick Select

`BPC_QuickSelect` and `WBP_QuickSelectMenu` are category-driven.

They do not own inventory state.

Current category:

```text
E_ItemCategory
```

Widget instance should expose:

```text
CurrentCategory
```

Dynamic data:

```text
Items
ItemCount
HorizontalBox_Items
```

Filtering:

```text
Inventory ItemSlots
↓
ItemID
↓
Get Item Data
↓
ItemCategory
↓
Category Match
↓
Quick Select
```

Each quick-select slot stores:

```text
ItemID
ItemInstanceID
```

---

# 20. Three Quick Select Menus

Rihla retains three quick-select menus.

Initial category mappings:

```text
Melee
→ Weapons

Defensive
→ Shields

Attachment / Utility
→ Materials or other appropriate utility category
```

These are category shortcuts only.

There is:

```text
NO fusion mode
NO building mode
NO Ultrahand mode
```

---

# 21. Empty Quick Select Protection

If the selected category has no valid items:

```text
Do not create widget
Do not pause game
Do not change selection
Do not enter selection mode
```

Quick-select should fail cleanly.

---

# 22. Quick Select Equipment / Use

Quick select must call the authoritative gameplay system.

For equipment:

```text
Quick Select
↓
BPC_Inventory
↓
Equip Item By Instance ID
```

For consumables:

```text
Quick Select
↓
BPC_Inventory.UseItem(ItemInstanceID)
```

Do not duplicate equipment logic inside the widget.

---

# 23. Icon Inventory Tabs

Inventory tabs should use icons rather than text.

Potential categories:

* Weapons
* Bows
* Shields
* Armor
* Materials
* Meals / Consumables
* Utility / Miscellaneous
* Key Items

The exact category count must match the final `E_ItemCategory`.

`WBP_ItemFilter`:

```text
CategoryIcon : Texture2D
```

and corresponding Image widget.

---

# 24. Container System

Normal pickup:

```text
Container
↓
Attempt Inventory Add
↓
Success
↓
Remove container item
↓
Show Pickup Popup
```

If inventory is full:

```text
Container
↓
Inventory Full
↓
Full Inventory Popup
↓
Player selects replacement
↓
Transactional replacement
↓
Pickup succeeds
```

Never remove the container item before inventory insertion succeeds.

---

# 25. Container Save Representation

Persistent container state must NOT depend on runtime:

```text
BPC_Inventory references
```

Save containers using stable container identity and serialized state.

Example concept:

```text
ContainerID
ContainedItemState
Opened/Collected State
```

Runtime references may be reconstructed after loading.

---

# 26. Eating / Item Use

Required API:

```text
UseItem(ItemInstanceID)
```

Flow:

```text
Validate item
↓
Resolve effective effects
↓
Apply effects
↓
Consume item
↓
Register use
↓
Commit
↓
Refresh UI
```

Possible effects:

```text
Health
Stamina
Temporary Health
Temporary Stamina
Other explicitly supported consumable effects
```

Invalid use consumes nothing.

Effects should be resolved through the shared effective-value system.

---

# 27. Wardrobe Hover

Wardrobe hover must carry:

```text
ItemID
ItemInstanceID
```

Flow:

```text
ItemInstanceID
↓
Resolve ItemSlot
↓
ItemID
↓
GetItemData
↓
Resolve Effective Values
↓
Populate Wardrobe UI
```

Display:

* Name
* Description
* Category
* Damage
* Defense
* Durability
* Upgrade level
* Relevant item effects

No fusion/infusion information.

No battery information.

---

# 28. Inventory Capacity

Capacity is authoritative inside `BPC_Inventory`.

Applicable categories:

* Weapons
* Bows
* Shields

Capacity represents occupied inventory instances/slots, not raw quantity.

Flow:

```text
Try Add
↓
Can Merge?
↓
Yes → Merge
↓
No → Check Occupied Capacity
↓
Capacity Available?
↓
Add New Instance
```

Structured results should distinguish:

```text
Success
Invalid Item
Invalid Data
Invalid Class
Category Full
Other Failure
```

UI does not decide whether an item fits.

---

# 29. Capacity Upgrades

Required API:

```text
UpgradeCategoryCapacity(Category)
```

Capacity upgrades can be rewarded through:

* Currency
* Quests
* NPC progression
* Exploration
* Other explicitly designed progression

The inventory remains the authority.

---

# 30. Full Inventory Replacement

Replacement must be atomic.

Flow:

```text
Validate incoming item
↓
Validate old instance
↓
Reserve old instance
↓
Prepare incoming instance
↓
Commit incoming
↓
Commit old removal
↓
Spawn/drop old item
↓
Refresh
```

Never:

```text
Drop old item
↓
Try to add new item
```

That can cause item loss.

If any step fails:

```text
Restore exact previous state
```

Cancel:

```text
No mutation
```

Incoming item preserves:

```text
ItemID
Quantity
CurrentDurability
ItemInstanceID
Upgrade state
Other valid persistent instance state
```

---

# 31. World Interaction Component

Create a reusable:

```text
WorldInteractionComponent
```

Its purpose is to provide a common interface for world objects and gameplay systems.

Potential interaction types:

```text
None
Interact
Grapple
Breakable
Pushable
Pullable
Climbable
```

The system should be expandable.

The initial implementation should only include interactions actually needed.

---

# 32. World Interaction Architecture

Conceptual flow:

```text
Player
↓
Interaction System
↓
Find Target
↓
WorldInteractionComponent
↓
Can Perform Interaction?
↓
Get Interaction Data
↓
Execute Interaction
```

World objects expose capabilities.

The player/tool should not need custom knowledge of every object type.

---

# 33. Grapple Gun

Rihla includes a dedicated grapple gun.

Core functionality:

```text
Aim
↓
Find Grapple Target
↓
Validate Target
↓
Fire Grapple
↓
Attach
↓
Pull / Traverse
↓
Release / Cancel
```

Grapple targets are validated through the world interaction system.

Potential grapple properties:

```text
CanGrapple
GrapplePoint
GrappleDistance
GrappleType
```

The system can later support:

* Pulling toward targets
* Swinging
* Releasing
* Grappling while airborne
* Different grapple surfaces
* Special grapple objects

---

# 34. Shield Parry

Shield parry is a timing-based combat mechanic.

Flow:

```text
Shield Raised
↓
Parry Window
↓
Incoming Attack
↓
Successful Timing?
```

Success:

```text
Negate Damage
↓
Enemy Stagger / Opening
↓
Parry Feedback
```

Failure:

```text
Normal Shield Block
```

or normal attack consequences depending on the combat system.

Shield durability remains controlled by the central durability API.

---

# 35. Perfect Dodge

Perfect dodge is separate from normal movement.

Flow:

```text
Dodge
↓
Perfect Dodge Window
↓
Incoming Attack Misses During Window?
```

Success can trigger:

* Combat advantage
* Enemy opening
* Slow-motion
* Counterattack opportunity
* Special feedback

The exact reward should be implemented once the combat framework is established.

The important rule is that perfect dodge timing is authoritative in gameplay code, not UI.

---

# 36. Dash

Dash is a player movement ability that consumes stamina.

Flow:

```text
Dash Input
↓
Check Stamina
↓
Enough?
 ↙      ↘
YES      NO
 ↓        ↓
Consume   Reject
Stamina
 ↓
Dash
```

Dash should support:

* Ground dash
* Directional dash
* Potential air dash if later desired

Dash must use:

```text
PlayerStatsComponent.DrainStamina()
```

It must not directly modify the stamina variable.

---

# 37. Ability System

The initial player abilities are:

```text
Grapple Gun
Shield Parry
Perfect Dodge
Dash
```

Potential future abilities can be added without restructuring the core player architecture.

Abilities should be independently testable.

---

# 38. Quest System

Rihla requires a proper quest architecture.

Quest state can include:

```text
Locked
Available
Active
Completed
Failed
```

Quest objectives can include:

```text
Collect Item
Kill Enemy
Reach Location
Talk To NPC
Interact With Object
Use Item
Upgrade Equipment
Cook Item
```

Quest rewards can include:

```text
Currency
Items
Materials
Equipment
Recipes
Ability Unlocks
Capacity Upgrades
Upgrade Unlocks
```

---

# 39. Equipment Upgrade + Quest Integration

Quests can unlock:

```text
Upgrade Tiers
Blacksmiths
Recipes
Materials
Abilities
New Equipment
```

Example:

```text
Blacksmith Quest
↓
Complete Quest
↓
Tier 2 Equipment Upgrades Unlocked
↓
New Materials Become Useful
↓
Upgrade Equipment
```

This creates progression without requiring fusion.

---

# 40. Durability

Required authoritative API:

```text
DamageItem(ItemInstanceID, Amount)
```

Optional:

```text
DamageEquippedItem(EquipmentCategory, Amount)
```

Flow:

```text
Resolve Instance
↓
Validate
↓
Subtract Durability
↓
Clamp
↓
Detect Break
↓
Refresh / Broadcast
```

Combat must never directly modify:

```text
CurrentDurability
```

---

# 41. Weapon Breaking

When equipment reaches zero durability:

```text
Resolve exact ItemInstanceID
↓
Unequip
↓
Remove instance
↓
Clear equipment reference
↓
Invalidate stale quick-select selection
↓
VFX/SFX
↓
Refresh UI
```

No stale instance IDs may remain active.

---

# 42. Usage Tracking

`S_ItemSlot`:

```text
TimesUsed : Integer
```

Required API:

```text
RegisterItemUsed(ItemInstanceID)
```

It should increment only after successful gameplay actions.

Examples:

```text
Equip
Eat / Consume
Other explicitly defined successful uses
```

Do NOT count:

```text
Hover
Preview
Menu open
Highlight
Cancelled selection
Uncommitted operation
```

Only one system should increment `TimesUsed`.

---

# 43. Sorting

Required:

```text
E_SortMode
```

Modes:

```text
Category
MostUsed
Damage
Armor
Food
```

`CurrentSortMode` is runtime view state.

Do not persist it unless intentionally designed as a player preference.

---

# 44. Derived Sorting Representation

`ItemSlots` remains authoritative storage.

Never physically reorder it merely for sorting.

Create:

```text
SortedItemInstances : TArray<FGuid>
```

This is:

```text
Derived
Rebuildable
Non-authoritative
Non-persistent
```

Sorting flow:

```text
ItemSlots
↓
Resolve effective values
↓
Apply sort criteria
↓
Deterministic tie-break
↓
SortedItemInstances
```

Tie-break:

```text
Primary Criterion
↓
Category Priority
↓
Stable ItemInstanceID
```

Never array index.

---

# 45. Sorting + Quick Select

Quick select should derive from the same authoritative inventory representation.

Conceptually:

```text
ItemSlots
↓
SortedItemInstances
↓
Category Filter
↓
Quick Select
```

Quick select should never create its own inventory database.

---

# 46. Save / Load

`S_GameData` is the persistent save snapshot.

Required:

```text
SaveVersion
```

Save:

### Inventory

```text
ItemInstanceID
ItemID
Quantity
CurrentDurability
TimesUsed
UpgradeLevel
Other legitimate persistent instance state
```

### Equipment

Equipment references must use:

```text
ItemInstanceID
```

### Capacity

Save:

```text
Category Capacity
Capacity Upgrades
```

### Player Resources

Save:

```text
Current Health
Max Health
Temporary Health
Current Stamina
Max Stamina
Temporary Stamina
Currency
```

No Battery.

### Quests

Save:

```text
Quest State
Quest Progress
Completed Objectives
Unlocked Rewards / Progression
```

### Cooking

Save only legitimate persistent data such as:

```text
Discovered Recipes
Recipe Unlocks
```

if those systems are designed to persist.

No fusion state.

No building state.

---

# 47. Save Versioning

Save data must be versioned.

Future structural changes should be handled through migration rather than assuming every save has the newest structure.

Example:

```text
SaveVersion 1
↓
Migration
↓
SaveVersion 2
```

The system should remain expandable.

---

# 48. GameMode / Template Cleanup

Audit the project for stale template references.

Particularly:

```text
Third Person template paths
Old GameMode references
Redirectors
Obsolete Blueprint references
```

Update the authoritative Rihla GameMode.

Remove obsolete redirects only after confirming there are no remaining references.

---

# 49. Current Integration Gates

Before finalizing later systems, complete:

## Gate 1 — Container Save Architecture

Persistent containers must use stable serialized state rather than runtime inventory references.

## Gate 2 — ItemInstanceID Audit

Audit:

```text
Add
Remove
Drop
Equip
Unequip
Use
Transfer
Move
Split
Merge
Durability
Sorting
Swap
Save
Load
Quick Select
Cooking results
Equipment upgrades
Quest rewards
```

Every operation must preserve identity correctly.

## Gate 3 — GameMode Cleanup

Remove stale template paths and references.

## Gate 4 — Effective Item Resolution

Finalize:

```text
GetEffectiveDamage()
GetEffectiveDefense()
GetEffectiveItemEffects()
GetEffectiveDurability()
```

before completing durability, upgrades, sorting and item-use systems.

---

# 50. Dependency Order

Recommended implementation order:

```text
0. Foundational Cleanup / Integration Gates
        ↓
1. Player Stats
        ↓
2. Generic Quick Select
        ↓
3. Icon Inventory Tabs
        ↓
4. Container Popup
        ↓
5. Eating / Item Use
        ↓
6. Cooking
        ↓
7. Currency
        ↓
8. Equipment Progression / Upgrades
        ↓
9. Capacity Enforcement
        ↓
10. Full Inventory Swap
        ↓
11. Durability / Weapon Breaking
        ↓
12. WorldInteractionComponent
        ↓
13. Grapple Gun
        ↓
14. Shield Parry
        ↓
15. Perfect Dodge
        ↓
16. Dash / Stamina Integration
        ↓
17. Quest System
        ↓
18. Wardrobe Hover
        ↓
19. Save / Load Expansion
        ↓
20. Sorting + Synchronization
```

Some systems can be developed in parallel after their dependencies are stable.

---

# 51. Cross-System Contracts

## Inventory owns

* Inventory instances
* Item stacks
* Equipment
* Capacity
* Durability
* Usage
* Effective item queries
* Item use
* Inventory transactions
* Sorting source data

## PlayerStatsComponent owns

* Health
* Temporary health
* Stamina
* Temporary stamina

## Currency system owns

* Currency
* Currency transactions

## Cooking system owns

* Recipes
* Cooking
* Cooking validation
* Cooking results/effects

## Equipment Upgrade system owns

* Upgrade requirements
* Upgrade progression
* Upgrade transactions

## Quest system owns

* Quest state
* Objectives
* Quest rewards
* Quest progression

## WorldInteractionComponent owns

* World interaction capabilities
* Interaction validation
* Interaction data

## Combat owns

* Attacks
* Parry timing
* Dodge timing
* Combat reactions

## Ability systems own

* Grapple
* Dash
* Other player abilities

## Quick Select owns

* Opening
* Category
* Selection
* Presentation state

## Widgets own

* Display
* Filtering
* Selection interaction

They do not own gameplay state.

## Static database owns

* Static item definitions
* Base values
* Static descriptions
* Static categories
* Static recipes where appropriate

## Save system owns

* Persistent snapshot
* Save versioning
* Migration

---

# 52. Critical "Must Not" Rules

Never:

* Modify static DataTable values to represent runtime item state.
* Use `ItemID` as the unique identity of a physical item.
* Use array index as persistent identity.
* Let UI directly mutate inventory.
* Let UI directly modify health/stamina/currency.
* Create a second inventory database in quick select.
* Let combat directly modify durability.
* Have multiple systems increment `TimesUsed`.
* Have multiple independent sorting systems.
* Put capacity logic only in the container UI.
* Drop the old replacement before the new item is committed.
* Consume an item before its transaction is safely committed.
* Save `SortedItemInstances` as authoritative data.
* Save array indices as item identity.
* Save `CurrentSortMode` unless intentionally designed as a persistent preference.
* Modify base damage at runtime.
* Reintroduce fusion/infusion.
* Reintroduce building.
* Reintroduce Battery.
* Implement cooking as fusion.
* Put quest state inside inventory.
* Put currency inside the item inventory unless specifically required for a gameplay item.
* Create custom interaction logic for every world object when a reusable interaction component can handle it.

---

# 53. Definition of Done

Rihla's core architecture is complete when:

### Inventory

* Instance-based inventory
* Stable `FGuid` IDs
* Correct stacking
* Split/merge
* Drop
* Transfer
* Equipment
* Equipment highlighting
* Capacity
* Capacity upgrades
* Full inventory replacement
* Sorting

### Equipment

* Weapons
* Bows
* Shields
* Armor
* Instance-based equipment references
* Durability
* Breaking
* Equipment upgrades
* Material requirements
* Currency requirements

### Player

* Health
* Temporary health
* Stamina
* Temporary stamina
* Damage
* Healing
* Stamina drain
* Resource upgrades
* Death handling

### Combat

* Shield parry
* Perfect dodge
* Normal dodge
* Dash
* Stamina integration
* Durability integration

### Traversal

* Grapple gun
* Grapple target validation
* World interaction framework
* Expandable interaction types

### Cooking

* Ingredients
* Recipes
* Cooking stations
* Cooked items
* Food effects
* Recipe discovery
* Transaction-safe ingredient consumption

### Currency

* Currency balance
* Add
* Spend
* Affordability checks
* Shop integration
* Upgrade integration
* Quest rewards
* Save/load

### Quests

* Quest state
* Objectives
* Rewards
* Item objectives
* NPC objectives
* Location objectives
* Combat objectives
* Cooking objectives
* Upgrade objectives

### Quick Select

* Three menus
* Category-driven filtering
* Dynamic slot count
* Instance-aware selection
* Empty-category protection
* Equipment integration
* Item-use integration
* Selection preservation

### UI

* Icon tabs
* Wardrobe hover information
* Equipment highlighting
* Health display
* Stamina display
* Currency display
* Quick select
* Full inventory replacement
* Container pickup popup
* Inventory pause

### Containers

* Normal pickup
* Capacity checking
* Replacement
* Transaction safety
* Persistent container state

### Save / Load

* Instance-aware inventory
* Equipment
* Durability
* Upgrade levels
* Usage
* Capacity
* Currency
* Player resources
* Quest progression
* Recipe progression
* Save versions
* Migration support

### Architecture

* No conflicting sources of truth
* No ItemID-only identity
* No array-index identity
* No runtime static DataTable mutation
* No UI gameplay mutation
* No duplicate inventory logic
* No duplicate durability logic
* No duplicate sorting logic
* No fusion
* No infusion
* No building
* No Battery

---

# 54. Final Rihla Feature Set

## Inventory

* Instance-based inventory
* Stable item IDs
* Stack management
* Split / merge
* Drop
* Transfer
* Equipment
* Equipment highlighting
* Capacity
* Capacity upgrades
* Full replacement
* Sorting

## Equipment

* Weapons
* Bows
* Shields
* Armor
* Durability
* Weapon breaking
* Equipment progression
* Material-based upgrades
* Currency-based upgrades
* Quest-based progression

## Player Resources

* Health
* Temporary health
* Stamina
* Temporary stamina
* Currency

## Combat

* Weapon combat
* Shield blocking
* Shield parry
* Perfect dodge
* Dash
* Stamina management
* Durability

## Traversal

* Grapple gun
* Grapple points
* World interaction
* Expandable traversal interactions

## Cooking

* Ingredients
* Recipes
* Cooking stations
* Food
* Healing
* Stamina recovery
* Temporary effects
* Recipe discovery

## World

* Generic interaction framework
* Grappleable objects
* Breakable objects
* Pushable objects
* Climbable objects
* Expandable interaction types

## Quests

* Objectives
* NPC quests
* Collection quests
* Combat quests
* Exploration quests
* Cooking quests
* Upgrade quests
* Rewards
* Progression unlocks

## Quick Select

* Three menus
* Category filtering
* Dynamic slot count
* Instance-aware selection
* Equipment/use integration
* Empty-category protection

## UI

* Icon inventory tabs
* Wardrobe hover
* Equipment highlighting
* Health/stamina
* Currency
* Quick select
* Inventory replacement
* Container popup
* Pause-on-open

## Save / Load

* Inventory
* Item instances
* Equipment
* Durability
* Upgrades
* Usage
* Capacity
* Currency
* Player resources
* Quests
* Recipe progression
* Save versions

---

# 55. Systems Explicitly Removed From Rihla

The following are **not part of the Rihla design**:

```text
Building / Ultrahand
Ancient Relic Construction
Battery
Weapon Fusion
Weapon Infusion
Arrow Fusion
Arrow Infusion
Permanent Item Fusion
Temporary Item Fusion
Cooking-as-Fusion
Fusion-specific Quick Select
Build-specific Quick Select
Build transactions
Fusion save state
Infusion save state
Battery save state
Build-world persistence
```

The game's progression instead comes from:

```text
EXPLORATION
    +
COMBAT
    +
TRAVERSAL
    +
QUESTS
    +
MATERIALS
    +
COOKING
    +
CURRENCY
    +
EQUIPMENT UPGRADES
    +
PLAYER ABILITIES
```

**This is the canonical Rihla v6 direction.**
