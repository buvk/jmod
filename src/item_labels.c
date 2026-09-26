#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "item_labels.h"
#include "game_ui.h"
#include "loot_filter.h"
#include "d2_109b.h"

static BYTE show_key_down[256];
static CRITICAL_SECTION labels_lock;
static volatile LONG item_labels_on;

static int is_interactive_target(DWORD type)
{
    return type == D2UNIT_MONSTER || type == D2UNIT_OBJECT;
}

static void refresh_object_target(BYTE *client)
{
    typedef void (__cdecl *update_cursor_fn)(void);
    *(DWORD *)(client + D2CLIENT_TARGET_REFRESH_OFFSET) = 0;
    ((update_cursor_fn)(client + D2CLIENT_FN_UPDATE_CURSOR_TARGET_OFFSET))();
}

static void __cdecl draw_item_labels(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__cdecl *draw_fn)(void);
    typedef void *(__cdecl *selected_unit_fn)(void);
    typedef void (__fastcall *draw_hover_fn)(void *);
    DWORD *selected = (DWORD *)(client + D2CLIENT_TARGET_ID_OFFSET);
    DWORD type = *(DWORD *)(client + D2CLIENT_TARGET_TYPE_OFFSET);
    int had_object = *selected && is_interactive_target(type);
    void *object = NULL;

    ((draw_fn)(client + D2CLIENT_FN_DRAW_ITEM_LABELS_OFFSET))();
    if (had_object && item_labels_on) {
        refresh_object_target(client);
        object = ((selected_unit_fn)(client + D2CLIENT_FN_GET_SELECTED_UNIT_OFFSET))();
        if (object && is_interactive_target(*(DWORD *)object))
            ((draw_hover_fn)(client + D2CLIENT_FN_DRAW_HOVER_OFFSET))(object);
    }
}

/* D2Client's label renderer stores the ground units it drew in this list.
   A selected unit in that list already has its hovered ground label. */
static int ground_label_contains(BYTE *client, const void *selected)
{
    DWORD count = *(const DWORD *)(client + D2CLIENT_ITEM_LABEL_COUNT_OFFSET);
    DWORD i;
    const BYTE *labels = client + D2CLIENT_ITEM_LABELS_OFFSET;
    if (!selected ||
        *(const DWORD *)selected != D2UNIT_ITEM)
        return 0;
    if (count > D2CLIENT_ITEM_LABEL_MAX)
        count = D2CLIENT_ITEM_LABEL_MAX;
    for (i = 0; i < count; ++i)
        if (*(const void *const *)(labels + i * D2CLIENT_ITEM_LABEL_STRIDE +
                                  D2CLIENT_ITEM_LABEL_UNIT_OFFSET) == selected)
            return 1;
    return 0;
}

/* A label entry can outlive the ground unit it pointed at for a frame.
   Validate a cached label pointer against D2Client's live item-unit table
   before dereferencing or selecting it. Pointer comparison itself is safe. */
static int live_item_unit(BYTE *client, const void *candidate)
{
    const BYTE *const *items;
    const BYTE *unit;
    unsigned bucket, visited = 0;

    if (!client || !candidate)
        return 0;

    items = (const BYTE *const *)(client + D2CLIENT_UNIT_HASH_TABLES_OFFSET +
        D2UNIT_ITEM * D2CLIENT_UNIT_HASH_BUCKET_COUNT * sizeof(void *));
    for (bucket = 0; bucket < D2CLIENT_UNIT_HASH_BUCKET_COUNT; ++bucket) {
        for (unit = items[bucket]; unit && visited++ < 4096;
             unit = *(const BYTE *const *)(unit + D2UNIT_HASH_NEXT_OFFSET)) {
            if (unit == candidate)
                return 1;
        }
        if (visited >= 4096)
            break;
    }
    return 0;
}

/* The game can recalculate its world target between drawing a label and
   handling a click. Use the hovered label's unit when clicking its text. */
