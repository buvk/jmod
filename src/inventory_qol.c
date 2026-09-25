#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "inventory_qol.h"
#include "d2_109b.h"
#include "game_ui.h"

#define INVENTORY_QOL_MESSAGE (WM_APP + 0x314)
#define QUICK_MOVE_TIMEOUT_MS 1500u

typedef int (__fastcall *inventory_click_fn)(
    void *, void *, int, int, DWORD, const BYTE *, DWORD);
typedef int (__stdcall *inventory_get_free_position_fn)(
    void *, void *, int, int *, int *, BYTE);
typedef void *(__stdcall *inventory_get_item_from_page_fn)(
    void *, int, int, int *, int *, int, BYTE);
typedef void *(__stdcall *inventory_get_cursor_item_fn)(void *);
typedef int (__stdcall *get_inventory_record_id_fn)(void *, int, int);
typedef BYTE (__stdcall *get_item_page_fn)(void *);
typedef DWORD (__stdcall *send_packet_fn)(DWORD, const BYTE *, DWORD);
typedef DWORD (__fastcall *is_expansion_fn)(void);
typedef int (__stdcall *get_unit_stat_fn)(const void *, DWORD);
typedef int (__stdcall *is_not_quest_item_fn)(const void *);
typedef int (__stdcall *get_transaction_cost_fn)(
    void *, int, void *, int, int, int);
typedef void (__cdecl *cursor_draw_fn)(void);
typedef int (__fastcall *play_sound_fn)(DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void (__fastcall *item_id_packet_fn)(DWORD, DWORD);

typedef struct PendingQuickMove {
    int active;
    int message_queued;
    DWORD generation;
    DWORD started_at;
    DWORD item_id;
    DWORD target_page;
    DWORD ui_mode;
    int target_record;
    int drop_to_ground;
    void *inventory;
} PendingQuickMove;

typedef struct QuickMoveVisual {
    int active;
    int seen_on_cursor;
    DWORD started_at;
    DWORD item_id;
} QuickMoveVisual;

static const D2ModConfig *config;
static BYTE *client_base;
static inventory_click_fn original_inventory_click;
static inventory_get_free_position_fn get_free_position;
static inventory_get_item_from_page_fn get_item_from_page;
static inventory_get_cursor_item_fn get_cursor_item;
static get_inventory_record_id_fn get_inventory_record_id;
static get_item_page_fn get_item_page;
static send_packet_fn send_packet;
static is_expansion_fn is_expansion;
static get_unit_stat_fn get_unit_stat;
static is_not_quest_item_fn is_not_quest_item;
static get_transaction_cost_fn get_transaction_cost;
static cursor_draw_fn original_cursor_draw;
static play_sound_fn play_sound;
static item_id_packet_fn original_item_id_packet;
static CRITICAL_SECTION state_lock;
static int state_lock_ready;
static PendingQuickMove pending;
static QuickMoveVisual visual;

static int call_target_matches(const BYTE *site, const BYTE *expected)
{
    LONG relative;

    if (site[0] != 0xe8)
        return 0;
    CopyMemory(&relative, site + 1, sizeof(relative));
    return site + 5 + relative == expected;
}

static int screen_to_grid(const BYTE *grid, int x, int y,
                          int *grid_x, int *grid_y)
{
    int left, top;
    BYTE cell_width, cell_height;

    if (!grid || !grid_x || !grid_y)
        return 0;

    left = *(const int *)(grid + D2CLIENT_INVENTORY_GRID_LEFT_OFFSET);
    top = *(const int *)(grid + D2CLIENT_INVENTORY_GRID_TOP_OFFSET);
    cell_width = *(const BYTE *)(
        grid + D2CLIENT_INVENTORY_GRID_CELL_WIDTH_OFFSET);
    cell_height = *(const BYTE *)(
        grid + D2CLIENT_INVENTORY_GRID_CELL_HEIGHT_OFFSET);
    if (!cell_width || !cell_height || x < left || y < top)
        return 0;

    *grid_x = (x - left) / cell_width;
    *grid_y = (y - top) / cell_height;
    return 1;
}

static DWORD current_ui_mode(void)
{
    return client_base ?
        *(const DWORD *)(client_base + D2CLIENT_UI_MODE_OFFSET) : 0;
}

static int quick_move_target(DWORD source_page, DWORD *target_page,
                             DWORD *ui_mode)
{
    DWORD mode;

    if (!target_page || !ui_mode)
        return 0;

    mode = current_ui_mode();
    if (mode == D2CLIENT_UI_MODE_TRADE) {
        if (source_page == D2INVPAGE_INVENTORY)
            *target_page = D2INVPAGE_TRADE;
        else if (source_page == D2INVPAGE_TRADE)
            *target_page = D2INVPAGE_INVENTORY;
        else
            return 0;
    } else if (mode == D2CLIENT_UI_MODE_STASH) {
        if (source_page == D2INVPAGE_INVENTORY)
            *target_page = D2INVPAGE_STASH;
        else if (source_page == D2INVPAGE_STASH)
            *target_page = D2INVPAGE_INVENTORY;
        else
            return 0;
    } else if (mode == D2CLIENT_UI_MODE_CUBE) {
        if (source_page == D2INVPAGE_INVENTORY)
            *target_page = D2INVPAGE_CUBE;
        else if (source_page == D2INVPAGE_CUBE)
            *target_page = D2INVPAGE_INVENTORY;
        else
            return 0;
    } else {
        return 0;
    }

    *ui_mode = mode;
    return 1;
}

static int pending_active_locked(void)
{
    return pending.active &&
        (DWORD)(GetTickCount() - pending.started_at) <= QUICK_MOVE_TIMEOUT_MS;
}

static void clear_pending_locked(void)
{
    if (pending.active || pending.message_queued)
        ++pending.generation;
    pending.active = 0;
    pending.message_queued = 0;
}

static void clear_visual_locked(void)
{
    visual.active = 0;
    visual.seen_on_cursor = 0;
    visual.started_at = 0;
    visual.item_id = 0;
}

static int quick_drop_modifier_down(void)
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetKeyState(VK_SHIFT) & 0x8000) == 0;
}

