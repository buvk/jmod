#ifndef JMOD_D2_109B_H
#define JMOD_D2_109B_H

/* Reverse-engineered constants for the exact Diablo II 1.09b binaries
   supported by jmod. Module offsets are relative to the DLL base. */

/* D2Client globals and tables. */
#define D2CLIENT_PLAYER_PTR_OFFSET                 0x127578u
#define D2CLIENT_ESCAPE_MENU_STATE_OFFSET          0x125a58u
#define D2CLIENT_TARGET_TYPE_OFFSET                0x116db8u
#define D2CLIENT_TARGET_ID_OFFSET                  0x116dd0u
#define D2CLIENT_TARGET_REFRESH_OFFSET             0x116dd4u
#define D2CLIENT_KEY_BINDINGS_OFFSET               0x11e128u
#define D2CLIENT_ITEM_LABELS_OFFSET                0x122490u
#define D2CLIENT_ITEM_LABEL_COUNT_OFFSET           0x124890u
#define D2CLIENT_ITEM_LABEL_RENDER_FLAG_OFFSET     0x125a68u
#define D2CLIENT_UNIT_HASH_TABLES_OFFSET           0x125d78u

/* D2Client functions. */
#define D2CLIENT_FN_GAME_MODE_OFFSET               0x14a20u
#define D2CLIENT_FN_GET_SELECTED_UNIT_OFFSET       0x14cf0u
#define D2CLIENT_FN_SELECT_LABEL_TARGET_OFFSET     0x14d60u
#define D2CLIENT_FN_SET_CURSOR_TARGET_OFFSET       0x14db0u
#define D2CLIENT_FN_UPDATE_CURSOR_TARGET_OFFSET    0x14fc0u
#define D2CLIENT_FN_DRAW_ITEM_LABELS_OFFSET        0x63b60u
#define D2CLIENT_FN_DRAW_HOVER_OFFSET              0x861c0u
#define D2CLIENT_FN_CURSOR_X_OFFSET                0xb6670u
#define D2CLIENT_FN_CURSOR_Y_OFFSET                0xb6680u

/* D2Client patch sites. */
#define D2CLIENT_HOOK_LOOT_LABEL_OFFSET            0x63bf9u
#define D2CLIENT_HOOK_LOOT_HOVER_OFFSET            0x8729eu
#define D2CLIENT_HOOK_LOOT_CURSOR_OFFSET           0x155f4u
#define D2CLIENT_HOOK_LABEL_TOOLTIP_OFFSET         0x872a5u
#define D2CLIENT_HOOK_LABEL_RENDER_FLAG_OFFSET     0x877d2u
#define D2CLIENT_HOOK_LABEL_DRAW_CALL_OFFSET       0x877e5u

/* Key binding table. */
#define D2CLIENT_KEY_BINDING_COUNT                 114u
#define D2CLIENT_KEY_BINDING_STRIDE                10u
#define D2CLIENT_KEY_BINDING_ACTION_OFFSET         0u
#define D2CLIENT_KEY_BINDING_KEY_OFFSET            4u
#define D2ACTION_SHOW_ITEMS                        37u
#define D2ACTION_SKILL_RANGE1_FIRST                14u
#define D2ACTION_SKILL_RANGE1_LAST                 21u
#define D2ACTION_SKILL_RANGE2_FIRST                46u
#define D2ACTION_SKILL_RANGE2_LAST                 53u

/* D2Unit fields and unit types. */
#define D2UNIT_TYPE_OFFSET                         0x00u
#define D2UNIT_CLASS_ID_OFFSET                     0x04u
#define D2UNIT_ID_OFFSET                           0x08u
#define D2UNIT_MODE_OFFSET                         0x0cu
#define D2UNIT_PATH_OFFSET                         0x38u
#define D2UNIT_HASH_NEXT_OFFSET                    0x108u
#define D2UNIT_PLAYER                              0u
#define D2UNIT_MONSTER                             1u
#define D2UNIT_OBJECT                              2u
#define D2UNIT_ITEM                                4u
#define D2ITEM_MODE_GROUND                         3u

/* Client ground-label records. */
#define D2CLIENT_ITEM_LABEL_MAX                    32u
#define D2CLIENT_ITEM_LABEL_STRIDE                 0x120u
#define D2CLIENT_ITEM_LABEL_LEFT_OFFSET            0x00u
#define D2CLIENT_ITEM_LABEL_TOP_OFFSET             0x04u
#define D2CLIENT_ITEM_LABEL_RIGHT_OFFSET           0x08u
#define D2CLIENT_ITEM_LABEL_BOTTOM_OFFSET          0x0cu
#define D2CLIENT_ITEM_LABEL_UNIT_OFFSET            0x10u
#define D2CLIENT_ITEM_LABEL_STATE_OFFSET           0x118u
#define D2CLIENT_ITEM_LABEL_GROUND_STATE           5u
#define D2CLIENT_GAME_MODE_SKIP_LABELS              3u

/* Unit hash tables. */
#define D2CLIENT_UNIT_HASH_BUCKET_COUNT            128u

/* ItemTxt and item/stat constants. */
#define D2ITEMTXT_CODE_OFFSET                      0x144u
#define D2ITEM_CODE_3CHAR_MASK                     0x00ffffffu
#define D2ITEM_CODE_GOLD                           0x20646c67u
#define D2ITEM_CODE_GOLD_3CHAR                     0x00646c67u
#define D2STAT_GOLD                                14u

/* Town level IDs. */
#define D2LEVEL_ROGUE_ENCAMPMENT                   1u
#define D2LEVEL_LUT_GHOLEIN                        40u
#define D2LEVEL_KURAST_DOCKS                       75u
#define D2LEVEL_PANDEMONIUM_FORTRESS               103u
#define D2LEVEL_HARROGATH                          109u

/* Item interaction rules verified in D2Game 1.09b. */
#define D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE       4
#define D2ITEM_INTERACT_COLLISION_MASK             0x804

/* Client-to-server item pickup packet. */
#define D2NET_PACKET_PICKUP_ITEM                   0x16u
#define D2NET_PACKET_PICKUP_ITEM_SIZE              13u
#define D2NET_PACKET_PICKUP_UNIT_TYPE_OFFSET       1u
#define D2NET_PACKET_PICKUP_UNIT_ID_OFFSET         5u
#define D2NET_PACKET_PICKUP_RESERVED_OFFSET        9u

/* Export ordinals used by jmod. */
#define D2COMMON_GET_LEVEL_ID_ORDINAL              10057
#define D2COMMON_GET_ROOM_ORDINAL                  10342
#define D2COMMON_TEST_INTERACTION_COLLISION_ORDINAL 10363
#define D2COMMON_UNIT_DISTANCE_ORDINAL             10399
#define D2COMMON_GET_UNIT_STAT_ORDINAL             10519
#define D2COMMON_GET_ITEM_TEXT_ORDINAL             10600
#define D2COMMON_GET_ITEM_QUALITY_ORDINAL          10695
#define D2NET_SEND_PACKET_ORDINAL                  10005
#define D2LANG_LOOKUP_BY_KEY_ORDINAL               10003
#define D2LANG_LOOKUP_BY_ID_ORDINAL                10004

/* English 1.09b rune string IDs. */
#define D2LANG_RUNE_EXPANSION_FIRST_ID             0x28c8u
#define D2LANG_RUNE_EXPANSION_LAST_ID              0x28e8u
#define D2LANG_RUNE_IO_ID                          0x51a6u
#define D2LANG_RUNE_SHAEL_ID                       0x51a8u
#define D2LANG_RUNE_JAH_ID                         0x51aau

#endif
