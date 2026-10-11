/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌‌​‌​​​​‌‌‌​​‌​​​​‌​‌‌​​​​​​‌‌​​‌‌​​‌‌​​‌‌​​​​‌‌⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo d0e42c0cccc3
 */

#include "jj.h"

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_engine[] = "wm:d0e42c0cccc3";

Macro g_macro;
Prefs g_prefs;
CRITICAL_SECTION g_cs;

#ifndef LLMHF_INJECTED
#define LLMHF_INJECTED 0x00000001
#endif
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

static volatile LONG g_enabled = 0, g_busy = 0, g_quit = 0, g_abort = 0;
static volatile LONG g_tgt_stamp = 0, g_mhk_t = 0, g_mhk_n = 0, g_swallow_btn = 0;
static HANDLE   g_evt, g_thread;
static HHOOK    g_hook;
static HWND     g_notify;
static LONGLONG g_freq = 1;
static DWORD    g_pid;
static int      g_sleepok = 0;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void timer_res_begin(void) { if (timeBeginPeriod(1) == TIMERR_NOERROR) g_sleepok = 1; }
void timer_res_end(void)   { if (g_sleepok) { timeEndPeriod(1); g_sleepok = 0; } }

/* ------------------------------------------------------------- secuencia */
void eng_build(const Macro *m, EvList *L)
{
    static const int pm6[6] = { 0, 240, 330, 570, 660, 900 };      /* milesimas del tiempo total */
    static const int pm4[4] = { 0, 300, 600, 900 };
    int n = 0, i;
    ZeroMemory(L, sizeof *L);
    if (m->mode == 0) {
        int T = clampi(m->ms, 1, 100), cnt = m->repeat ? 6 : 4;
        const int *pm = m->repeat ? pm6 : pm4;
        for (i = 0; i < cnt; i++) {
            int kind = (i == 0 || i == 4) ? EV_KDOWN : (i == 1 || i == 5) ? EV_KUP : (i == 2) ? EV_MDOWN : EV_MUP;
            L->ev[n].t_us = T * pm[i];
            L->ev[n].kind = kind;
            L->ev[n].code = (kind <= EV_KUP) ? m->key : m->button;
            n++;
        }
        L->total_us = T * 1000;
    } else {
        long t = 0;
        for (i = 0; i < m->nsteps && i < MAXSTEPS; i++) {
            const Step *s = &m->steps[i];
            int hold = clampi(s->hold, 0, 1000) * 1000, after = clampi(s->after, 0, 1000) * 1000;
            if (s->type == ST_KEY) {
                L->ev[n].t_us = (int)t;          L->ev[n].kind = EV_KDOWN; L->ev[n].code = s->code; n++;
                L->ev[n].t_us = (int)t + hold;   L->ev[n].kind = EV_KUP;   L->ev[n].code = s->code; n++;
            } else {
                int b = clampi(s->code, 0, 2);
                L->ev[n].t_us = (int)t;          L->ev[n].kind = EV_MDOWN; L->ev[n].code = b; n++;
                L->ev[n].t_us = (int)t + hold;   L->ev[n].kind = EV_MUP;   L->ev[n].code = b; n++;
            }
            t += hold + after;
        }
        L->total_us = (int)t;
    }
    L->n = n;
}

/* ------------------------------------------------------------- tiempo y entrada */
static inline LONGLONG ticks(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

/* Duerme mientras falte mucho y gira (espera activa) en los ultimos 3 ms: exacto y sin gastar CPU. */
static void wait_until(LONGLONG target)
{
    LONGLONG margin = g_freq * 3 / 1000;
    while (!g_abort) {
        LONGLONG now = ticks();
        if (now >= target) return;
        if (g_sleepok && target - now > margin) Sleep(1);
        else YieldProcessor();
    }
}

static void send_key(int code, int up)
{
    INPUT in;
    ZeroMemory(&in, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)(code & 0xFF);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0) | ((code & 0x100) ? KEYEVENTF_EXTENDEDKEY : 0);
    SendInput(1, &in, sizeof in);
}

