# Loot filter exclusions and 1.09b verification

This document records item codes deliberately omitted from the named
`loot_filter.ini` and `src/item_names.h` list. An omitted code is **still
visible** if it occurs in-game: the filter leaves unknown codes alone.
Removing a name from this list does not remove an item from Diablo II.

The initial list was assembled from **1.13 item tables**. We have since
compared its codes with `weapons.txt`, `armor.txt`, `misc.txt`, and
`TreasureClassEx.txt` extracted from the user's **`d2exp.mpq` and
`patch_d2.mpq`**. For files present in both, the patch archive is the
effective source. A code absent from both sets of item tables cannot be
an ordinary item in that unmodified installation. `spawnable` by itself
does **not** prove that an item is unavailable: `rvs` and `rvl` Rejuvenation
Potions have `spawnable=0` but occur directly in the supplied treasure
classes. An item also can be obtained through quests, vendors, recipes, or
PvP without a random treasure-class drop.

| Code | Omitted name | Patch `version` | Patch `spawnable` | Why excluded from the named list |
| --- | --- | ---: | ---: | --- |
| `hpo` | Healing Potion | 0 | 0 | Duplicate name of the active `hp3` potion; excluded as a legacy record. |
| `mpo` | Mana Potion | 0 | 0 | Duplicate name of the active `mp3` potion; excluded as a legacy record. |
| `rps` | Small Red Potion | 100 | 0 | Legacy red-potion entry. |
| `rpl` | Large Red Potion | 100 | 0 | Legacy red-potion entry. |
| `bps` | Small Blue Potion | 100 | 0 | Legacy blue-potion entry. |
| `bpl` | Large Blue Potion | 100 | 0 | Legacy blue-potion entry. |
| `elx` | elixir | 0 | 1 | Excluded at the user's request; no direct reference in the supplied treasure classes; other sources unverified. |
| `hpf` | Full Healing Potion | 0 | 0 | Legacy full-restoration entry. |
| `mpf` | Full Mana Potion | 0 | 0 | Legacy full-restoration entry. |
| `0sc` | Scroll | 100 | 1 | Generic scroll; distinct from Town Portal and Identify Scrolls; no direct treasure-class reference. |

The `version` field distinguishes classic (`0`) and expansion (`100`)
records; it is **not** a Diablo II patch number. The four red/blue entries
are present in the patch table and remain excluded by user choice; their
absence from the older base archive was not evidence that they were absent
from 1.09b.

## Check of the supplied `excel content.zip`

The uploaded archive contains a **base-game-style** item set: 175 weapon,
92 armor, and 94 misc records. It has `TreasureClass.txt`, but no
`TreasureClassEx.txt` or `ItemTypes.txt`, and its `misc.txt` contains no
runes. The archive does not identify its source MPQ or patch version.
It cannot verify the complete item list for an expansion 1.09b install.

| Omitted codes | Present in supplied `misc.txt`? | Direct code in supplied `TreasureClass.txt`? |
| --- | --- | --- |
| `hpo`, `mpo`, `hpf`, `mpf` | Yes; `spawnable=0` | No |
| `elx` | Yes; `spawnable=1` | No |
| `rps`, `rpl`, `bps`, `bpl`, `0sc` | No | No |

This confirms only what is in **this archive**. In particular, it does
not prove that elixirs cannot be produced by any mechanism, or that missing
codes are absent from expansion or patch archives. As a cross-check, the
archive marks `rvs` and `rvl` Rejuvenation Potions `spawnable=0` while its
`TreasureClass.txt` explicitly references both. Random-drop and overall
item-availability claims need separate evidence.

## Removed after checking `d2exp.mpq` and `patch_d2.mpq`

The following **13 codes occur in neither archive's item tables**. Their
names came from 1.13 data and cannot represent normal items in the supplied
1.09b installation. Even if a mod adds one of these codes, jmod will leave
it visible because unknown codes are not filtered.

| Code | Former filter entry |
| --- | --- |
| `pk1`, `pk2`, `pk3` | Pandemonium Key 1, 2, 3 |
| `dhn`, `mbr`, `bey` | Diablo's Horn, Mephisto's Brain, Baal's Eye |
| `toa` | Token of Absolution |
| `tes`, `ceh`, `bet`, `fed` | Twisted, Charged, Burning, Festering Essence |
| `std` | Standard |
| `neg` | Hellspawn Skull |

The following **12 codes do occur** in the patch `misc.txt`, but all are
named `Not used`, have `spawnable=0` and `quest=0`, and have no direct code
reference in its `TreasureClassEx.txt`. Their earlier filter labels were
`Unused Item (code)`. This supports removing them from the editable list;
it does not establish that no mod or scripted source can create one.

| Code | Code | Code | Code |
| --- | --- | --- | --- |
| `spe` | `flg` | `fng` | `tal` |
| `qll` | `sol` | `hrn` | `hrt` |
| `jaw` | `scz` | `brz` | `eyz` |

## Other filter entries to review separately

`gold=0x3F` is ineffective: `MinGold` in `jmod.ini` controls ground gold
labels. The `[Quest Items]` masks also have no effect because jmod always
shows quest items. Neither issue is evidence that these items cannot appear.
Other entries without a direct random-drop reference include vendor tomes,
Player Ears, and quest items; retain them until each acquisition path has
been considered. The same care applies to `elx`, `0sc`, and the red/blue
potions excluded by user choice above.

Initial reference: [1.13 Misc.txt](https://github.com/fabd/diablo2/blob/master/code/d2_113_data/Misc.txt).
