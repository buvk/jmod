#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "config.h"

#define QC_MESSAGE (WM_APP + 0x310)
#define PICKUP_GOLD_MESSAGE (WM_APP + 0x313)

static HMODULE module;
static HHOOK message_hook;
static HWND game_window;
static DWORD game_thread;
static BYTE held[256];
static BYTE show_key_down[256];
static int was_focused;
static volatile unsigned held_count;
static volatile LONG item_labels_on;
static DWORD last_gold_at;
static struct { DWORD id, at; } recent_gold[32];
static volatile LONG gold_message_pending;
static D2ModConfig config;

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

static void __fastcall draw_labels_before_tooltip(void *selected)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    typedef void (__fastcall *draw_hover_fn)(void *);
    typedef int (__cdecl *mode_getter_fn)(void);
    /* vanilla skips label drawing entirely in game-state 3; preserve that
       guard here since it no longer runs through the original gated site */
    if (item_labels_on && ((mode_getter_fn)(client + 0x14a20))() != 3)
        draw_item_labels();
    ((draw_hover_fn)(client + 0x861c0))(selected);
}

/* the flag-gated draw_call site now no-ops: labels are drawn from the
   always-runs tooltip call site instead, so the tooltip never gets starved
   by the label toggle */
static void __cdecl labels_already_drawn(void)
{
}

static void pick_up_nearby_gold(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    HMODULE common = GetModuleHandleA("D2Common.dll");
    HMODULE net = GetModuleHandleA("D2Net.dll");
    typedef int (__stdcall *unit_coord_fn)(const void *);
    typedef const BYTE *(__stdcall *item_text_fn)(DWORD);
    typedef void (__stdcall *send_packet_fn)(DWORD, const BYTE *, DWORD);
    unit_coord_fn unit_x, unit_y;
    item_text_fn item_text;
    send_packet_fn send_packet;
    union { FARPROC raw; item_text_fn typed; } item_text_export;
    union { FARPROC raw; send_packet_fn typed; } send_packet_export;
    const BYTE *player;
    const BYTE *item;
    const BYTE *record;
    BYTE packet[13];
    DWORD id, now;
    int x, y, ix, iy;
    unsigned bucket, visited, slot;
    const BYTE *const *items;

    if (!config.auto_gold_pickup || !client || !common || !net)
        return;
    player = *(const BYTE *const *)(client + 0x127578);
    if (!player || *(const DWORD *)player != 0 ||
        !*(const void *const *)(player + 0x38))
        return;

    unit_x = (unit_coord_fn)GetProcAddress(common, MAKEINTRESOURCEA(10327));
    unit_y = (unit_coord_fn)GetProcAddress(common, MAKEINTRESOURCEA(10330));
    item_text_export.raw = GetProcAddress(common, MAKEINTRESOURCEA(10600));
    send_packet_export.raw = GetProcAddress(net, MAKEINTRESOURCEA(10005));
    item_text = item_text_export.typed;
    send_packet = send_packet_export.typed;
    if (!unit_x || !unit_y || !item_text || !send_packet)
        return;

    x = unit_x(player) >> 16;
    y = unit_y(player) >> 16;
    now = GetTickCount();
    if ((DWORD)(now - last_gold_at) < config.gold_request_interval_ms)
        return;

    items = (const BYTE *const *)(client + 0x125d78 + 4 * 128 * sizeof(void *));
    for (bucket = 0, visited = 0; bucket < 128; ++bucket) {
        for (item = items[bucket]; item && visited++ < 4096;
             item = *(const BYTE *const *)(item + 0x108)) {
            if (*(const DWORD *)item != 4 || *(const DWORD *)(item + 0x0c) != 3 ||
                !*(const void *const *)(item + 0x38))
                continue;
            id = *(const DWORD *)(item + 0x08);
            slot = id % 32;
            if (recent_gold[slot].id == id &&
                (DWORD)(now - recent_gold[slot].at) < config.gold_retry_interval_ms)
                continue;
            record = item_text(*(const DWORD *)(item + 0x04));
            if (!record || *(const DWORD *)(record + 0x144) != 0x20646c67)
                continue;
            ix = (unit_x(item) >> 16) - x;
            iy = (unit_y(item) >> 16) - y;
            if (ix < -config.gold_pickup_range || ix > config.gold_pickup_range ||
                iy < -config.gold_pickup_range || iy > config.gold_pickup_range ||
                ix * ix + iy * iy > config.gold_pickup_range * config.gold_pickup_range)
                continue;
            packet[0] = 0x16;
            *(DWORD *)(packet + 1) = 4;
            *(DWORD *)(packet + 5) = id;
            *(DWORD *)(packet + 9) = 0;
            send_packet(0, packet, sizeof(packet));
            last_gold_at = now;
            recent_gold[slot].id = id;
            recent_gold[slot].at = now;
            return;
        }
        if (visited >= 4096) break;
    }
}

