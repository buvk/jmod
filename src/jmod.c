#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "config.h"
#include "item_labels.h"
#include "quick_cast.h"
#include "auto_gold.h"
#include "game_ui.h"
#include "rune_color.h"
#include "loot_filter.h"

static HMODULE module;
static HHOOK message_hook;
static HWND game_window;
static DWORD game_thread;
static int was_focused;
static D2ModConfig config;

static LRESULT CALLBACK on_message(int code, WPARAM removed, LPARAM value)
{
    MSG *msg;
    if (code < 0 || removed != PM_REMOVE)
        return CallNextHookEx(message_hook, code, removed, value);
    msg = (MSG *)value;
    if (config.always_show_items && item_labels_on_message(msg, game_window)) {
        msg->message = WM_NULL;
        return CallNextHookEx(message_hook, code, removed, value);
    }
    if (auto_gold_on_message(msg, game_window) ||
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
    read_options(module, &config);
    if (config.rune_color)
        rune_color_init();
    quick_cast_init(config.quick_cast);
    auto_gold_init(&config);
    if (!loot_filter_init(&config))
        config.loot_filter_enabled = 0;
    if (config.always_show_items && !item_labels_init())
        config.always_show_items = 0;
    for (;;) {
        GameWindowMatch match = { NULL, 0 };
        HWND found;
        DWORD thread;
        int focused;
        EnumWindows(find_window, (LPARAM)&match);
        found = match.window;
        thread = match.thread;

        if (message_hook && (!found || found != game_window ||
                             thread != game_thread || !IsWindow(found))) {
            quick_cast_release_all();
            UnhookWindowsHookEx(message_hook);
            message_hook = NULL;
            auto_gold_reset();
        }
        game_window = found;
        game_thread = thread;
        if (!message_hook && found && thread)
            message_hook = SetWindowsHookExA(WH_GETMESSAGE, on_message, module, thread);
        focused = (GetForegroundWindow() == game_window);
        if (config.always_show_items && focused && !was_focused)
            item_labels_resync_keys();
        if (focused && !was_focused)
            quick_cast_resync_keys();
        was_focused = focused;
        auto_gold_poll(found, message_hook != NULL);
        if (!focused)
            quick_cast_release_all();
        else if (game_menu_open())
            quick_cast_pause();
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