static void arm_belt_quick_drop(DWORD item_id)
{
    void *player, *inventory;

    if (!config || !config->inventory_quick_move || !state_lock_ready ||
        !client_base || !quick_drop_modifier_down())
        return;

    player = *(void **)(client_base + D2CLIENT_PLAYER_PTR_OFFSET);
    if (!player)
        return;

    /* Exact 1.09b UnitAny layout: current player's inventory is +0x84. */
    inventory = *(void **)((BYTE *)player + D2UNIT_INVENTORY_OFFSET);
    if (!inventory || get_cursor_item(inventory))
        return;

    EnterCriticalSection(&state_lock);
    if (pending.active && !pending_active_locked()) {
        clear_pending_locked();
        clear_visual_locked();
    }
    if (!pending.active) {
        pending.active = 1;
        pending.message_queued = 0;
        ++pending.generation;
        pending.started_at = GetTickCount();
        pending.item_id = item_id;
        pending.target_page = 0;
        pending.ui_mode = 0;
        pending.target_record = -1;
        pending.drop_to_ground = 1;
        pending.inventory = inventory;

        visual.active = 1;
        visual.seen_on_cursor = 0;
        visual.started_at = pending.started_at;
        visual.item_id = item_id;
    }
    LeaveCriticalSection(&state_lock);
}

static void __fastcall belt_remove_packet_hook(DWORD packet_id, DWORD item_id)
{
    /* Preserve the exact vanilla 0x24 send first. This hook site is reached
       only after D2Client has resolved an occupied belt slot for removal. */
    original_item_id_packet(packet_id, item_id);

    if ((BYTE)packet_id == D2NET_PACKET_REMOVE_BELT_ITEM)
        arm_belt_quick_drop(item_id);
}

void inventory_qol_reset(void)
{
    if (!state_lock_ready)
        return;

    EnterCriticalSection(&state_lock);
    clear_pending_locked();
    clear_visual_locked();
    LeaveCriticalSection(&state_lock);
}

