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
#define D2CLIENT_UI_MODE_OFFSET                    0x120d54u
#define D2CLIENT_UI_MODE_TRADE                     11u
#define D2CLIENT_UI_MODE_STASH                     12u
#define D2CLIENT_UI_MODE_CUBE                      14u
#define D2CLIENT_CURSOR_ITEM_PTR_OFFSET             0x12c2a8u
#define D2CLIENT_UI_INVENTORY_STATE_OFFSET          0x125a38u
#define D2CLIENT_UI_NPCSHOP_STATE_OFFSET            0x125a64u
#define D2CLIENT_UI_SPECIAL_STATE_OFFSET            0x125a6cu
#define D2CLIENT_UI_TRADE_STATE_OFFSET              0x125a90u
#define D2CLIENT_UI_STASH_STATE_OFFSET              0x125a98u
#define D2CLIENT_UI_CUBE_STATE_OFFSET               0x125a9cu
#define D2CLIENT_DIFFICULTY_OFFSET                  0x111d5cu
#define D2CLIENT_INTERACTED_NPC_ID_OFFSET           0x12115du
#define D2CLIENT_INTERACTED_NPC_ACTIVE_OFFSET       0x121161u
#define D2CLIENT_INTERACTED_NPC_CLASS_ID_OFFSET     0x121165u
#define D2CLIENT_ITEM_PRICE_CONTEXT_OFFSET          0x12117bu

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
#define D2CLIENT_FN_IS_EXPANSION_OFFSET            0x0ba00u
#define D2CLIENT_FN_INVENTORY_CLICK_OFFSET         0x40b00u
#define D2CLIENT_FN_DRAW_CURSOR_OFFSET              0xb6570u
#define D2CLIENT_FN_PLAY_SOUND_OFFSET               0xb4360u
#define D2CLIENT_FN_ITEM_ID_PACKET_OFFSET           0x0d360u

/* D2Client patch sites. */
#define D2CLIENT_HOOK_LOOT_LABEL_OFFSET            0x63bf9u
#define D2CLIENT_HOOK_LOOT_HOVER_OFFSET            0x8729eu
#define D2CLIENT_HOOK_LOOT_CURSOR_OFFSET           0x155f4u
#define D2CLIENT_HOOK_LABEL_TOOLTIP_OFFSET         0x872a5u
#define D2CLIENT_HOOK_LABEL_RENDER_FLAG_OFFSET     0x877d2u
#define D2CLIENT_HOOK_LABEL_DRAW_CALL_OFFSET       0x877e5u
#define D2CLIENT_HOOK_INVENTORY_CLICK_OFFSET       0x405ecu
#define D2CLIENT_HOOK_STASH_CLICK_OFFSET           0x40a2fu
#define D2CLIENT_HOOK_TRADE_CLICK_OFFSET           0x40a99u
#define D2CLIENT_HOOK_CUBE_CLICK_OFFSET            0x40976u
#define D2CLIENT_HOOK_BELT_REMOVE_PACKET_OFFSET    0x5ae38u
#define D2CLIENT_HOOK_DRAW_CURSOR_1_OFFSET          0x090d3u
#define D2CLIENT_HOOK_DRAW_CURSOR_2_OFFSET          0x0aa65u
#define D2CLIENT_HOOK_DRAW_CURSOR_3_OFFSET          0x35fffu
#define D2CLIENT_HOOK_DRAW_CURSOR_4_OFFSET          0x3622au
#define D2CLIENT_HOOK_DRAW_CURSOR_5_OFFSET          0x367e1u
#define D2CLIENT_HOOK_DRAW_CURSOR_6_OFFSET          0x36a6au

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
#define D2UNIT_INVENTORY_OFFSET                    0x84u
#define D2UNIT_HASH_NEXT_OFFSET                    0x108u
#define D2UNIT_PLAYER                              0u
#define D2UNIT_MONSTER                             1u
#define D2UNIT_OBJECT                              2u
#define D2UNIT_ITEM                                4u
#define D2ITEM_MODE_STORED                         0u
#define D2ITEM_MODE_GROUND                         3u

