# Rihla HUD Implementation Guide — UE 5.7.4

This guide explains how to build the simple Rihla player HUD inspired by the mockup:

- Health hearts in the top-left
- No stamina bar
- Contextual interaction prompt in the bottom-left
- Item pickup popup in the lower-right
- Existing gameplay systems remain authoritative

---

## 1. HUD Overview

### Target layout

```text
┌─────────────────────────────────────────────┐

 ♥  ♥  ♥  ♥  ♥  ♥                     [MAP]



                              ┌──────────────┐
                              │    🍎 Apple │
                              │              │
                              │ A common...  │
                              │          x1 │
                              └──────────────┘


 [E] Talk

└─────────────────────────────────────────────┘
```

### Widget architecture

```text
WBP_PlayerHUD
│
├── WBP_HealthDisplay
│
├── WBP_InteractionPrompt
│
└── WBP_PickupPopup
```

The HUD should integrate with the existing Rihla systems rather than owning gameplay data.

---

# PART 1 — Create the Main HUD

## Step 1 — Create the HUD folders

In the Content Browser:

```text
Content
└── UI
    └── HUD
```

Create folders if they do not already exist.

## Step 2 — Create the widget

Create:

```text
Widget Blueprint
→ WBP_PlayerHUD
```

Open it.

## Step 3 — Create the base layout

Add a:

```text
Canvas Panel
```

The eventual hierarchy should look like:

```text
Canvas Panel

├── HealthContainer
│
├── InteractionPrompt
│
└── PickupPopup
```

---

# PART 2 — Health Hearts

## Step 4 — Create the hearts container

Inside `WBP_PlayerHUD`, add:

```text
Horizontal Box
```

Rename it:

```text
HB_Hearts
```

Set:

```text
Anchor: Top Left
Position X: 60
Position Y: 45
```

The container will hold the generated heart widgets.

---

## Step 5 — Health system rules

The existing project health system uses:

```text
1 = Quarter Heart
4 = Full Heart
```

Example:

```text
MaxHealth = 12
```

This equals:

```text
3 Full Hearts
```

Therefore, do **not** use a standard Progress Bar.

Generate individual heart widgets based on:

```text
CurrentHealth
MaxHealth
TemporaryHealth
```

The HUD should only read these values. It must not directly modify health.

---

## Step 6 — Create WBP_Heart

Create:

```text
Widget Blueprint
→ WBP_Heart
```

Widget hierarchy:

```text
Canvas Panel
└── Image
```

Rename the image:

```text
IMG_Heart
```

Suggested size:

```text
45 × 45
```

Eventually, prepare these states:

```text
Heart_Full
Heart_ThreeQuarter
Heart_Half
Heart_Quarter
Heart_Empty
```

Temporary placeholder images are fine while testing.

---

## Step 7 — Connect to PlayerStatsComponent

Create an Event Dispatcher inside `PlayerStatsComponent`:

```text
OnHealthChanged
```

Inputs:

```text
CurrentHealth (Integer)
MaxHealth (Integer)
TemporaryHealth (Integer)
```

Whenever health changes through systems such as:

```text
TakeDamage
RestoreHealth
AddTemporaryHealth
SetMaxHealth
```

Broadcast:

```text
OnHealthChanged
```

### Correct update flow

```text
PlayerStatsComponent
        ↓
OnHealthChanged
        ↓
WBP_PlayerHUD
        ↓
UpdateHearts
```

### Do not use Event Tick

Do not do:

```text
Event Tick
↓
Get Health
↓
Update UI
```

Use events instead.

---

## Step 8 — Create UpdateHearts

Inside `WBP_PlayerHUD`, create:

```text
UpdateHearts
```

Inputs:

```text
CurrentHealth
MaxHealth
TemporaryHealth
```

Basic logic:

```text
Clear Children
↓
Calculate Heart Count
↓
For Loop
↓
Create WBP_Heart
↓
Determine Heart Fill
↓
Add Child to HB_Hearts
```

Since:

```text
4 Health = 1 Heart
```

The total heart count is:

```text
Ceil(MaxHealth / 4)
```

Example:

```text
CurrentHealth = 10
MaxHealth = 12
```

The HUD displays three hearts, with the final heart partially filled.

---

# PART 3 — Interaction Prompt

## Step 9 — Create the interaction prompt

Inside `WBP_PlayerHUD`:

```text
Canvas Panel
→ Horizontal Box
```

Rename:

```text
HB_InteractionPrompt
```

Set:

```text
Anchor: Bottom Left
Position X: 60
Position Y: -100
```

Suggested hierarchy:

```text
HB_InteractionPrompt

├── Border
│   └── Text
│       E
│
└── Text
    Talk
```

The prompt can later display:

```text
E Talk
E Pick Up
E Open
E Speak
```

The interaction system should control:

```text
ShowInteractionPrompt
HideInteractionPrompt
UpdateInteractionText
```

---

# PART 4 — Item Pickup Popup

## Step 10 — Create WBP_PickupPopup

Create:

```text
Widget Blueprint
→ WBP_PickupPopup
```

Suggested hierarchy:

```text
Canvas Panel
└── Border
    └── Horizontal Box

        ├── Image
        │   └── Item Icon
        │
        └── Vertical Box

            ├── Item Name
            │
            ├── Description
            │
            └── Quantity
```

Name the important widgets:

```text
IMG_ItemIcon
TXT_ItemName
TXT_Description
TXT_Quantity
```

---

## Step 11 — Popup variables

Inside `WBP_PickupPopup`, create:

```text
ItemID
```