int inventory_qol_pending(void)
{
    int active = 0;

    if (!state_lock_ready)
        return 0;

    EnterCriticalSection(&state_lock);
    if (pending.active && !pending_active_locked()) {
        clear_pending_locked();
        clear_visual_locked();
    } else if (pending.active) {
        active = 1;
    }
    LeaveCriticalSection(&state_lock);
    return active;
}

static void send_insert_item(DWORD item_id, int x, int y, DWORD page)
{
    BYTE packet[D2NET_PACKET_INSERT_ITEM_SIZE] = { 0 };

    packet[0] = D2NET_PACKET_INSERT_ITEM;
    CopyMemory(packet + D2NET_PACKET_INSERT_ITEM_ID_OFFSET,
               &item_id, sizeof(item_id));
    CopyMemory(packet + D2NET_PACKET_INSERT_ITEM_X_OFFSET,
               &x, sizeof(x));
    CopyMemory(packet + D2NET_PACKET_INSERT_ITEM_Y_OFFSET,
               &y, sizeof(y));
    CopyMemory(packet + D2NET_PACKET_INSERT_ITEM_PAGE_OFFSET,
               &page, sizeof(page));
    (void)send_packet(0, packet, sizeof(packet));
}

static void send_drop_item(DWORD item_id)
{
    BYTE packet[D2NET_PACKET_DROP_ITEM_SIZE] = { 0 };

    packet[0] = D2NET_PACKET_DROP_ITEM;
    CopyMemory(packet + D2NET_PACKET_DROP_ITEM_ID_OFFSET,
               &item_id, sizeof(item_id));
    (void)send_packet(0, packet, sizeof(packet));
}

static int merchant_shop_open(void)
{
    return client_base &&
        *(const DWORD *)(client_base + D2CLIENT_UI_NPCSHOP_STATE_OFFSET) != 0 &&
        *(const DWORD *)(client_base + D2CLIENT_INTERACTED_NPC_ACTIVE_OFFSET) != 0 &&
        *(const DWORD *)(client_base + D2CLIENT_INTERACTED_NPC_ID_OFFSET) != 0;
}

static int normal_inventory_open(void)
{
    if (!client_base)
        return 0;

    /* D2Client's UI-state array starts at +0x125a34. Inventory is index 1;
       exclude the other panels that expose or overlap inventory handling. */
    return *(const DWORD *)(client_base + D2CLIENT_UI_INVENTORY_STATE_OFFSET) != 0 &&
        *(const DWORD *)(client_base + D2CLIENT_UI_NPCSHOP_STATE_OFFSET) == 0 &&
        *(const DWORD *)(client_base + D2CLIENT_UI_SPECIAL_STATE_OFFSET) == 0 &&
        *(const DWORD *)(client_base + D2CLIENT_UI_TRADE_STATE_OFFSET) == 0 &&
        *(const DWORD *)(client_base + D2CLIENT_UI_STASH_STATE_OFFSET) == 0 &&
        *(const DWORD *)(client_base + D2CLIENT_UI_CUBE_STATE_OFFSET) == 0;
}

