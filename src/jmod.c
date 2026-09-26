#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "config.h"
#include "item_labels.h"
#include "quick_cast.h"
#include "auto_gold_pickup.h"
#include "game_ui.h"
#include "rune_color.h"
#include "loot_filter.h"
#include "d2_109b.h"
#include "ctrl_click_actions.h"
#include "item_stat_ranges.h"

static HMODULE module;
static HHOOK message_hook;
static HWND game_window;
static DWORD game_thread;
static int was_focused;
static int was_game_active;
static D2ModConfig config;

static LRESULT CALLBACK on_message(int code, WPARAM removed, LPARAM value)
{
    MSG *msg;
    if (code < 0 || removed != PM_REMOVE)
        return CallNextHookEx(message_hook, code, removed, value);
    msg = (MSG *)value;
    if (ctrl_click_actions_on_message(msg, game_window)) {
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }
    if (config.always_show_items && item_labels_on_message(msg, game_window)) {
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }
    if (auto_gold_pickup_on_message(msg, game_window) ||
        quick_cast_on_message(msg, game_window)) {
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }
    return CallNextHookEx(message_hook, code, removed, value);
}

typedef struct GameWindowMatch {
    HWND window;
    DWORD thread;
} GameWindowMatch;

static BOOL CALLBACK find_window(HWND hwnd, LPARAM value)
{
    GameWindowMatch *match = (GameWindowMatch *)value;
    DWORD process;
    DWORD thread = GetWindowThreadProcessId(hwnd, &process);
    if (process == GetCurrentProcessId() && IsWindowVisible(hwnd) &&
        GetWindow(hwnd, GW_OWNER) == NULL) {
        match->window = hwnd;
        match->thread = thread;
        return FALSE;
    }
    return TRUE;
}

static int cached_window_valid(HWND hwnd, DWORD thread)
{
    DWORD process;
    DWORD actual_thread;

    if (!hwnd || !thread || !IsWindow(hwnd) || !IsWindowVisible(hwnd) ||
        GetWindow(hwnd, GW_OWNER) != NULL)
        return 0;

    actual_thread = GetWindowThreadProcessId(hwnd, &process);
    return actual_thread == thread && process == GetCurrentProcessId();
}

static DWORD WINAPI start_hook(void *unused)
{
    char singleton_name[64];
    HANDLE singleton_mutex;
    DWORD mutex_error;
    (void)unused;
    /* The proxy and PlugY variants share this process. Only one may hook it. */
    wsprintfA(singleton_name, "Local\\jmod_%08lX",
              (unsigned long)GetCurrentProcessId());
    singleton_mutex = CreateMutexA(NULL, FALSE, singleton_name);
    if (!singleton_mutex)
        return 0;
    mutex_error = GetLastError();
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton_mutex);
        return 0;
    }
    while (!GetModuleHandleA("D2Client.dll")) Sleep(100);
    /* All gameplay helpers use 1.09b-specific D2Client offsets. Fail
       closed before installing hooks or reading any of those globals. */
    if (!d2_109b_client_matches()) {
        CloseHandle(singleton_mutex);
        return 0;
    }
    read_options(module, &config);
    if (config.rune_color && !rune_color_init())
        config.rune_color = 0;
    quick_cast_init(config.quick_cast);
    auto_gold_pickup_init(&config);
    if (config.item_stat_ranges && !item_stat_ranges_init())
        config.item_stat_ranges = 0;
    if (config.ctrl_click_actions && !ctrl_click_actions_init(&config))
        config.ctrl_click_actions = 0;
    if (!loot_filter_init(&config, module))
        config.loot_filter_enabled = 0;
    if (config.always_show_items && !item_labels_init())
        config.always_show_items = 0;
    for (;;) {
        HWND found = game_window;
        DWORD thread = game_thread;
        int focused;
        int active;

        /* The game normally keeps the same top-level window for its entire
           lifetime. Re-enumerate only when the cached handle disappears or
           no longer matches the window/thread we originally hooked. */
        if (!cached_window_valid(found, thread)) {
            GameWindowMatch match = { NULL, 0 };
            EnumWindows(find_window, (LPARAM)&match);
            found = match.window;
            thread = match.thread;
        }

        if (message_hook && (!found || found != game_window ||
                             thread != game_thread || !IsWindow(found))) {
            quick_cast_release_all();
            UnhookWindowsHookEx(message_hook);
            message_hook = NULL;
            auto_gold_pickup_reset();
            ctrl_click_actions_reset();
            was_focused = 0;
            was_game_active = 0;
        }
        game_window = found;
        game_thread = thread;
        if (!message_hook && found && thread)
            message_hook = SetWindowsHookExA(WH_GETMESSAGE, on_message, module, thread);

        focused = game_window && GetForegroundWindow() == game_window;
        active = game_active();

        /* Resync on focus gain and on front-end -> game transitions. A key
           held through either transition must be released before it can act. */
        if (focused && (!was_focused || (active && !was_game_active))) {
            if (config.always_show_items)
                item_labels_resync_keys();
            quick_cast_resync_keys();
        }
        if (!active && was_game_active) {
            auto_gold_pickup_reset();
            ctrl_click_actions_reset();
        }

        auto_gold_pickup_poll(found, message_hook != NULL);
        ctrl_click_actions_poll(found, message_hook != NULL);
        if (!focused || !active) {
            quick_cast_release_all();
            ctrl_click_actions_reset();
        }
        else if (game_menu_open())
            quick_cast_pause();

        was_focused = focused;
        was_game_active = active;
        if (ctrl_click_actions_pending())
            Sleep(CTRL_CLICK_ACTIONS_POLL_INTERVAL_MS);
        else
            Sleep(config.auto_gold_pickup && active ?
                  AUTO_GOLD_PICKUP_SCAN_INTERVAL_MS : 100);
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