static void send_mouse(int btn, int up)
{
    static const DWORD dn[3] = { MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN };
    static const DWORD uf[3] = { MOUSEEVENTF_LEFTUP,   MOUSEEVENTF_RIGHTUP,   MOUSEEVENTF_MIDDLEUP };
    INPUT in;
    btn = clampi(btn, 0, 2);
    ZeroMemory(&in, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = up ? uf[btn] : dn[btn];
    SendInput(1, &in, sizeof in);
}

static void run_macro(void)
{
    Macro m;
    EvList L;
    int pk[16], npk = 0, pm = 0, i, j;
    LONGLONG t0;

    EnterCriticalSection(&g_cs);
    m = g_macro;
    LeaveCriticalSection(&g_cs);
    eng_build(&m, &L);

    t0 = ticks();
    for (i = 0; i < L.n && !g_abort; i++) {
        const Ev *e = &L.ev[i];
        if (e->t_us > 0) wait_until(t0 + g_freq * e->t_us / 1000000);
        if (g_abort) break;
        switch (e->kind) {
        case EV_KDOWN:
            send_key(e->code, 0);
            if (npk < 16) pk[npk++] = e->code;
            break;
        case EV_KUP:
            send_key(e->code, 1);
            for (j = 0; j < npk; j++) if (pk[j] == e->code) { pk[j] = pk[--npk]; break; }
            break;
        case EV_MDOWN:
            send_mouse(e->code, 0);
            pm |= 1 << clampi(e->code, 0, 2);
            break;
        default:
            send_mouse(e->code, 1);
            pm &= ~(1 << clampi(e->code, 0, 2));
            break;
        }
    }
    /* si se cancelo a medias, soltar lo que haya quedado apretado */
    for (i = 0; i < npk; i++) send_key(pk[i], 1);
    for (i = 0; i < 3; i++) if (pm & (1 << i)) send_mouse(i, 1);
}

static DWORD WINAPI Worker(LPVOID unused)
{
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (!g_quit) {
        WaitForSingleObject(g_evt, INFINITE);
        if (g_quit) break;
        run_macro();
        InterlockedExchange(&g_busy, 0);
    }
    return 0;
}

/* ------------------------------------------------------------- ventanas y programas */
static int exe_of_pid(DWORD pid, WCHAR *out, int n)
{
    WCHAR path[MAX_PATH], *b;
    DWORD sz = MAX_PATH;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    out[0] = 0;
    if (!h) return 0;
    if (QueryFullProcessImageNameW(h, 0, path, &sz)) {
        b = path + lstrlenW(path);
        while (b > path && b[-1] != L'\\') b--;
        lstrcpynW(out, b, n);
    }
    CloseHandle(h);
    return out[0] != 0;
}

static int contains_i(const WCHAR *hay, const WCHAR *needle)
{
    WCHAR a[160], b[80];
    int i, j, la, lb;
    if (!needle[0]) return 1;
    lstrcpynW(a, hay, 160);
    lstrcpynW(b, needle, 80);
    la = lstrlenW(a); lb = lstrlenW(b);
    CharLowerBuffW(a, la);
    CharLowerBuffW(b, lb);
    for (i = 0; i + lb <= la; i++) {
        for (j = 0; j < lb && a[i + j] == b[j]; j++) {}
        if (j == lb) return 1;
    }
    return 0;
}

int win_info(HWND h, WinItem *o)
{
    DWORD pid = 0;
    HWND r = h ? GetAncestor(h, GA_ROOT) : NULL;
    ZeroMemory(o, sizeof *o);
    if (!r) return 0;
    o->h = r;
    GetWindowTextW(r, o->title, 96);
    GetWindowThreadProcessId(r, &pid);
    exe_of_pid(pid, o->exe, 64);
    return 1;
}

static WinItem *g_enum_out;
static int g_enum_max, g_enum_n;

static BOOL CALLBACK enum_cb(HWND h, LPARAM lp)
{
    WinItem it;
    DWORD pid = 0;
    (void)lp;
    if (g_enum_n >= g_enum_max) return FALSE;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER) != NULL) return TRUE;
    if (GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    if (GetWindowTextLengthW(h) <= 0) return TRUE;
    GetWindowThreadProcessId(h, &pid);
    if (pid == g_pid) return TRUE;
    if (!win_info(h, &it)) return TRUE;
    if (!it.title[0] || !lstrcmpW(it.title, L"Program Manager")) return TRUE;
    g_enum_out[g_enum_n++] = it;
    return TRUE;
}