static void send_sell_item(void *player, void *item)
{
    BYTE packet[D2NET_PACKET_NPC_SELL_SIZE] = { 0 };
    DWORD npc_id, item_id, buffer_type = 0;
    DWORD price;
    int npc_class_id, difficulty, player_level;
    void *price_context;

    npc_id = *(const DWORD *)(
        client_base + D2CLIENT_INTERACTED_NPC_ID_OFFSET);
    npc_class_id = *(const int *)(
        client_base + D2CLIENT_INTERACTED_NPC_CLASS_ID_OFFSET);
    difficulty = *(const BYTE *)(
        client_base + D2CLIENT_DIFFICULTY_OFFSET);
    price_context = *(void **)(
        client_base + D2CLIENT_ITEM_PRICE_CONTEXT_OFFSET);
    item_id = *(const DWORD *)((const BYTE *)item + D2UNIT_ID_OFFSET);
    player_level = get_unit_stat(player, D2STAT_LEVEL);

    /* 1.09b's #10775 argument order was verified from D2Client's sell path:
       item, difficulty, price context, NPC class, transaction type, level. */
    price = (DWORD)get_transaction_cost(
        item, difficulty, price_context, npc_class_id,
        D2TRANSACTION_SELL, player_level);

    packet[0] = D2NET_PACKET_NPC_SELL;
    CopyMemory(packet + D2NET_PACKET_NPC_SELL_NPC_ID_OFFSET,
               &npc_id, sizeof(npc_id));
    CopyMemory(packet + D2NET_PACKET_NPC_SELL_ITEM_ID_OFFSET,
               &item_id, sizeof(item_id));
    CopyMemory(packet + D2NET_PACKET_NPC_SELL_BUFFER_OFFSET,
               &buffer_type, sizeof(buffer_type));
    CopyMemory(packet + D2NET_PACKET_NPC_SELL_PRICE_OFFSET,
               &price, sizeof(price));
    if (send_packet(0, packet, sizeof(packet)) && play_sound)
        (void)play_sound(D2SOUND_ITEM_GOLD, 0, 0, 0, 0);
}

static int try_quick_sell(void *player, void *inventory,
                          int mouse_x, int mouse_y, DWORD mouse_flags,
                          const BYTE *source_grid, DWORD source_page)
{
    void *item;
    int source_x, source_y, item_x, item_y, source_record;

    if (!config || !config->inventory_quick_move ||
        !(mouse_flags & MK_CONTROL) || (mouse_flags & MK_SHIFT) ||
        source_page != D2INVPAGE_INVENTORY || !merchant_shop_open())
        return 0;

    /* Ctrl+click is owned by Quick Sell while the real store panel is open.
       Never fall through to vanilla pickup for an unsellable/empty slot. */
    if (!player || !inventory || get_cursor_item(inventory))
        return 1;

    if (!screen_to_grid(source_grid, mouse_x, mouse_y,
                        &source_x, &source_y))
        return 1;

    source_record = get_inventory_record_id(
        player, D2INVPAGE_INVENTORY, is_expansion() != 0);
    if (source_record < 0)
        return 1;

    item_x = item_y = 0;
    item = get_item_from_page(inventory, source_x, source_y,
                              &item_x, &item_y, source_record,
                              D2INVPAGE_INVENTORY);
    if (!item ||
        *(const DWORD *)item != D2UNIT_ITEM ||
        *(const DWORD *)((const BYTE *)item + D2UNIT_MODE_OFFSET) !=
            D2ITEM_MODE_STORED ||
        get_item_page(item) != D2INVPAGE_INVENTORY)
        return 1;

    /* Vanilla 1.09b performs this exact check immediately after computing
       sell value; quest/unsellable items stay in the inventory. */
    if (!is_not_quest_item(item))
        return 1;

    send_sell_item(player, item);
    return 1;
}