static void select_clicked_label(BYTE *client)
{
    typedef int (__cdecl *cursor_fn)(void);
    typedef void (__fastcall *select_unit_fn)(void *);
    const BYTE *labels = client + D2CLIENT_ITEM_LABELS_OFFSET;
    DWORD count = *(const DWORD *)(client + D2CLIENT_ITEM_LABEL_COUNT_OFFSET);
    int x = ((cursor_fn)(client + D2CLIENT_FN_CURSOR_X_OFFSET))();
    int y = ((cursor_fn)(client + D2CLIENT_FN_CURSOR_Y_OFFSET))();
    DWORD i;

    if (count > D2CLIENT_ITEM_LABEL_MAX) count = D2CLIENT_ITEM_LABEL_MAX;
    /* Labels draw in list order, so the last hit is the one on top. */
    for (i = count; i > 0; --i) {
        const BYTE *label = labels + (i - 1) * D2CLIENT_ITEM_LABEL_STRIDE;
        void *item = *(void *const *)(label + D2CLIENT_ITEM_LABEL_UNIT_OFFSET);
        if (item &&
            live_item_unit(client, item) &&
            *(const DWORD *)(label + D2CLIENT_ITEM_LABEL_STATE_OFFSET) ==
            D2CLIENT_ITEM_LABEL_GROUND_STATE &&
            x >= *(const int *)(label + D2CLIENT_ITEM_LABEL_LEFT_OFFSET) &&
            x <= *(const int *)(label + D2CLIENT_ITEM_LABEL_RIGHT_OFFSET) &&
            y >= *(const int *)(label + D2CLIENT_ITEM_LABEL_TOP_OFFSET) &&
            y <= *(const int *)(label + D2CLIENT_ITEM_LABEL_BOTTOM_OFFSET)) {
            ((select_unit_fn)(client + D2CLIENT_FN_SELECT_LABEL_TARGET_OFFSET))(item);
            return;
        }
    }
}

static void __fastcall draw_labels_before_tooltip(void *selected)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__fastcall *draw_hover_fn)(void *);
    typedef int (__cdecl *mode_getter_fn)(void);
    if (!game_active()) {
        /* Outside an active game, preserve the original tooltip call only. */
        ((draw_hover_fn)(client + D2CLIENT_FN_DRAW_HOVER_OFFSET))(selected);
        return;
    }
    /* vanilla skips label drawing entirely in game-state 3; preserve that
       guard here since it no longer runs through the original gated site */
    if (item_labels_on && game_menu_open())
        return;
    if (item_labels_on &&
        ((mode_getter_fn)(client + D2CLIENT_FN_GAME_MODE_OFFSET))() !=
            D2CLIENT_GAME_MODE_SKIP_LABELS) {
        draw_item_labels();
        if (ground_label_contains(client, selected))
            return;
    }
    if (selected && *(const DWORD *)selected == D2UNIT_ITEM &&
        !loot_filter_show_item(selected))
        return;
    ((draw_hover_fn)(client + D2CLIENT_FN_DRAW_HOVER_OFFSET))(selected);
}

/* the flag-gated draw_call site now no-ops: labels are drawn from the
   always-runs tooltip call site instead, so the tooltip never gets starved
   by the label toggle */
static void __cdecl labels_already_drawn(void)
{
}

int item_labels_init(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    BYTE *render, *draw_call, *tooltip_call;
    DWORD expected, old_render, old_call, old_tooltip, unused;
    InitializeCriticalSection(&labels_lock);
    if (!client) return 0;
    render = client + D2CLIENT_HOOK_LABEL_RENDER_FLAG_OFFSET;
    draw_call = client + D2CLIENT_HOOK_LABEL_DRAW_CALL_OFFSET;
    tooltip_call = client + D2CLIENT_HOOK_LABEL_TOOLTIP_OFFSET;
    expected = (DWORD)(client + D2CLIENT_ITEM_LABEL_RENDER_FLAG_OFFSET);
    if (render[0] != 0xa1 || *(DWORD *)(render + 1) != expected ||
        draw_call[0] != 0xe8 ||
        *(DWORD *)(draw_call + 1) !=
            (DWORD)(client + D2CLIENT_FN_DRAW_ITEM_LABELS_OFFSET -
                    (draw_call + 5)) ||
        tooltip_call[0] != 0xe8 ||
        *(DWORD *)(tooltip_call + 1) !=
            (DWORD)(client + D2CLIENT_FN_DRAW_HOVER_OFFSET -
                    (tooltip_call + 5)))
        return 0;
    if (!VirtualProtect(render, 5, PAGE_EXECUTE_READWRITE, &old_render))
        return 0;
    if (!VirtualProtect(draw_call, 5, PAGE_EXECUTE_READWRITE, &old_call)) {
        VirtualProtect(render, 5, old_render, &unused);
        return 0;
    }
    if (!VirtualProtect(tooltip_call, 5, PAGE_EXECUTE_READWRITE, &old_tooltip)) {
        VirtualProtect(draw_call, 5, old_call, &unused);
        VirtualProtect(render, 5, old_render, &unused);
        return 0;
    }
    *(DWORD *)(render + 1) = (DWORD)&item_labels_on;
    *(DWORD *)(draw_call + 1) = (DWORD)((BYTE *)labels_already_drawn - (draw_call + 5));
    *(DWORD *)(tooltip_call + 1) =
        (DWORD)((BYTE *)draw_labels_before_tooltip - (tooltip_call + 5));
    FlushInstructionCache(GetCurrentProcess(), render, 5);
    FlushInstructionCache(GetCurrentProcess(), draw_call, 5);
    FlushInstructionCache(GetCurrentProcess(), tooltip_call, 5);
    VirtualProtect(tooltip_call, 5, old_tooltip, &unused);
    VirtualProtect(draw_call, 5, old_call, &unused);
    VirtualProtect(render, 5, old_render, &unused);
    return 1;
}