int win_enum(WinItem *out, int max)
{
    g_enum_out = out; g_enum_max = max; g_enum_n = 0;
    EnumWindows(enum_cb, 0);
    return g_enum_n;
}

void key_name(int code, WCHAR *out, int n)
{
    LONG lp = ((code & 0xFF) << 16) | (((code >> 8) & 1) << 24);
    if (!(code & 0xFF) || !GetKeyNameTextW(lp, out, n) || !out[0]) lstrcpynW(out, L"(ninguna)", n);
}

/* ------------------------------------------------------------- nombres de atajos */
int hk_vk(int hk)       { return hk & 0xFF; }
int hk_mods(int hk)     { return (hk >> 8) & 0xF; }
int hk_is_mouse(int hk) { int vk = hk & 0xFF; return vk == 0x04 || vk == 0x05 || vk == 0x06; }

static int is_ext_vk(int vk)
{
    switch (vk) {
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_NUMLOCK: case VK_DIVIDE:
    case VK_RCONTROL: case VK_RMENU: case VK_SNAPSHOT: case VK_APPS: case VK_LWIN: case VK_RWIN:
        return 1;
    }
    return 0;
}

void hk_name(int hk, WCHAR *out, int n)
{
    int vk = hk_vk(hk), md = hk_mods(hk);
    WCHAR k[64], b[160];
    b[0] = 0;
    if (!vk) { lstrcpynW(out, L"(ninguna)", n); return; }
    if (md & 2) lstrcatW(b, L"Ctrl+");
    if (md & 1) lstrcatW(b, L"Alt+");
    if (md & 4) lstrcatW(b, L"Shift+");
    if (md & 8) lstrcatW(b, L"Win+");
    if (vk >= VK_F1 && vk <= VK_F24) wsprintfW(k, L"F%d", vk - VK_F1 + 1);
    else if (vk == 0x04) lstrcpyW(k, L"Botón medio");
    else if (vk == 0x05) lstrcpyW(k, L"Botón lateral 1");
    else if (vk == 0x06) lstrcpyW(k, L"Botón lateral 2");
    else {
        UINT sc = MapVirtualKeyW((UINT)vk, 0 /* MAPVK_VK_TO_VSC */);
        LONG lp = (LONG)(sc << 16);
        if (is_ext_vk(vk)) lp |= 1 << 24;
        if (!sc || !GetKeyNameTextW(lp, k, 64) || !k[0]) wsprintfW(k, L"Tecla %d", vk);
    }
    lstrcatW(b, k);
    lstrcpynW(out, b, n);
}

void eng_set_mouse_hotkeys(int toggle, int next)
{
    InterlockedExchange(&g_mhk_t, toggle);
    InterlockedExchange(&g_mhk_n, next);
}

static int cur_mods(void)
{
    return ((GetAsyncKeyState(VK_MENU) & 0x8000) ? 1 : 0) | ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 2 : 0) |
           ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 4 : 0) | (((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) ? 8 : 0);
}

/* ------------------------------------------------------------- gancho del mouse */
static int own_active(void)
{
    DWORD pid = 0;
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == g_pid;
}

void eng_target_changed(void) { InterlockedIncrement(&g_tgt_stamp); }