/* Inventory/container grid descriptors used by the 1.09b click handler. */
#define D2CLIENT_INVENTORY_GRID_LEFT_OFFSET        0x04u
#define D2CLIENT_INVENTORY_GRID_TOP_OFFSET         0x0cu
#define D2CLIENT_INVENTORY_GRID_CELL_WIDTH_OFFSET  0x14u
#define D2CLIENT_INVENTORY_GRID_CELL_HEIGHT_OFFSET 0x15u
#define D2INVPAGE_INVENTORY                        0u
#define D2INVPAGE_TRADE                            2u
#define D2INVPAGE_CUBE                             3u
#define D2INVPAGE_STASH                            4u

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
#define D2STAT_LEVEL                               12u
#define D2STAT_GOLD                                14u
#define D2ITEM_HORADRIC_CUBE_CLASS_ID              549u

/* Town level IDs. */
#define D2LEVEL_ROGUE_ENCAMPMENT                   1u
#define D2LEVEL_LUT_GHOLEIN                        40u
#define D2LEVEL_KURAST_DOCKS                       75u
#define D2LEVEL_PANDEMONIUM_FORTRESS               103u
#define D2LEVEL_HARROGATH                          109u

/* Item interaction rules verified in D2Game 1.09b. */
#define D2ITEM_IMMEDIATE_PICKUP_MAX_DISTANCE       4
#define D2ITEM_INTERACT_COLLISION_MASK             0x804

/* Client-to-server cursor-item drop packet. */
#define D2NET_PACKET_DROP_ITEM                     0x17u
#define D2NET_PACKET_DROP_ITEM_SIZE                5u
#define D2NET_PACKET_DROP_ITEM_ID_OFFSET           1u

/* Client-to-server grid placement packet. */
#define D2NET_PACKET_INSERT_ITEM                   0x18u
#define D2NET_PACKET_INSERT_ITEM_SIZE              17u
#define D2NET_PACKET_INSERT_ITEM_ID_OFFSET         1u
#define D2NET_PACKET_INSERT_ITEM_X_OFFSET          5u
#define D2NET_PACKET_INSERT_ITEM_Y_OFFSET          9u
#define D2NET_PACKET_INSERT_ITEM_PAGE_OFFSET       13u

/* Client-to-server belt item removal packet. */
#define D2NET_PACKET_REMOVE_BELT_ITEM              0x24u

/* Client-to-server NPC sell packet. */
#define D2NET_PACKET_NPC_SELL                      0x33u
#define D2NET_PACKET_NPC_SELL_SIZE                 17u
#define D2NET_PACKET_NPC_SELL_NPC_ID_OFFSET        1u
#define D2NET_PACKET_NPC_SELL_ITEM_ID_OFFSET       5u
#define D2NET_PACKET_NPC_SELL_BUFFER_OFFSET        9u
#define D2NET_PACKET_NPC_SELL_PRICE_OFFSET         13u
#define D2TRANSACTION_SELL                         1
#define D2SOUND_ITEM_GOLD                          0xddu
#define D2SOUND_CURSOR_ERROR                       3u

/* Client-to-server item pickup packet. */
#define D2NET_PACKET_PICKUP_ITEM                   0x16u
#define D2NET_PACKET_PICKUP_ITEM_SIZE              13u
#define D2NET_PACKET_PICKUP_UNIT_TYPE_OFFSET       1u
#define D2NET_PACKET_PICKUP_UNIT_ID_OFFSET         5u
#define D2NET_PACKET_PICKUP_RESERVED_OFFSET        9u

/* Export ordinals used by jmod. */
#define D2COMMON_GET_LEVEL_ID_ORDINAL              10057
#define D2COMMON_INVENTORY_GET_FREE_POSITION_ORDINAL 10245
#define D2COMMON_INVENTORY_GET_ITEM_FROM_PAGE_ORDINAL 10252
#define D2COMMON_INVENTORY_GET_CURSOR_ITEM_ORDINAL 10262
#define D2COMMON_GET_INVENTORY_RECORD_ID_ORDINAL   10409
#define D2COMMON_GET_ITEM_PAGE_ORDINAL             10719
#define D2COMMON_ITEMS_IS_NOT_QUEST_ORDINAL        10740
#define D2COMMON_GET_TRANSACTION_COST_ORDINAL      10775
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

/* Verify the untouched D2Client sites jmod depends on before using raw offsets. */
int d2_109b_client_matches(void);

#endif