static int install_item_label_toggle(void)
{
    BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
    BYTE *render, *draw_call, *tooltip_call;
    DWORD expected, old_render, old_call, old_tooltip, unused;
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

static int is_skill_key(WPARAM key)
{
    HMODULE client = GetModuleHandleA("D2Client.dll");
    const BYTE *bindings;
    unsigned i;
    if (!client || key > 0xff)
        return 0;
    bindings = (const BYTE *)client + 0x11e128;
    for (i = 0; i < 114; ++i) {
        const BYTE *entry = bindings + i * 10;
        DWORD action = *(const DWORD *)entry;
        WORD bound_key = *(const WORD *)(entry + 4);
        if (((action >= 14 && action <= 21) ||
             (action >= 46 && action <= 53)) && bound_key == key)
            return 1;
    }
    return 0;
}

static void mouse_button(DWORD flag)
{
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flag;
    SendInput(1, &input, sizeof(input));
}

static void release_all(void)
{
    ZeroMemory(held, sizeof(held));
    if (held_count) {
        held_count = 0;
        mouse_button(MOUSEEVENTF_RIGHTUP);
    }
}

static LRESULT CALLBACK on_message(int code, WPARAM removed, LPARAM value)
{
    MSG *msg;
    unsigned key;

    if (code < 0 || removed != PM_REMOVE)
        return CallNextHookEx(message_hook, code, removed, value);

    msg = (MSG *)value;
    if (config.always_show_items && item_labels_on && msg->hwnd == game_window &&
        msg->message == WM_LBUTTONDOWN &&
        GetForegroundWindow() == game_window) {
        BYTE *client = (BYTE *)GetModuleHandleA("D2Client.dll");
        if (client && *(DWORD *)(client + 0x116dd0) &&
            is_interactive_target(*(DWORD *)(client + 0x116db8)))
            refresh_object_target(client);
    }
    if (msg->message == PICKUP_GOLD_MESSAGE) {
        /* always clear pending: game_window can race to NULL on the polling thread */
        InterlockedExchange(&gold_message_pending, 0);
        if (msg->hwnd == game_window && GetForegroundWindow() == msg->hwnd)
            pick_up_nearby_gold();
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }
    if (msg->message == QC_MESSAGE && msg->hwnd == game_window) {
        if (msg->wParam == 1 && held_count && GetForegroundWindow() == game_window)
            mouse_button(MOUSEEVENTF_RIGHTDOWN);
        else if (msg->wParam == 2)
            mouse_button(MOUSEEVENTF_RIGHTUP);
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }

    key = (unsigned)msg->wParam;
    if (key >= 256)
        return CallNextHookEx(message_hook, code, removed, value);

    if (config.always_show_items && msg->hwnd == game_window &&
        is_show_items_key(key)) {
        if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
            if (GetForegroundWindow() == game_window &&
                *(const void *const *)((BYTE *)GetModuleHandleA("D2Client.dll") + 0x127578)) {
                if (!show_key_down[key]) {
                    show_key_down[key] = 1;
                    InterlockedExchange(&item_labels_on, !item_labels_on);
                }
                msg->message = WM_NULL;
                return CallNextHookEx(message_hook, code, removed, value);
            }
        } else if ((msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) &&
                   show_key_down[key]) {
            show_key_down[key] = 0;
            msg->message = WM_NULL;
            return CallNextHookEx(message_hook, code, removed, value);
        }
    }

    if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
        msg->hwnd == game_window && GetForegroundWindow() == game_window) {
        if (config.quick_cast && !held[key] && is_skill_key(key)) {
            held[key] = 1;
            if (++held_count == 1)
                PostMessageA(game_window, QC_MESSAGE, 1, 0);
        }
    } else if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
        if (held[key]) {
            held[key] = 0;
            if (--held_count == 0)
                PostMessageA(game_window, QC_MESSAGE, 2, 0);
        }
    }
    return CallNextHookEx(message_hook, code, removed, value);
}

static BOOL CALLBACK find_window(HWND hwnd, LPARAM unused)
{
    DWORD process;
    DWORD thread = GetWindowThreadProcessId(hwnd, &process);
    (void)unused;
    if (process == GetCurrentProcessId() && IsWindowVisible(hwnd) &&
        GetWindow(hwnd, GW_OWNER) == NULL) {
        game_window = hwnd;
        game_thread = thread;
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI start_hook(void *unused)
{
    (void)unused;
    while (!GetModuleHandleA("D2Client.dll")) Sleep(100);
    read_options(module, &config);
    if (config.always_show_items && !install_item_label_toggle())
        config.always_show_items = 0;
    for (;;) {
        HWND found;
        DWORD thread;
        int focused;
        game_window = NULL;
        game_thread = 0;
        EnumWindows(find_window, 0);
        found = game_window;
        thread = game_thread;

        if (message_hook && (!found || !IsWindow(found) ||
                             thread != GetWindowThreadProcessId(found, NULL))) {
            release_all();
            UnhookWindowsHookEx(message_hook);
            message_hook = NULL;
            InterlockedExchange(&gold_message_pending, 0);
        }
        if (!message_hook && found && thread)
            message_hook = SetWindowsHookExA(WH_GETMESSAGE, on_message, module, thread);
        focused = (GetForegroundWindow() == game_window);
        if (config.always_show_items && focused && !was_focused) {
            /* the game's queue misses key-up events while unfocused; resync
               against real key state instead of assuming everything is up,
               or a still-held Show Items key (e.g. mid Alt-Tab) would look
               like a fresh press and toggle labels again on return */
            unsigned key;
            for (key = 0; key < 256; ++key)
                show_key_down[key] = (GetAsyncKeyState((int)key) & 0x8000) != 0;
        }
        was_focused = focused;
        if (config.auto_gold_pickup && message_hook && found &&
            GetForegroundWindow() == found &&
            InterlockedCompareExchange(&gold_message_pending, 1, 0) == 0 &&
            !PostMessageA(found, PICKUP_GOLD_MESSAGE, 0, 0))
            InterlockedExchange(&gold_message_pending, 0);
        if (held_count && GetForegroundWindow() != game_window)
            release_all();
        Sleep(config.auto_gold_pickup ? config.gold_scan_interval_ms : 100);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    HANDLE thread;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance;
        DisableThreadLibraryCalls(instance);
        thread = CreateThread(NULL, 0, start_hook, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