static int begin_quick_move(void *player, void *inventory,
                            int mouse_x, int mouse_y, DWORD mouse_flags,
                            const BYTE *source_grid, DWORD source_page)
{
    void *item;
    int source_x, source_y, item_x, item_y;
    int source_record, target_record = -1;
    int free_x, free_y;
    int drop_to_ground = 0;
    DWORD target_page = 0, ui_mode = 0;

    if (!config || !config->inventory_quick_move ||
        !(mouse_flags & MK_CONTROL) || (mouse_flags & MK_SHIFT))
        return 0;

    if (!player || !inventory || get_cursor_item(inventory))
        return 0;

    if (!quick_move_target(source_page, &target_page, &ui_mode)) {
        /* With only the ordinary inventory panel open, Ctrl+click drops the
           item. Special inventory-facing UIs are deliberately excluded. */
        if (source_page != D2INVPAGE_INVENTORY ||
            !normal_inventory_open())
            return 0;
        drop_to_ground = 1;
    }

    if (!screen_to_grid(source_grid, mouse_x, mouse_y,
                        &source_x, &source_y))
        return 1;

    source_record = get_inventory_record_id(
        player, (int)source_page, is_expansion() != 0);
    if (source_record < 0)
        return 1;

    item_x = item_y = 0;
    item = get_item_from_page(inventory, source_x, source_y,
                              &item_x, &item_y, source_record,
                              (BYTE)source_page);
    if (!item)
        return 1;

    if (*(const DWORD *)item != D2UNIT_ITEM ||
        *(const DWORD *)((const BYTE *)item + D2UNIT_MODE_OFFSET) !=
            D2ITEM_MODE_STORED ||
        get_item_page(item) != (BYTE)source_page)
        return 1;

    if (!drop_to_ground) {
        /* The open container decides the destination: stash or Horadric Cube.
           Never try to put the Cube item inside itself. */
        if (target_page == D2INVPAGE_CUBE &&
            *(const DWORD *)((const BYTE *)item + D2UNIT_CLASS_ID_OFFSET) ==
                D2ITEM_HORADRIC_CUBE_CLASS_ID)
            return 1;

        target_record = get_inventory_record_id(
            player, (int)target_page, is_expansion() != 0);
        if (target_record < 0)
            return 1;

        /* Do not pick the source item up if the opposite container is already
           full. D2Common uses the same grid rules the game uses for placement. */
        free_x = free_y = 0;
        if (!get_free_position(inventory, item, target_record,
                               &free_x, &free_y, (BYTE)target_page)) {
            if (play_sound)
                (void)play_sound(D2SOUND_CURSOR_ERROR, 0, 0, 0, 0);
            return 1;
        }
    }

    EnterCriticalSection(&state_lock);
    if (pending_active_locked()) {
        LeaveCriticalSection(&state_lock);
        return 1;
    }
    pending.active = 1;
    pending.message_queued = 0;
    ++pending.generation;
    pending.started_at = GetTickCount();
    pending.item_id = *(const DWORD *)((const BYTE *)item + D2UNIT_ID_OFFSET);
    pending.target_page = target_page;
    pending.ui_mode = ui_mode;
    pending.target_record = target_record;
    pending.drop_to_ground = drop_to_ground;
    pending.inventory = inventory;
    visual.active = 1;
    visual.seen_on_cursor = 0;
    visual.started_at = pending.started_at;
    visual.item_id = pending.item_id;
    LeaveCriticalSection(&state_lock);

    return 2;
}

static int __fastcall inventory_click_hook(
    void *player, void *inventory, int mouse_x, int mouse_y,
    DWORD mouse_flags, const BYTE *grid, DWORD source_page)
{
    int action;

    if (inventory_qol_pending()) {
        /* Never overlap two remove-to-cursor requests. A normal click cancels
           the automatic second half and falls back to vanilla behavior. */
        if (mouse_flags & MK_CONTROL)
            return 1;
        inventory_qol_reset();
    }

    if (try_quick_sell(player, inventory, mouse_x, mouse_y,
                       mouse_flags, grid, source_page))
        return 1;

    action = begin_quick_move(player, inventory, mouse_x, mouse_y,
                              mouse_flags, grid, source_page);
    if (action == 1)
        return 1;

    if (action == 2) {
        /* v1 sent 0x19 itself and skipped this path. That let D2Game move the
           item while D2Client still had stale cursor/grid state. Let vanilla
           perform the source click and its normal 0x19 transition instead. */
        return original_inventory_click(
            player, inventory, mouse_x, mouse_y,
            mouse_flags & ~MK_CONTROL, grid, source_page);
    }

    return original_inventory_click(
        player, inventory, mouse_x, mouse_y,
        mouse_flags, grid, source_page);
}