static int is_show_items_key(WPARAM key)
{
    HMODULE client = GetModuleHandleA("D2Client.dll");
    const BYTE *bindings;
    unsigned i;
    if (!client || key > 0xff) return 0;
    bindings = (const BYTE *)client + D2CLIENT_KEY_BINDINGS_OFFSET;
    for (i = 0; i < D2CLIENT_KEY_BINDING_COUNT; ++i) {
        const BYTE *entry = bindings + i * D2CLIENT_KEY_BINDING_STRIDE;
        if (*(const DWORD *)(entry + D2CLIENT_KEY_BINDING_ACTION_OFFSET) ==
            D2ACTION_SHOW_ITEMS &&
            *(const WORD *)(entry + D2CLIENT_KEY_BINDING_KEY_OFFSET) == key)
            return 1;
    }
    return 0;
}

int item_labels_on_message(MSG *msg, HWND game_window)
{
    unsigned key = (unsigned)msg->wParam;
    if (item_labels_on && msg->hwnd == game_window &&
        msg->message == WM_LBUTTONDOWN &&
        GetForegroundWindow() == game_window) {
        BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
        /* D2Client's world-target functions require an active game.
           Calling them from the front end can dereference stale target state. */
        if (game_active() && !game_menu_open()) {
            if (*(DWORD *)(client + D2CLIENT_TARGET_ID_OFFSET) &&
                is_interactive_target(
                    *(DWORD *)(client + D2CLIENT_TARGET_TYPE_OFFSET)))
                refresh_object_target(client);
            select_clicked_label(client);
        }
    }
    if (key >= 256 || msg->hwnd != game_window || !is_show_items_key(key))
        return 0;

    /* Track the binding outside a game without consuming it. If the key is
       held while a game is created, its first repeat must not toggle labels. */
    if (!game_active()) {
        if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
            EnterCriticalSection(&labels_lock);
            show_key_down[key] = 1;
            LeaveCriticalSection(&labels_lock);
        } else if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
            EnterCriticalSection(&labels_lock);
            show_key_down[key] = 0;
            LeaveCriticalSection(&labels_lock);
        }
        return 0;
    }

    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        if (game_menu_open()) {
            /* Remember a press made in the menu so a repeat after closing
               it cannot toggle the labels without a fresh key press. */
            EnterCriticalSection(&labels_lock);
            show_key_down[key] = 1;
            LeaveCriticalSection(&labels_lock);
            return 1;
        }
        if (GetForegroundWindow() == game_window) {
            EnterCriticalSection(&labels_lock);
            if (!show_key_down[key]) {
                show_key_down[key] = 1;
                InterlockedExchange(&item_labels_on, !item_labels_on);
            }
            LeaveCriticalSection(&labels_lock);
            return 1;
        }
    } else if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
        int was_down;
        EnterCriticalSection(&labels_lock);
        was_down = show_key_down[key];
        show_key_down[key] = 0;
        LeaveCriticalSection(&labels_lock);
        return was_down;
    }
    return 0;
}

void item_labels_resync_keys(void)
{
    unsigned key;
    EnterCriticalSection(&labels_lock);
    for (key = 0; key < 256; ++key)
        show_key_down[key] = (GetAsyncKeyState((int)key) & 0x8000) != 0;
    LeaveCriticalSection(&labels_lock);
}
