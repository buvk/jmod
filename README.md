# jmod (juice mod) for Diablo II 1.09b

`jmod` patches a specific Diablo II 1.09b install with five helpers:

- Quick Cast for keyboard-bound skills
- Toggleable item labels via the in-game Show Items binding
- Automatic gold pickup near the character
- Optional orange rune names (no MPQ replacement or `-direct -txt` needed)
- Ground-label filtering for small gold piles and low-tier potions

The code is split into a small modular layout under `src/` and builds both DLL variants from the same source files.

## Supported loading methods

Choose one loading method. If both DLLs are loaded into the same game process,
only the first instance to start its hook thread activates the gameplay helpers.
The other instance remains loaded but does not install hooks.

| Method | How to load it | Notes |
| --- | --- | --- |
| `D2Win.dll` proxy | Replace the game's `D2Win.dll` | Keep the original as `D2Win_original.dll` |
| `jmod.dll` | Load it via PlugY's `DllToLoad` | Leave the original `D2Win.dll` intact |

Both variants read `jmod.ini` from the same folder as the DLL.

## Default configuration

The bundled `jmod.ini` enables all five helpers by default:

```ini
[Mods]
QuickCast=1
AlwaysShowItems=1
AutoGoldPickup=1
GoldPickupInTown=0
RuneColor=1
GoldPickupRange=4
GoldScanIntervalMs=30
GoldRequestIntervalMs=50
GoldRetryIntervalMs=500

[LootFilter]
; 0 = off, 1 = on. Only ground labels are hidden.
Enabled=1
; Hide gold piles smaller than this amount. 0 shows all piles.
MinGold=500
; Any, Minor, Light, Standard, Greater, Super, None
; Shows the selected tier and higher; Any shows all; None hides all.
MinHealthPotion=Greater
MinManaPotion=Greater
; Any, Full, None. Full hides regular rejuvenation potions.
MinRejuvenationPotion=Full
```

Meaning of the settings:

- `QuickCast=1`: holds the right mouse button while a bound skill key is held.
  Opening the Escape menu releases the simulated hold; a key held through the
  menu must be released and pressed again before it can cast.
- `AlwaysShowItems=1`: makes the normal Show Items binding toggle labels on and off.
  Ground labels are drawn before the hover tooltip pass so inventory and
  equipment item stats can appear over labels. Ground labels and their hover
  targets are hidden while the Escape menu is open.
- `AutoGoldPickup=1`: requests pickup of nearby gold piles while the game is
  focused and the Escape menu is closed.
- `GoldPickupInTown=0`: skips automatic gold pickup in the five towns; set to `1` to allow it.
- `RuneColor=1`: displays English rune names in orange through the game's
  string lookup. Set it to `0` to leave the original names unchanged. This option
  checks each rune name against the supplied 1.09b strings before coloring it;
  names that differ keep their original color.
  Remove any previously installed orange-rune `.tbl` overrides before testing
  this option, to avoid coloring the names twice.
- `GoldPickupRange`: radius in map tiles, clamped to 1–6.
- `GoldScanIntervalMs`, `GoldRequestIntervalMs`, `GoldRetryIntervalMs`: timing controls for gold scanning and pickup requests.
- `[LootFilter] Enabled=1`: filter ground labels shown by the game's Show Items binding, including when `AlwaysShowItems` is on. It does not delete items or change automatic gold pickup.
- `MinGold`: hides piles below the specified amount; a pile of exactly that amount remains visible.
- `MinHealthPotion` and `MinManaPotion`: show the selected tier and higher tiers; `Any` shows all, and `None` hides all five tiers.
- `MinRejuvenationPotion`: `Any` shows both kinds, `Full` shows only Full Rejuvenation Potions, and `None` hides both.

An unrecognized potion setting falls back to `Any`. If the label hook does not match the running client, jmod leaves labels unfiltered.

Restart the game after changing the INI. The patch is tied to the supplied 1.09b binaries and their pointer layout.

## Installation

### Proxy method

1. Make a copy of your Diablo II 1.09b folder.
2. Rename the original `D2Win.dll` in that copy to `D2Win_original.dll`.
3. Copy the built `build/D2Win.dll` into the game folder beside `D2Win_original.dll`.
4. Copy `jmod.ini` into the same folder if it is not already present.
5. Launch the game from the copy and test in single-player.

### PlugY method

1. Restore the original `D2Win.dll` if the proxy was previously installed.
2. Copy the built `build/jmod.dll` into the game folder.
3. Add `jmod.dll` to PlugY's `[GENERAL] DllToLoad` list.
4. Start the game through PlugY and verify the features.

## Build

This project is built with the 32-bit MinGW-w64 toolchain, not a 64-bit or MSYS runtime toolchain.

In MSYS2, install the expected toolchain:

```sh
pacman -S --needed mingw-w64-i686-gcc make
```

Then build from the repository root:

```sh
make
```

If the compiler is not on your `PATH`, you can point Make at it explicitly:

```sh
make CC="/mingw32/bin/i686-w64-mingw32-gcc"
```

The output is placed under `build/` and the repository is configured to ignore generated binaries and temporary build artifacts.

## Repository layout

```text
.
├── build/                 # compiled DLLs produced by make
├── src/
│   ├── auto_gold.c/.h     # automatic gold pickup
│   ├── config.c/.h        # INI parsing and options
│   ├── game_ui.h          # Escape menu state
│   ├── item_labels.c/.h   # item label toggle and drawing
│   ├── jmod.c             # DLL startup, window discovery, message hook
│   ├── loot_filter.c/.h   # ground item label filter
│   ├── quick_cast.c/.h    # skill key handling and simulated mouse input
│   └── rune_color.c/.h    # optional rune name color
├── jmod.ini               # default runtime settings
├── D2Win.def              # export definition file for the proxy build
├── Makefile               # build rules
├── .gitignore             # ignores generated files
└── README.md              # this file
```

This repository includes source and configuration only; it does not bundle the game binaries, the original `D2Win.dll`, or any compiled output in the root directory.