static int finish_pending(DWORD generation)
{
    PendingQuickMove move;
    void *cursor_item;
    int free_x, free_y;

    EnterCriticalSection(&state_lock);
    pending.message_queued = 0;
    if (!pending_active_locked() || pending.generation != generation) {
        if (pending.active && !pending_active_locked())
            clear_pending_locked();
        LeaveCriticalSection(&state_lock);
        return 1;
    }
    move = pending;
    LeaveCriticalSection(&state_lock);

    if (!game_active()) {
        inventory_qol_reset();
        return 1;
    }

    /* Container moves keep their strict UI-mode guard. A ground drop was
       already validated when it started; exact cursor-item identity below is
       the authoritative completion guard. */
    if (!move.drop_to_ground && current_ui_mode() != move.ui_mode) {
        inventory_qol_reset();
        return 1;
    }

    cursor_item = get_cursor_item(move.inventory);
    if (!cursor_item)
        return 1;

    if (*(const DWORD *)cursor_item != D2UNIT_ITEM ||
        *(const DWORD *)((const BYTE *)cursor_item + D2UNIT_ID_OFFSET) !=
            move.item_id) {
        /* Never place/drop whatever happens to be on the cursor later. */
        inventory_qol_reset();
        return 1;
    }

    if (move.drop_to_ground) {
        /* Keep the same safe ordering as container quick-move: vanilla 0x19
           first, then only after this exact item is confirmed on the cursor
           send the vanilla 0x17 ground-drop request. */
        EnterCriticalSection(&state_lock);
        clear_pending_locked();
        LeaveCriticalSection(&state_lock);
        send_drop_item(move.item_id);
        return 1;
    }

    /* The server has now completed vanilla 0x19 and the client agrees that
       this exact item is on the cursor. Recompute the free cell, then send
       only the vanilla 0x18 placement request. */
    free_x = free_y = 0;
    if (!get_free_position(move.inventory, cursor_item, move.target_record,
                           &free_x, &free_y, (BYTE)move.target_page)) {
        /* Destination changed while the remove request was in flight. Leave
           the item safely on the cursor for normal manual placement. */
        inventory_qol_reset();
        return 1;
    }

    /* The transfer state can finish now, but keep the render suppression
       alive until D2Client removes this exact item from its cursor state. */
    EnterCriticalSection(&state_lock);
    clear_pending_locked();
    LeaveCriticalSection(&state_lock);
    send_insert_item(move.item_id, free_x, free_y, move.target_page);
    return 1;
}

int inventory_qol_on_message(MSG *msg, HWND game_window)
{
    if (!state_lock_ready || !msg ||
        msg->message != INVENTORY_QOL_MESSAGE ||
        msg->hwnd != game_window)
        return 0;

    return finish_pending((DWORD)msg->wParam);
}

void inventory_qol_poll(HWND game_window, int hooked)
{
    DWORD generation = 0;
    int post = 0;

    if (!state_lock_ready)
        return;

    EnterCriticalSection(&state_lock);
    if (pending.active && !pending_active_locked()) {
        clear_pending_locked();
        clear_visual_locked();
    } else if (pending.active && hooked && game_window &&
               !pending.message_queued) {
        pending.message_queued = 1;
        generation = pending.generation;
        post = 1;
    }
    LeaveCriticalSection(&state_lock);

    if (post &&
        !PostMessageA(game_window, INVENTORY_QOL_MESSAGE,
                      (WPARAM)generation, 0)) {
        EnterCriticalSection(&state_lock);
        if (pending.generation == generation)
            pending.message_queued = 0;
        LeaveCriticalSection(&state_lock);
    }
}

static int hide_quick_move_cursor_item(void)
{
    const BYTE *item;
    int hide = 0;

    if (!state_lock_ready || !client_base)
        return 0;

    item = *(const BYTE *const *)(
        client_base + D2CLIENT_CURSOR_ITEM_PTR_OFFSET);

    EnterCriticalSection(&state_lock);
    if (visual.active) {
        if ((DWORD)(GetTickCount() - visual.started_at) >
                QUICK_MOVE_TIMEOUT_MS) {
            clear_visual_locked();
        } else if (item &&
                   *(const DWORD *)(item + D2UNIT_TYPE_OFFSET) ==
                       D2UNIT_ITEM &&
                   *(const DWORD *)(item + D2UNIT_ID_OFFSET) ==
                       visual.item_id) {
            visual.seen_on_cursor = 1;
            hide = 1;
        } else if (visual.seen_on_cursor) {
            /* Once the exact item has appeared on the cursor, the first frame
               where it is gone means the placement response has caught up. */
            clear_visual_locked();
        }
    }
    LeaveCriticalSection(&state_lock);
    return hide;
}

