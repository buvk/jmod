#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "quick_cast.h"
#include "game_ui.h"

#define QC_MESSAGE (WM_APP + 0x310)
#define RBUTTON_INJECT_MARK ((ULONG_PTR)0x6a6d6f64)
static BYTE held[256];
static BYTE blocked[256];
static int real_rbutton_down;
static int injected_rbutton_down;
static unsigned held_count;
static unsigned qc_generation;
static CRITICAL_SECTION input_lock;
static int quick_cast_enabled;

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
    input.mi.dwExtraInfo = RBUTTON_INJECT_MARK;
    SendInput(1, &input, sizeof(input));
}

void quick_cast_release_all(void)
{
    unsigned key;
    EnterCriticalSection(&input_lock);
    /* Preserve held skill keys as blocked until their key-up (or a resync).
       This prevents a held key from immediately casting after a transition. */
    for (key = 0; key < 256; ++key) {
        if (held[key])
            blocked[key] = 1;
        held[key] = 0;
    }
    if (held_count || injected_rbutton_down)
        ++qc_generation; /* cancel commands queued for the old hold */
    held_count = 0;
    if (injected_rbutton_down && !real_rbutton_down)
        mouse_button(MOUSEEVENTF_RIGHTUP);
    injected_rbutton_down = 0;
    LeaveCriticalSection(&input_lock);
}

void quick_cast_pause(void)
{
    unsigned key;
    EnterCriticalSection(&input_lock);
    /* A key held when the menu opens must not cast from a repeat after it closes. */
    for (key = 0; key < 256; ++key) {
        if (held[key])
            blocked[key] = 1;
        held[key] = 0;
    }
    if (held_count || injected_rbutton_down)
        ++qc_generation;
    held_count = 0;
    if (injected_rbutton_down && !real_rbutton_down)
        mouse_button(MOUSEEVENTF_RIGHTUP);
    injected_rbutton_down = 0;
    LeaveCriticalSection(&input_lock);
}

void quick_cast_resync_keys(void)
{
    unsigned key;
    EnterCriticalSection(&input_lock);
    for (key = 0; key < 256; ++key)
        blocked[key] = (GetAsyncKeyState((int)key) & 0x8000) != 0;
    LeaveCriticalSection(&input_lock);
}

void quick_cast_init(int enabled)
{
    InitializeCriticalSection(&input_lock);
    quick_cast_enabled = enabled;
}

int quick_cast_on_message(MSG *msg, HWND game_window)
{
    unsigned key;
    int menu_open;

    /* Skill bindings and simulated world clicks are meaningful only while a
       game is active. Still track key state in the front end so a key held
       through game creation cannot cast from its first repeat. */
    if (!game_active()) {
        if (msg->message == QC_MESSAGE && msg->hwnd == game_window)
            return 1;
        key = (unsigned)msg->wParam;
        if (msg->hwnd == game_window && key < 256) {
            if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
                EnterCriticalSection(&input_lock);
                blocked[key] = 1;
                LeaveCriticalSection(&input_lock);
            } else if (msg->message == WM_KEYUP ||
                       msg->message == WM_SYSKEYUP) {
                EnterCriticalSection(&input_lock);
                blocked[key] = 0;
                LeaveCriticalSection(&input_lock);
            }
        }
        return 0;
    }

    menu_open = game_menu_open();
    if (menu_open || ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
                      msg->wParam == VK_ESCAPE))
        quick_cast_pause();
    if (msg->hwnd == game_window &&
        (msg->message == WM_RBUTTONDOWN || msg->message == WM_RBUTTONUP) &&
        GetMessageExtraInfo() != RBUTTON_INJECT_MARK) {
        /* Track genuine clicks so releasing a skill key does not release
           the physical button. Keep the hold alive if a skill remains held. */
        EnterCriticalSection(&input_lock);
        real_rbutton_down = (msg->message == WM_RBUTTONDOWN);
        if (real_rbutton_down) {
            injected_rbutton_down = 0;
        } else if (held_count && !menu_open &&
                   GetForegroundWindow() == game_window) {
            injected_rbutton_down = 1;
            mouse_button(MOUSEEVENTF_RIGHTDOWN);
        }
        LeaveCriticalSection(&input_lock);
    }
    if (msg->message == QC_MESSAGE && msg->hwnd == game_window) {
        EnterCriticalSection(&input_lock);
        if ((unsigned)msg->lParam == qc_generation) {
            if (msg->wParam == 1 && held_count && !menu_open &&
                GetForegroundWindow() == game_window &&
                !real_rbutton_down && !injected_rbutton_down) {
                injected_rbutton_down = 1;
                mouse_button(MOUSEEVENTF_RIGHTDOWN);
            } else if (msg->wParam == 2 && !held_count && injected_rbutton_down) {
                injected_rbutton_down = 0;
                if (!real_rbutton_down)
                    mouse_button(MOUSEEVENTF_RIGHTUP);
            }
        }
        LeaveCriticalSection(&input_lock);
        return 1;
    }
    key = (unsigned)msg->wParam;
    if (key >= 256)
        return 0;
    if (menu_open && (msg->message == WM_KEYDOWN ||
                      msg->message == WM_SYSKEYDOWN)) {
        EnterCriticalSection(&input_lock);
        blocked[key] = 1;
        LeaveCriticalSection(&input_lock);
        return 0;
    }
    if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
        msg->hwnd == game_window && GetForegroundWindow() == game_window) {
        EnterCriticalSection(&input_lock);
        if (quick_cast_enabled && !held[key] && !blocked[key] &&
            !menu_open && is_skill_key(key)) {
            held[key] = 1;
            if (++held_count == 1) {
                if (!injected_rbutton_down)
                    real_rbutton_down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
                PostMessageA(game_window, QC_MESSAGE, 1, (LPARAM)++qc_generation);
            }
        }
        LeaveCriticalSection(&input_lock);
    } else if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
        EnterCriticalSection(&input_lock);
        blocked[key] = 0;
        if (held[key]) {
            held[key] = 0;
            if (--held_count == 0)
                PostMessageA(game_window, QC_MESSAGE, 2, (LPARAM)++qc_generation);
        }
        LeaveCriticalSection(&input_lock);
    }
    return 0;
}