Use the same Item ID type already used by the project.

Also create:

```text
Quantity
```

Type:

```text
Integer
```

Enable:

```text
Instance Editable
Expose on Spawn
```

Create a function:

```text
InitializePickupPopup
```

Inputs:

```text
ItemID
Quantity
```

---

## Step 12 — Get item information from the existing data system

Use the existing data flow:

```text
S_ItemSlot
        ↓
ItemID
        ↓
DT_ItemData
        ↓
S_ItemInfo
```

The popup should not create a separate item database.

Use:

```text
ItemID
↓
Get Item Data
↓
Break S_ItemInfo
```

Read:

```text
Item Name
Description
Item Icon
```

Then update:

```text
TXT_ItemName
TXT_Description
IMG_ItemIcon
TXT_Quantity
```

Example:

```text
Apple

A common fruit found in many regions.

x1
```

---

# PART 5 — Add the Popup to the HUD

## Step 13 — Add WBP_PickupPopup

Return to:

```text
WBP_PlayerHUD
```

Add:

```text
WBP_PickupPopup
```

to the Canvas Panel.

Rename it:

```text
PickupPopup
```

Set:

```text
Anchor: Bottom Right
Position X: -80
Position Y: -200
```

Initially set:

```text
Visibility = Collapsed
```

---

## Step 14 — Create ShowPickup

Inside `WBP_PlayerHUD`, create:

```text
ShowPickup
```

Inputs:

```text
ItemID
Quantity
```

Logic:

```text
PickupPopup
↓
InitializePickupPopup(ItemID, Quantity)
↓
Set Visibility = Visible
↓
Play Appearance Animation
↓
Delay (3 Seconds)
↓
Play Hide Animation
↓
Delay (0.3 Seconds)
↓
Set Visibility = Collapsed
```

---

# PART 6 — Pickup Animation

## Step 15 — Create the animation

Inside `WBP_PickupPopup`, create:

```text
PickupAppear
```

### Start

```text
Opacity = 0
Position X = +100
```

### At approximately 0.25 seconds

```text
Opacity = 1
Position X = 0
```

### Hide animation

```text
Opacity = 1
↓
Opacity = 0

Position X = +50
```

This creates a clean slide-in and fade effect.

---

# PART 7 — Connect the Popup to Inventory

## Step 16 — Correct pickup flow

The popup should only appear after the inventory successfully accepts the item.

Correct flow:

```text
Player interacts with item
        ↓
Attempt AddItem
        ↓
Inventory validates capacity
        ↓
Inventory successfully adds item
        ↓
World item is removed
        ↓
HUD.ShowPickup
        ↓
Popup appears
```

Do **not** use:

```text
Pickup Item
↓
Show Popup
↓
Try Adding Item
```

Otherwise, the player could see a pickup notification even if the inventory is full.

---

## Step 17 — Blueprint integration

Find the existing pickup logic that calls:

```text
BPC_Inventory
→ Add Item
```

After the inventory operation returns successfully:

```text
Add Item
↓
Was Successful?
```

If:

```text
TRUE
```

Then:

```text
Get Player Controller
↓
Get PlayerHUDReference
↓
ShowPickup(ItemID, Quantity)
```

Only remove the world item after the inventory operation succeeds.

---

# PART 8 — Main HUD Creation

## Step 18 — Spawn the HUD

Inside the existing Player Controller, on:

```text
Event BeginPlay
```

Create:

```text
Event BeginPlay
↓
Create Widget
    Class = WBP_PlayerHUD
↓
Promote Return Value
    PlayerHUDReference
↓
Add To Viewport
```

Then connect the health system:

```text
Get Player Character
↓
Get PlayerStatsComponent
↓
Bind Event to OnHealthChanged
```

The bound event calls:

```text
PlayerHUDReference.UpdateHearts
```

---

# Final Architecture

The recommended Rihla HUD architecture:

```text
Player Controller
│
└── WBP_PlayerHUD
        │
        ├── Health Display
        │       ↑
        │       │
        │ PlayerStatsComponent
        │
        ├── Interaction Prompt
        │       ↑
        │       │
        │ Interaction System
        │
        └── Pickup Popup
                ↑
                │
             BPC_Inventory
                ↑
                │
            Successful Pickup
```

---

# Future Improvement — Popup Queue

Do not build this first.

After the basic popup works, add:

```text
PickupQueue
```

Each queue entry contains:

```text
ItemID
Quantity
```

Flow:

```text
Show Pickup Request
        ↓
Is Popup Currently Showing?
       /      Yes   No
      ↓     ↓
   Add Queue Show Immediately
              ↓
         Play Animation
              ↓
           Wait
              ↓
         Queue Empty?
          /       \
        Yes        No
        ↓          ↓
      Hide      Show Next
```

This prevents rapid pickups from overwriting each other.

---

# Recommended Implementation Order

Do not build everything simultaneously.

## Part 1

```text
Create WBP_PlayerHUD
```

## Part 2

```text
Create hearts
Connect them to PlayerStatsComponent
```

## Part 3

```text
Create WBP_PickupPopup
```

## Part 4

```text
Connect successful inventory pickups to ShowPickup
```

## Part 5

```text
Add animations
Add the pickup queue
```

---

# Important Project Rules

- Gameplay systems own gameplay data.
- UI reads and displays data.
- UI must not directly mutate health or inventory state.
- The item pickup popup appears only after successful inventory insertion.
- Static item information should continue coming from the existing item data system.
- Use events and dispatchers rather than updating the HUD every frame.
- Keep the HUD minimal and hide elements when they are not needed.