static void __cdecl cursor_draw_hook(void)
{
    void **cursor_item_slot;
    void *saved_item;

    if (!hide_quick_move_cursor_item()) {
        original_cursor_draw();
        return;
    }

    /*
       D2Client's cursor renderer draws either the held item or, when the
       cursor-item pointer is NULL, the normal pointer. Keep the real quick-
       move state intact and hide the item only for this synchronous draw.
    */
    cursor_item_slot = (void **)(
        client_base + D2CLIENT_CURSOR_ITEM_PTR_OFFSET);
    saved_item = *cursor_item_slot;
    *cursor_item_slot = NULL;
    original_cursor_draw();
    *cursor_item_slot = saved_item;
}

static int patch_call(BYTE *site, const void *replacement)
{
    DWORD old_protect, unused;
    LONG relative;

    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old_protect))
        return 0;

    relative = (LONG)((const BYTE *)replacement - (site + 5));
    site[0] = 0xe8;
    CopyMemory(site + 1, &relative, sizeof(relative));
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualProtect(site, 5, old_protect, &unused);
    return 1;
}

static int resolve_functions(void)
{
    HMODULE common = GetModuleHandleA("D2Common.dll");
    HMODULE net = GetModuleHandleA("D2Net.dll");
    union { FARPROC raw; inventory_get_free_position_fn typed; } free_pos;
    union { FARPROC raw; inventory_get_item_from_page_fn typed; } item_from_page;
    union { FARPROC raw; inventory_get_cursor_item_fn typed; } cursor_item;
    union { FARPROC raw; get_inventory_record_id_fn typed; } record_id;
    union { FARPROC raw; get_item_page_fn typed; } item_page;
    union { FARPROC raw; get_unit_stat_fn typed; } unit_stat;
    union { FARPROC raw; is_not_quest_item_fn typed; } not_quest;
    union { FARPROC raw; get_transaction_cost_fn typed; } transaction_cost;
    union { FARPROC raw; send_packet_fn typed; } packet;

    if (!common || !net)
        return 0;

    free_pos.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(
            D2COMMON_INVENTORY_GET_FREE_POSITION_ORDINAL));
    item_from_page.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(
            D2COMMON_INVENTORY_GET_ITEM_FROM_PAGE_ORDINAL));
    cursor_item.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(
            D2COMMON_INVENTORY_GET_CURSOR_ITEM_ORDINAL));
    record_id.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(
            D2COMMON_GET_INVENTORY_RECORD_ID_ORDINAL));
    item_page.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_GET_ITEM_PAGE_ORDINAL));
    unit_stat.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_GET_UNIT_STAT_ORDINAL));
    not_quest.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_ITEMS_IS_NOT_QUEST_ORDINAL));
    transaction_cost.raw = GetProcAddress(
        common, MAKEINTRESOURCEA(D2COMMON_GET_TRANSACTION_COST_ORDINAL));
    packet.raw = GetProcAddress(
        net, MAKEINTRESOURCEA(D2NET_SEND_PACKET_ORDINAL));

    if (!free_pos.raw || !item_from_page.raw || !cursor_item.raw ||
        !record_id.raw || !item_page.raw || !unit_stat.raw ||
        !not_quest.raw || !transaction_cost.raw || !packet.raw)
        return 0;

    get_free_position = free_pos.typed;
    get_item_from_page = item_from_page.typed;
    get_cursor_item = cursor_item.typed;
    get_inventory_record_id = record_id.typed;
    get_item_page = item_page.typed;
    get_unit_stat = unit_stat.typed;
    is_not_quest_item = not_quest.typed;
    get_transaction_cost = transaction_cost.typed;
    send_packet = packet.typed;
    return 1;
}

