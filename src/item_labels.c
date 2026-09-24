#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "item_labels.h"

static BYTE show_key_down[256];
static CRITICAL_SECTION labels_lock;
static volatile LONG item_labels_on;

static int is_interactive_target(DWORD type)
{
    return type == 1 || type == 2;
}

static void refresh_object_target(BYTE *client)
{
    typedef void (__cdecl *update_cursor_fn)(void);
    *(DWORD *)(client + 0x116dd4) = 0;
    ((update_cursor_fn)(client + 0x14fc0))();
}

static void __cdecl draw_item_labels(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__cdecl *draw_fn)(void);
    typedef void *(__cdecl *selected_unit_fn)(void);
    typedef void (__fastcall *draw_hover_fn)(void *);
    DWORD *selected = (DWORD *)(client + 0x116dd0);
    DWORD type = *(DWORD *)(client + 0x116db8);
    int had_object = *selected && is_interactive_target(type);
    void *object = NULL;

    ((draw_fn)(client + 0x63b60))();
    if (had_object && item_labels_on) {
        refresh_object_target(client);
        object = ((selected_unit_fn)(client + 0x14cf0))();
        if (object && is_interactive_target(*(DWORD *)object))
            ((draw_hover_fn)(client + 0x861c0))(object);
    }
}

/* D2Client's label renderer stores the ground units it drew in this list.
   A selected unit in that list already has its hovered ground label. */
static int ground_label_contains(BYTE *client, const void *selected)
{
    DWORD count = *(const DWORD *)(client + 0x124890);
    DWORD i;
    const BYTE *labels = client + 0x122490;
    if (!selected || *(const DWORD *)selected != 4)
        return 0;
    if (count > 32)
        count = 32;
    for (i = 0; i < count; ++i)
        if (*(const void *const *)(labels + i * 0x120 + 0x10) == selected)
            return 1;
    return 0;
}

static void __fastcall draw_labels_before_tooltip(void *selected)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__fastcall *draw_hover_fn)(void *);
    typedef int (__cdecl *mode_getter_fn)(void);
    /* vanilla skips label drawing entirely in game-state 3; preserve that
       guard here since it no longer runs through the original gated site */
    if (item_labels_on && ((mode_getter_fn)(client + 0x14a20))() != 3) {
        draw_item_labels();
        if (ground_label_contains(client, selected))
            return;
    }
    ((draw_hover_fn)(client + 0x861c0))(selected);
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
    render = client + 0x877d2;
    draw_call = client + 0x877e5;
    tooltip_call = client + 0x872a5;
    expected = (DWORD)(client + 0x125a68);
    if (render[0] != 0xa1 || *(DWORD *)(render + 1) != expected ||
        draw_call[0] != 0xe8 ||
        *(DWORD *)(draw_call + 1) != (DWORD)(client + 0x63b60 - (draw_call + 5)) ||
        tooltip_call[0] != 0xe8 ||
        *(DWORD *)(tooltip_call + 1) !=
            (DWORD)(client + 0x861c0 - (tooltip_call + 5)))
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
    bindings = (const BYTE *)client + 0x11e128;
    for (i = 0; i < 114; ++i) {
        const BYTE *entry = bindings + i * 10;
        if (*(const DWORD *)entry == 37 &&
            *(const WORD *)(entry + 4) == key)
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
        if (client && *(DWORD *)(client + 0x116dd0) &&
            is_interactive_target(*(DWORD *)(client + 0x116db8)))
            refresh_object_target(client);
    }
    if (key >= 256 || msg->hwnd != game_window || !is_show_items_key(key))
        return 0;
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        if (GetForegroundWindow() == game_window &&
            *(const void *const *)((BYTE *)GetModuleHandleA("D2Client.dll") + 0x127578)) {
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