/* 1 si la macro puede funcionar con la ventana que esta al frente. */
static int target_ok(void)
{
    static HWND ch;
    static DWORD ct;
    static LONG cs;
    static int cok;
    HWND fg;
    DWORD pid = 0, now = GetTickCount();
    WCHAR exe[64], title[160];
    int ok = 1;

    if (!g_macro.tgt_on || (!g_macro.tgt_exe[0] && !g_macro.tgt_filter[0])) return 1;   /* sin limite */
    fg = GetForegroundWindow();
    if (!fg) return 0;
    if (fg == ch && cs == g_tgt_stamp && now - ct < 400) return cok;
    GetWindowThreadProcessId(fg, &pid);
    if (g_macro.tgt_exe[0]) {
        exe_of_pid(pid, exe, 64);
        ok = (lstrcmpiW(exe, g_macro.tgt_exe) == 0);
    }
    if (ok && g_macro.tgt_filter[0]) {
        GetWindowTextW(fg, title, 160);
        ok = contains_i(title, g_macro.tgt_filter);
    }
    ch = fg; ct = now; cs = g_tgt_stamp; cok = ok;
    return ok;
}


static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const MSLLHOOKSTRUCT *m = (const MSLLHOOKSTRUCT *)lParam;
        if (!(m->flags & LLMHF_INJECTED)) {                        /* solo clics fisicos */
            if (wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN || wParam == WM_MBUTTONUP || wParam == WM_XBUTTONUP) {
                int mc = (wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP) ? 0x04 : (HIWORD(m->mouseData) == XBUTTON1 ? 0x05 : 0x06);
                if (wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN) {        /* atajos con botones del mouse */
                    int ht = (int)g_mhk_t, hn = (int)g_mhk_n, cm = cur_mods();
                    if (ht && hk_vk(ht) == mc && hk_mods(ht) == cm) { InterlockedExchange(&g_swallow_btn, mc); PostMessageW(g_notify, WM_APP_HOTKEY, 1, 0); return 1; }
                    if (hn && hk_vk(hn) == mc && hk_mods(hn) == cm) { InterlockedExchange(&g_swallow_btn, mc); PostMessageW(g_notify, WM_APP_HOTKEY, 2, 0); return 1; }
                } else if (g_swallow_btn == mc) {
                    InterlockedExchange(&g_swallow_btn, 0);
                    return 1;
                }
            }
            if (g_enabled) {
                WPARAM want = g_macro.button
                    ? (g_macro.release ? WM_RBUTTONUP : WM_RBUTTONDOWN)
                    : (g_macro.release ? WM_LBUTTONUP : WM_LBUTTONDOWN);
                if (wParam == want && !own_active() && target_ok()) {
                    if (InterlockedCompareExchange(&g_busy, 1, 0) == 0) SetEvent(g_evt);
                }
            }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int eng_hook_install(HWND notify)
{
    g_notify = notify;
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(NULL), 0);
    return g_hook != NULL;
}

void eng_hook_remove(void)
{
    if (g_hook) UnhookWindowsHookEx(g_hook);
    g_hook = NULL;
}

void eng_set_enabled(int on)
{
    if (on) InterlockedExchange(&g_abort, 0);
    else    InterlockedExchange(&g_abort, 1);                      /* corta la secuencia en curso */
    InterlockedExchange(&g_enabled, on ? 1 : 0);
}

int eng_get_enabled(void) { return g_enabled != 0; }

void eng_init(void)
{
    LARGE_INTEGER f;
    InitializeCriticalSection(&g_cs);
    macro_defaults(&g_macro);
    prefs_defaults(&g_prefs);
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    g_pid = GetCurrentProcessId();
    g_evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_thread = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
}

void eng_shutdown(void)
{
    InterlockedExchange(&g_quit, 1);
    InterlockedExchange(&g_abort, 1);
    if (g_evt) SetEvent(g_evt);
    if (g_thread) { WaitForSingleObject(g_thread, 1000); CloseHandle(g_thread); g_thread = NULL; }
}