int inventory_qol_init(const D2ModConfig *options)
{
    BYTE *inventory_site, *stash_site, *cube_site, *trade_site, *belt_remove_site, *original;
    BYTE *draw_sites[6];
    DWORD old_inventory, old_stash, unused;
    DWORD i;
    LONG relative;

    if (!options)
        return 0;
    if (!options->inventory_quick_move)
        return 1;

    client_base = (BYTE *)GetModuleHandleA("D2Client.dll");
    if (!client_base || !resolve_functions())
        return 0;

    original = client_base + D2CLIENT_FN_INVENTORY_CLICK_OFFSET;
    inventory_site = client_base + D2CLIENT_HOOK_INVENTORY_CLICK_OFFSET;
    stash_site = client_base + D2CLIENT_HOOK_STASH_CLICK_OFFSET;
    cube_site = client_base + D2CLIENT_HOOK_CUBE_CLICK_OFFSET;
    trade_site = client_base + D2CLIENT_HOOK_TRADE_CLICK_OFFSET;
    belt_remove_site = client_base + D2CLIENT_HOOK_BELT_REMOVE_PACKET_OFFSET;
    draw_sites[0] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_1_OFFSET;
    draw_sites[1] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_2_OFFSET;
    draw_sites[2] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_3_OFFSET;
    draw_sites[3] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_4_OFFSET;
    draw_sites[4] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_5_OFFSET;
    draw_sites[5] = client_base + D2CLIENT_HOOK_DRAW_CURSOR_6_OFFSET;
    if (!call_target_matches(inventory_site, original) ||
        !call_target_matches(stash_site, original) ||
        !call_target_matches(cube_site, original) ||
        !call_target_matches(trade_site, original) ||
        !call_target_matches(
            belt_remove_site,
            client_base + D2CLIENT_FN_ITEM_ID_PACKET_OFFSET))
        return 0;
    for (i = 0; i < 6; ++i) {
        if (!call_target_matches(
                draw_sites[i],
                client_base + D2CLIENT_FN_DRAW_CURSOR_OFFSET))
            return 0;
    }

    config = options;
    original_inventory_click = (inventory_click_fn)original;
    original_cursor_draw = (cursor_draw_fn)(
        client_base + D2CLIENT_FN_DRAW_CURSOR_OFFSET);
    play_sound = (play_sound_fn)(
        client_base + D2CLIENT_FN_PLAY_SOUND_OFFSET);
    original_item_id_packet = (item_id_packet_fn)(
        client_base + D2CLIENT_FN_ITEM_ID_PACKET_OFFSET);
    is_expansion = (is_expansion_fn)(
        client_base + D2CLIENT_FN_IS_EXPANSION_OFFSET);

    if (!VirtualProtect(inventory_site, 5, PAGE_EXECUTE_READWRITE,
                        &old_inventory))
        return 0;
    if (!VirtualProtect(stash_site, 5, PAGE_EXECUTE_READWRITE, &old_stash)) {
        VirtualProtect(inventory_site, 5, old_inventory, &unused);
        return 0;
    }

    InitializeCriticalSection(&state_lock);
    state_lock_ready = 1;
    ZeroMemory(&pending, sizeof(pending));
    ZeroMemory(&visual, sizeof(visual));

    relative = (LONG)((BYTE *)inventory_click_hook - (inventory_site + 5));
    inventory_site[0] = 0xe8;
    CopyMemory(inventory_site + 1, &relative, sizeof(relative));

    relative = (LONG)((BYTE *)inventory_click_hook - (stash_site + 5));
    stash_site[0] = 0xe8;
    CopyMemory(stash_site + 1, &relative, sizeof(relative));

    FlushInstructionCache(GetCurrentProcess(), inventory_site, 5);
    FlushInstructionCache(GetCurrentProcess(), stash_site, 5);
    VirtualProtect(stash_site, 5, old_stash, &unused);
    VirtualProtect(inventory_site, 5, old_inventory, &unused);

    if (!patch_call(cube_site, inventory_click_hook) ||
        !patch_call(trade_site, inventory_click_hook) ||
        !patch_call(belt_remove_site, belt_remove_packet_hook))
        return 0;

    for (i = 0; i < 6; ++i) {
        if (!patch_call(draw_sites[i], cursor_draw_hook))
            return 0;
    }
    return 1;
}
