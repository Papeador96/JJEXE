/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌‌‌​‌‌​‌​‌‌​​‌​‌​‌​​​​​‌‌​‌​‌‌‌‌​‌​‌‌‌​​​​​​​​‌​⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo ed6541af5c02
 */

/*
 * JJEXE - macro de clic y teclas
 *
 * Al hacer clic ejecuta una secuencia (por defecto: tecla 2 -> clic -> tecla 2) en pocos milisegundos.
 * Funciona solo con tus clics fisicos, solo en la ventana que elijas (opcional), con configuraciones
 * guardadas, secuencia personalizada, temas, bandeja del sistema y animacion de inicio propia.
 *
 * Compilar (MinGW-w64):
 *   x86_64-w64-mingw32-windres app.rc -O coff -o app_res.o
 *   x86_64-w64-mingw32-gcc -O2 -s -static -mwindows main.c engine.c ui.c cfg.c splash.c gdip.c app_res.o \
 *       -o JJEXE.exe -lgdiplus -lmsimg32 -lwinmm -lcomdlg32 -lshell32 -lgdi32 -luser32 -lkernel32
 */
#include "jj.h"

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_main[] = "wm:ed6541af5c02";

int   g_dpi = 96;
HFONT g_font, g_fontB, g_fontS, g_fontSB, g_fontBig;
HWND  g_hEditName, g_hEditFilter;

static HINSTANCE g_inst;
static HWND   g_hwnd, g_osd;
static HBRUSH g_brInput;
static HICON  g_icoApp, g_icoAppSm, g_icoOn, g_icoOff;
static WCHAR  g_last[CFG_NAME_MAX];
static int    g_sel = -1;
static WinItem g_wins[64];
static int    g_nwins;
static WCHAR  g_anims[64][128];
static int    g_nanims;
static WCHAR  g_snds[64][128];
static int    g_nsnds;
static HWND   g_hEditUpd;
static WCHAR  g_upd_open[256];                       /* enlace de descarga de la version nueva encontrada */

/* animaciones integradas (ademas de la predeterminada y de las que cada quien agregue) */
static const WCHAR *k_bi_id[]   = { L"*claude", L"*roblox", L"*minecraft", L"*synth" };
static const WCHAR *k_bi_name[] = { L"Claude (terminal y chispa)", L"Roblox (bloques)", L"Minecraft (píxeles)", L"Synthwave (neón azul y dorado)" };
#define N_BI 4
static UINT   g_msgTaskbar;
static int    g_tray_added, g_tracking, g_quitting;
static WCHAR  g_osd1[96], g_osd2[96];
static ULONGLONG g_osd_t0;

#define TIMER_PICK 1
#define HK_TOGGLE  1
#define HK_NEXT    2
enum { CMD_SHOW = 1, CMD_TOGGLE, CMD_NEXT, CMD_EXIT };
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

static void sync_ui(void);
static void autosave(void);
static void tray_update(void);
static void toast(const WCHAR *l1, const WCHAR *l2, int force);

static int g_pick_left;

static void pick_stop(void)
{
    KillTimer(g_hwnd, TIMER_PICK);
    g_pick_left = 0;
    ui_set_text(ID_TGT_PICK, L"Detectar ventana activa");
}

static void pick_labels(void)
{
    WCHAR b[96];
    wsprintfW(b, L"Cambia a la ventana del juego…  %d", g_pick_left);
    ui_set_text(ID_TGT_L1, b);
    ui_set_text(ID_TGT_L2, L"Al llegar a 0 se toma la ventana que esté al frente");
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void lock(void)   { EnterCriticalSection(&g_cs); }
static void unlock(void) { LeaveCriticalSection(&g_cs); }

/* ===================================================================== */
/*  Aviso en pantalla (OSD)                                               */
/* ===================================================================== */
static HDC     g_osd_dc;
static HBITMAP g_osd_bmp, g_osd_old;
static int     g_osd_w, g_osd_h;
#define WDA_EXCLUDE 0x11                 /* WDA_EXCLUDEFROMCAPTURE (Windows 10 2004 o mas nuevo) */

static int set_affinity(HWND h, DWORD flag)
{
    typedef BOOL (WINAPI *PFN)(HWND, DWORD);
    HMODULE u = GetModuleHandleW(L"user32.dll");
    PFN fn = u ? (PFN)(void *)GetProcAddress(u, "SetWindowDisplayAffinity") : NULL;
    return fn ? fn(h, flag) : 0;
}

static void apply_stream(void)
{
    if (g_hwnd) set_affinity(g_hwnd, (g_prefs.stream && g_prefs.stream_main) ? WDA_EXCLUDE : 0);
}

static HBITMAP dib32(HDC ref, int w, int h, DWORD **bits)
{
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(ref, &bi, DIB_RGB_COLORS, (void **)bits, NULL, 0);
}

/* Dibuja el aviso con esquinas suaves: contenido opaco + mascara de la forma (transparencia por pixel). */
static void osd_render(void)
{
    const Theme *t = ui_theme();
    int w = g_osd_w, h = g_osd_h, n = w * h, rad = S(16), i;
    HDC scr = GetDC(NULL), mdc;
    DWORD *px = NULL, *mk = NULL;
    HBITMAP mb, mold;
    RECT rc, a, b;
    HBRUSH br;
    void *g;
    if (g_osd_dc) { SelectObject(g_osd_dc, g_osd_old); DeleteObject(g_osd_bmp); DeleteDC(g_osd_dc); }
    g_osd_dc = CreateCompatibleDC(scr);
    g_osd_bmp = dib32(scr, w, h, &px);
    g_osd_old = (HBITMAP)SelectObject(g_osd_dc, g_osd_bmp);
    rc.left = 0; rc.top = 0; rc.right = w; rc.bottom = h;
    br = CreateSolidBrush(t->card); FillRect(g_osd_dc, &rc, br); DeleteObject(br);
    g = gp_begin(g_osd_dc);
    gp_fill_rr_grad(g, 0, 0, (float)w, (float)S(5), 0, t->accent, t->accent2, 255, 0);
    gp_stroke_rr(g, 1.0f, 1.0f, (float)w - 2.0f, (float)h - 2.0f, (float)rad, t->accent, 255, 1.6f);
    gp_end(g);
    SetBkMode(g_osd_dc, TRANSPARENT);
    a = rc; a.top += S(8);
    if (g_osd2[0]) {
        a.bottom = h / 2 + S(6);
        b = rc; b.top = h / 2 + S(4); b.bottom -= S(6);
        SelectObject(g_osd_dc, g_fontB); SetTextColor(g_osd_dc, t->text);
        DrawTextW(g_osd_dc, g_osd1, -1, &a, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(g_osd_dc, g_fontS); SetTextColor(g_osd_dc, t->dim);
        DrawTextW(g_osd_dc, g_osd2, -1, &b, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    } else {
        SelectObject(g_osd_dc, g_fontB); SetTextColor(g_osd_dc, t->text);
        DrawTextW(g_osd_dc, g_osd1, -1, &a, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    GdiFlush();
    mdc = CreateCompatibleDC(scr);
    mb = dib32(scr, w, h, &mk);
    mold = (HBITMAP)SelectObject(mdc, mb);
    ZeroMemory(mk, (size_t)n * 4);
    g = gp_begin(mdc);
    gp_fill_rr(g, 0, 0, (float)w, (float)h, (float)rad, RGB(255, 255, 255), 255);
    gp_end(g);
    GdiFlush();
    for (i = 0; i < n; i++) {                                        /* el rojo de la mascara = cobertura del borde */
        unsigned al = (mk[i] >> 16) & 0xFF, r8 = (px[i] >> 16) & 255, g8 = (px[i] >> 8) & 255, b8 = px[i] & 255;
        px[i] = (al << 24) | ((r8 * al / 255) << 16) | ((g8 * al / 255) << 8) | (b8 * al / 255);
    }
    SelectObject(mdc, mold); DeleteObject(mb); DeleteDC(mdc);
    ReleaseDC(NULL, scr);
}

static void osd_present(int alpha)
{
    BLENDFUNCTION bf;
    POINT src, pos;
    SIZE sz;
    src.x = 0; src.y = 0;
    pos.x = (GetSystemMetrics(SM_CXSCREEN) - g_osd_w) / 2; pos.y = S(36);
    sz.cx = g_osd_w; sz.cy = g_osd_h;
    bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0; bf.SourceConstantAlpha = (BYTE)alpha; bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_osd, NULL, &pos, &sz, g_osd_dc, &src, 0, &bf, ULW_ALPHA);
}

static LRESULT CALLBACK OsdProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER) {
        ULONGLONG el = GetTickCount64() - g_osd_t0;
        int al = 255;
        if (el > 1300) al = 255 - (int)((el - 1300) * 255 / 500);
        if (el >= 1800 || al <= 0) { KillTimer(hwnd, 1); ShowWindow(hwnd, SW_HIDE); return 0; }
        osd_present(al);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void toast(const WCHAR *l1, const WCHAR *l2, int force)
{
    static int reg;
    if (!force && !g_prefs.osd) return;
    lstrcpynW(g_osd1, l1, 96);
    lstrcpynW(g_osd2, l2 ? l2 : L"", 96);
    if (!reg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc; wc.lpfnWndProc = OsdProc; wc.hInstance = g_inst; wc.lpszClassName = L"JJEXEOsd";
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassExW(&wc);
        reg = 1;
    }
    if (!g_osd)
        g_osd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                L"JJEXEOsd", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_inst, NULL);
    if (!g_osd) return;
    /* modo stream: el aviso se excluye de la captura ANTES de mostrarse; si Windows no lo permite, no se muestra */
    if (g_prefs.stream) { if (!set_affinity(g_osd, WDA_EXCLUDE)) return; }
    else set_affinity(g_osd, 0);
    g_osd_w = S(350); g_osd_h = S(62);
    osd_render();
    osd_present(255);
    SetWindowPos(g_osd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_osd_t0 = GetTickCount64();
    SetTimer(g_osd, 1, 30, NULL);
}

/* ===================================================================== */
/*  Bandeja del sistema                                                   */
/* ===================================================================== */
static void tray_update(void)
{
    NOTIFYICONDATAW nid;
    int on = eng_get_enabled();
    if (!g_hwnd) return;
    ZeroMemory(&nid, sizeof nid);
    nid.cbSize = sizeof nid;
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = on ? g_icoOn : g_icoOff;
    wsprintfW(nid.szTip, L"JJEXE — %s", on ? L"ACTIVADO" : L"DESACTIVADO");
    Shell_NotifyIconW(g_tray_added ? NIM_MODIFY : NIM_ADD, &nid);
    g_tray_added = 1;
}

static void tray_remove(void)
{
    NOTIFYICONDATAW nid;
    if (!g_tray_added) return;
    ZeroMemory(&nid, sizeof nid);
    nid.cbSize = sizeof nid; nid.hWnd = g_hwnd; nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_tray_added = 0;
}

static void show_main(void)
{
    ShowWindow(g_hwnd, SW_SHOW);
    ShowWindow(g_hwnd, SW_RESTORE);
    SetForegroundWindow(g_hwnd);
}

/* ===================================================================== */
/*  Estado de la macro (activar / desactivar)                             */
/* ===================================================================== */
static void hook_update(void)
{
    static int on;
    int need = eng_get_enabled() || hk_is_mouse(g_prefs.hk_toggle) || hk_is_mouse(g_prefs.hk_next);
    if (need && !on) {
        if (eng_hook_install(g_hwnd)) on = 1;
        else toast(L"No se pudo activar la detección de clics", NULL, 1);
    } else if (!need && on) {
        eng_hook_remove();
        on = 0;
    }
}

static void sync_toggle(void)
{
    WCHAR hint[200], k[80];
    int on = eng_get_enabled();
    if (g_prefs.hk_toggle) {
        hk_name(g_prefs.hk_toggle, k, 80);
        wsprintfW(hint, L"%s con %s", on ? L"Desactivar" : L"Activar", k);
    } else {
        lstrcpyW(hint, on ? L"Clic para desactivar" : L"Clic para activar");
    }
    ui_set_val(ID_TOGGLE, on);
    ui_set_text(ID_TOGGLE, on ? L"ACTIVADO" : L"DESACTIVADO");
    ui_set_text(ID_TGLHINT, hint);
    tray_update();
}

static void set_enabled(int on)
{
    eng_set_enabled(on);
    hook_update();
    sync_toggle();
    if (g_prefs.sound && !g_prefs.stream) snd_play(on);
    toast(on ? L"Macro ACTIVADA" : L"Macro desactivada", g_last[0] ? g_last : NULL, 0);
}

/* ===================================================================== */
/*  Guardado y listas                                                     */
/* ===================================================================== */
static void autosave(void)
{
    Macro m;
    Prefs p;
    lock(); m = g_macro; p = g_prefs; unlock();
    cfg_write(g_ini, L"Actual", &m, &p, g_last);
}

static int names_sorted(WCHAR names[][CFG_NAME_MAX], int max)
{
    int n = cfg_names(g_ini, names, max), i, j;
    for (i = 1; i < n; i++) {
        WCHAR t[CFG_NAME_MAX];
        lstrcpyW(t, names[i]);
        for (j = i - 1; j >= 0 && lstrcmpiW(names[j], t) > 0; j--) lstrcpyW(names[j + 1], names[j]);
        lstrcpyW(names[j + 1], t);
    }
    return n;
}

static void reload_configs(void)
{
    static WCHAR names[64][CFG_NAME_MAX];
    int n = names_sorted(names, 64), i, sel = -1;
    ui_items_clear(ID_CFG_DROP);
    for (i = 0; i < n; i++) {
        ui_items_add(ID_CFG_DROP, names[i]);
        if (!lstrcmpiW(names[i], g_last)) sel = i;
    }
    ui_set_val(ID_CFG_DROP, sel);
}

static void reload_wins(void)
{
    WCHAR b[160];
    int i;
    g_nwins = win_enum(g_wins, 64);
    ui_items_clear(ID_TGT_LIST);
    for (i = 0; i < g_nwins; i++) {
        wsprintfW(b, L"%s  —  %s", g_wins[i].title, g_wins[i].exe[0] ? g_wins[i].exe : L"?");
        ui_items_add(ID_TGT_LIST, b);
    }
    ui_set_val(ID_TGT_LIST, -1);
}

static void reload_anims(void)
{
    int i, sel = 0;
    g_nanims = anim_scan(g_anims, 56);
    ui_items_clear(ID_ANIM_DROP);
    ui_items_add(ID_ANIM_DROP, L"JJEXE (predeterminada)");
    ui_items_add(ID_ANIM_DROP, L"Ninguna (abrir directo)");
    for (i = 0; i < N_BI; i++) ui_items_add(ID_ANIM_DROP, k_bi_name[i]);
    for (i = 0; i < g_nanims; i++) ui_items_add(ID_ANIM_DROP, g_anims[i]);
    if (!lstrcmpW(g_prefs.anim, L"*none")) sel = 1;
    else if (g_prefs.anim[0] == L'*') { for (i = 0; i < N_BI; i++) if (!lstrcmpW(k_bi_id[i], g_prefs.anim)) sel = 2 + i; }
    else if (g_prefs.anim[0])
        for (i = 0; i < g_nanims; i++) if (!lstrcmpiW(g_anims[i], g_prefs.anim)) sel = 2 + N_BI + i;
    ui_set_val(ID_ANIM_DROP, sel);
}

static int snd_sel(const WCHAR *nm)
{
    int i;
    if (!nm[0]) return 0;
    if (!lstrcmpW(nm, L"*none")) return 1;
    for (i = 0; i < g_nsnds; i++) if (!lstrcmpiW(g_snds[i], nm)) return 2 + i;
    return 0;
}

static void reload_sounds(void)
{
    int i, ids[2] = { ID_SND_ON, ID_SND_OFF }, k;
    g_nsnds = snd_scan(g_snds, 56);
    for (k = 0; k < 2; k++) {
        ui_items_clear(ids[k]);
        ui_items_add(ids[k], L"Tono predeterminado");
        ui_items_add(ids[k], L"Sin sonido");
        for (i = 0; i < g_nsnds; i++) ui_items_add(ids[k], g_snds[i]);
    }
    ui_set_val(ID_SND_ON,  snd_sel(g_prefs.snd_on));
    ui_set_val(ID_SND_OFF, snd_sel(g_prefs.snd_off));
}

static void snd_test(int on)
{
    if (g_prefs.stream) { toast(L"Modo stream activado", L"En modo stream no suena nada", 1); return; }
    snd_play(on);
}

/* ===================================================================== */
/*  Pasos de la secuencia personalizada                                   */
/* ===================================================================== */
static void step_text(int i, WCHAR *out)
{
    static const WCHAR *bn[3] = { L"izquierdo", L"derecho", L"del medio" };
    const Step *s = &g_macro.steps[i];
    WCHAR k[48];
    if (s->type == ST_KEY) {
        key_name(s->code, k, 48);
        wsprintfW(out, L"%d   Tecla %s  ·  mantener %d ms  ·  esperar %d ms", i + 1, k, s->hold, s->after);
    } else {
        wsprintfW(out, L"%d   Clic %s  ·  mantener %d ms  ·  esperar %d ms", i + 1, bn[clampi(s->code, 0, 2)], s->hold, s->after);
    }
}

static void rebuild_steps(void)
{
    WCHAR b[96];
    int i;
    ui_items_clear(ID_STEPLIST);
    for (i = 0; i < g_macro.nsteps; i++) { step_text(i, b); ui_items_add(ID_STEPLIST, b); }
    if (g_sel >= g_macro.nsteps) g_sel = g_macro.nsteps - 1;
    ui_set_val(ID_STEPLIST, g_sel);
    ui_list_reveal(ID_STEPLIST);
}

static void sync_step_editor(void)
{
    Step *s = (g_sel >= 0 && g_sel < g_macro.nsteps) ? &g_macro.steps[g_sel] : NULL;
    int has = s != NULL, key = has && s->type == ST_KEY, clk = has && s->type == ST_CLICK;
    WCHAR b[64];
    ui_set_enabled(ID_ST_DEL, has);
    ui_set_enabled(ID_ST_UP, has && g_sel > 0);
    ui_set_enabled(ID_ST_DOWN, has && g_sel < g_macro.nsteps - 1);
    ui_set_vis(ID_ST_LBL, has); ui_set_vis(ID_ST_L2, has); ui_set_vis(ID_ST_L3, has);
    ui_set_vis(ID_ST_HINT, !has);
    ui_set_vis(ID_ST_KEY, key);
    ui_set_vis(ID_ST_B0, clk); ui_set_vis(ID_ST_B1, clk); ui_set_vis(ID_ST_B2, clk);
    ui_set_vis(ID_ST_HOLD, has); ui_set_vis(ID_ST_AFTER, has);
    if (!has) return;
    ui_set_text(ID_ST_LBL, key ? L"Tecla" : L"Botón");
    if (key) { key_name(s->code, b, 64); ui_set_text(ID_ST_KEY, b); }
    else {
        ui_set_val(ID_ST_B0, s->code == 0);
        ui_set_val(ID_ST_B1, s->code == 1);
        ui_set_val(ID_ST_B2, s->code == 2);
    }
    ui_set_val(ID_ST_HOLD, s->hold);
    ui_set_val(ID_ST_AFTER, s->after);
}

static void steps_from_simple(void)
{
    int T = clampi(g_macro.ms, 1, 100), h = (T * 24 + 50) / 100, a = (T * 9 + 50) / 100, n = 0;
    if (h < 1) h = 1;
    g_macro.steps[n].type = ST_KEY;   g_macro.steps[n].code = g_macro.key;    g_macro.steps[n].hold = h; g_macro.steps[n].after = a; n++;
    g_macro.steps[n].type = ST_CLICK; g_macro.steps[n].code = g_macro.button; g_macro.steps[n].hold = h; g_macro.steps[n].after = a; n++;
    if (g_macro.repeat) { g_macro.steps[n].type = ST_KEY; g_macro.steps[n].code = g_macro.key; g_macro.steps[n].hold = h; g_macro.steps[n].after = 0; n++; }
    g_macro.nsteps = n;
    g_sel = 0;
}

/* ===================================================================== */
/*  Ventana elegida                                                       */
/* ===================================================================== */
static void sync_target_labels(void)
{
    WCHAR b[200];
    const Macro *m = &g_macro;
    if (m->tgt_exe[0] || m->tgt_filter[0]) {
        ui_set_text(ID_TGT_L1, m->tgt_title[0] ? m->tgt_title : (m->tgt_exe[0] ? m->tgt_exe : L"(cualquier programa)"));
        if (m->tgt_exe[0] && m->tgt_filter[0]) wsprintfW(b, L"%s   ·   el título contiene «%s»", m->tgt_exe, m->tgt_filter);
        else if (m->tgt_exe[0])                wsprintfW(b, L"%s", m->tgt_exe);
        else                                   wsprintfW(b, L"El título contiene «%s»", m->tgt_filter);
        ui_set_text(ID_TGT_L2, b);
    } else {
        ui_set_text(ID_TGT_L1, L"(ninguna)");
        ui_set_text(ID_TGT_L2, m->tgt_on ? L"Elige una ventana para limitar la macro (mientras tanto funciona en todas)"
                                         : L"Sin ventana elegida: la macro funciona en cualquier lado");
    }
}

static void set_target(const WinItem *it)
{
    lock();
    lstrcpynW(g_macro.tgt_exe, it->exe, 64);
    lstrcpynW(g_macro.tgt_title, it->title, 96);
    g_macro.tgt_on = 1;
    unlock();
    if (!it->exe[0]) toast(L"No pude leer el programa de esa ventana", L"Si es de administrador, abre JJEXE como administrador", 1);
    else             toast(L"Ventana elegida", it->title[0] ? it->title : it->exe, 0);
    eng_target_changed();
    sync_ui();
    autosave();
}

/* ===================================================================== */
/*  Sincronizar toda la interfaz con el estado                            */
/* ===================================================================== */
static void edits_visible(void)
{
    int page = ui_get_page(), ov = ui_drop_open();
    if (g_hEditName)   ShowWindow(g_hEditName,   (page == 0 && !ov) ? SW_SHOW : SW_HIDE);
    if (g_hEditFilter) ShowWindow(g_hEditFilter, (page == 3 && !ov) ? SW_SHOW : SW_HIDE);
    if (g_hEditUpd)    ShowWindow(g_hEditUpd,    (page == 5 && !ov) ? SW_SHOW : SW_HIDE);
}

static void layout_edits(void)
{
    RECT r;
    ui_rect(ID_CFG_FRAME, &r);
    SetWindowPos(g_hEditName, NULL, S(r.left + 10), S(r.top + 6), S(r.right - r.left - 20), S(18), SWP_NOZORDER | SWP_NOACTIVATE);
    ui_rect(ID_TGT_FRAME, &r);
    SetWindowPos(g_hEditFilter, NULL, S(r.left + 10), S(r.top + 6), S(r.right - r.left - 20), S(18), SWP_NOZORDER | SWP_NOACTIVATE);
    ui_rect(ID_UPD_FRAME, &r);
    SetWindowPos(g_hEditUpd, NULL, S(r.left + 10), S(r.top + 6), S(r.right - r.left - 20), S(18), SWP_NOZORDER | SWP_NOACTIVATE);
    edits_visible();
}

static void apply_theme(int idx)
{
    const Theme *t;
    g_prefs.theme = clampi(idx, 0, g_ntheme - 1);
    ui_set_theme(g_prefs.theme);
    t = ui_theme();
    if (g_brInput) DeleteObject(g_brInput);
    g_brInput = CreateSolidBrush(t->input);
    if (g_hEditName)   InvalidateRect(g_hEditName, NULL, TRUE);
    if (g_hEditFilter) InvalidateRect(g_hEditFilter, NULL, TRUE);
    if (g_hEditUpd)    InvalidateRect(g_hEditUpd, NULL, TRUE);
}

static void sync_hotkey_buttons(void)
{
    WCHAR b[80];
    hk_name(g_prefs.hk_toggle, b, 80); ui_set_text(ID_HK_TOGGLE, b);
    hk_name(g_prefs.hk_next, b, 80);   ui_set_text(ID_HK_NEXT, b);
}

static void sync_ui(void)
{
    WCHAR b[96];
    const Macro *m = &g_macro;
    int i;
    sync_toggle();
    key_name(m->key, b, 96);
    ui_set_text(ID_KEY, b);
    ui_set_val(ID_MS, m->ms);
    ui_set_val(ID_REPEAT, m->repeat);
    ui_set_val(ID_MODE, m->mode);
    rebuild_steps();
    sync_step_editor();
    ui_set_val(ID_BTN_L, m->button == 0); ui_set_val(ID_BTN_R, m->button == 1);
    ui_set_val(ID_TRG_REL, m->release != 0); ui_set_val(ID_TRG_PRE, m->release == 0);
    sync_hotkey_buttons();
    ui_set_val(ID_TGT_ON, m->tgt_on);
    sync_target_labels();
    if (g_hEditFilter) SetWindowTextW(g_hEditFilter, m->tgt_filter);
    if (g_hEditName)   SetWindowTextW(g_hEditName, g_last);
    for (i = 0; i < g_ntheme; i++) {}
    ui_set_val(ID_TRAYMIN, g_prefs.tray_min);
    ui_set_val(ID_SOUND, g_prefs.sound);
    ui_set_val(ID_SND_VOL, g_prefs.snd_vol);
    ui_set_val(ID_UPD_ON, g_prefs.upd_on);
    if (g_hEditUpd) SetWindowTextW(g_hEditUpd, g_prefs.upd_url);
    ui_set_enabled(ID_UPD_OPEN, g_upd_open[0] != 0);
    ui_set_val(ID_OSD, g_prefs.osd);
    ui_set_val(ID_STREAM, g_prefs.stream);
    ui_set_val(ID_STREAM_MAIN, g_prefs.stream_main);
    ui_set_enabled(ID_STREAM_MAIN, g_prefs.stream);
    reload_configs();
    reload_anims();
    reload_sounds();
    ui_invalidate();
}

/* ===================================================================== */
/*  Atajos globales                                                       */
/* ===================================================================== */
static void unregister_hotkeys(void)
{
    UnregisterHotKey(g_hwnd, HK_TOGGLE);
    UnregisterHotKey(g_hwnd, HK_NEXT);
    eng_set_mouse_hotkeys(0, 0);
}

static void register_hotkeys(void)
{
    WCHAR n[80], msg[160];
    int t = g_prefs.hk_toggle, x = g_prefs.hk_next;
    unregister_hotkeys();
    if (x && x == t) x = 0;                                      /* no puede ser el mismo atajo */
    eng_set_mouse_hotkeys(hk_is_mouse(t) ? t : 0, hk_is_mouse(x) ? x : 0);
    hook_update();
    if (t && !hk_is_mouse(t) && !RegisterHotKey(g_hwnd, HK_TOGGLE, (UINT)hk_mods(t) | MOD_NOREPEAT, (UINT)hk_vk(t))) {
        hk_name(t, n, 80);
        wsprintfW(msg, L"«%s» ya la usa otro programa: elige otra en «Disparo»", n);
        toast(L"Atajo ocupado", msg, 1);
    }
    if (x && !hk_is_mouse(x) && !RegisterHotKey(g_hwnd, HK_NEXT, (UINT)hk_mods(x) | MOD_NOREPEAT, (UINT)hk_vk(x))) {
        hk_name(x, n, 80);
        wsprintfW(msg, L"«%s» ya la usa otro programa", n);
        toast(L"Atajo ocupado", msg, 1);
    }
}

/* ===================================================================== */
/*  Configuraciones con nombre                                            */
/* ===================================================================== */
static int name_from_edit(WCHAR *out)
{
    WCHAR t[128], *a = t, *e, *q;
    GetWindowTextW(g_hEditName, t, 128);
    while (*a == L' ') a++;
    e = a + lstrlenW(a);
    while (e > a && e[-1] == L' ') e--;
    *e = 0;
    if (!*a) { toast(L"Escribe un nombre para la configuración", NULL, 1); return 0; }
    for (q = a; *q; q++)
        if (*q == L'[' || *q == L']') { toast(L"El nombre no puede llevar [ ni ]", NULL, 1); return 0; }
    if (lstrlenW(a) >= CFG_NAME_MAX) { toast(L"El nombre es muy largo (máximo 40 letras)", NULL, 1); return 0; }
    lstrcpyW(out, a);
    return 1;
}

static void apply_named(const WCHAR *name)
{
    WCHAR sec[CFG_NAME_MAX + 8];
    Macro m = g_macro;
    wsprintfW(sec, L"Config:%s", name);
    if (!cfg_read(g_ini, sec, &m, NULL)) { toast(L"No se encontró esa configuración", NULL, 1); return; }
    lock(); g_macro = m; unlock();
    lstrcpynW(g_last, name, CFG_NAME_MAX);
    g_sel = g_macro.nsteps > 0 ? 0 : -1;
    eng_target_changed();
    sync_ui();
    autosave();
    toast(L"Configuración", name, 0);
}

static void cfg_save_current(void)
{
    WCHAR name[CFG_NAME_MAX], sec[CFG_NAME_MAX + 8], msg[160];
    Macro old;
    if (!name_from_edit(name)) return;
    wsprintfW(sec, L"Config:%s", name);
    macro_defaults(&old);
    if (cfg_read(g_ini, sec, &old, NULL)) {
        wsprintfW(msg, L"Ya existe la configuración «%s».\n¿Quieres reemplazarla?", name);
        if (MessageBoxW(g_hwnd, msg, L"JJEXE", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    cfg_write(g_ini, sec, &g_macro, NULL, NULL);
    lstrcpynW(g_last, name, CFG_NAME_MAX);
    reload_configs();
    SetWindowTextW(g_hEditName, name);
    autosave();
    toast(L"Configuración guardada", name, 0);
}

static void cfg_delete_current(void)
{
    WCHAR name[CFG_NAME_MAX], sec[CFG_NAME_MAX + 8], msg[160];
    Macro tmp;
    if (!name_from_edit(name)) return;
    wsprintfW(sec, L"Config:%s", name);
    macro_defaults(&tmp);
    if (!cfg_read(g_ini, sec, &tmp, NULL)) { toast(L"Esa configuración no existe", NULL, 1); return; }
    wsprintfW(msg, L"¿Borrar la configuración «%s»?", name);
    if (MessageBoxW(g_hwnd, msg, L"JJEXE", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    cfg_delete(g_ini, name);
    if (!lstrcmpiW(g_last, name)) g_last[0] = 0;
    SetWindowTextW(g_hEditName, L"");
    reload_configs();
    autosave();
    toast(L"Configuración borrada", NULL, 0);
}

static int file_dialog(int save, const WCHAR *filter, const WCHAR *defext, WCHAR *path, const WCHAR *title)
{
    OPENFILENAMEW o;
    ZeroMemory(&o, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = g_hwnd;
    o.lpstrFilter = filter;
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = defext;
    o.lpstrTitle = title;
    o.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
}

static void cfg_export(void)
{
    WCHAR name[CFG_NAME_MAX], sec[CFG_NAME_MAX + 8], path[MAX_PATH], *c;
    if (!name_from_edit(name)) return;
    lstrcpynW(path, name, MAX_PATH - 8);
    for (c = path; *c; c++) if (wcschr(L"\\/:*?\"<>|", *c)) *c = L'_';
    if (!file_dialog(1, L"Configuración de JJEXE (*.jjcfg)\0*.jjcfg\0Todos los archivos\0*.*\0", L"jjcfg", path, L"Exportar configuración"))
        return;
    DeleteFileW(path);
    ini_prepare(path);
    wsprintfW(sec, L"Config:%s", name);
    cfg_write(path, sec, &g_macro, NULL, NULL);
    toast(L"Configuración exportada", name, 0);
}

static void cfg_import(void)
{
    static WCHAR names[64][CFG_NAME_MAX];
    WCHAR path[MAX_PATH], sec[CFG_NAME_MAX + 8], msg[160], first[CFG_NAME_MAX];
    Macro m, tmp;
    int n, i, done = 0;
    path[0] = 0;
    if (!file_dialog(0, L"Configuración de JJEXE (*.jjcfg;*.ini)\0*.jjcfg;*.ini\0Todos los archivos\0*.*\0", L"jjcfg", path, L"Importar configuración"))
        return;
    n = cfg_names(path, names, 64);
    if (n == 0) { MessageBoxW(g_hwnd, L"Ese archivo no tiene configuraciones de JJEXE.", L"JJEXE", MB_ICONINFORMATION); return; }
    first[0] = 0;
    for (i = 0; i < n; i++) {
        wsprintfW(sec, L"Config:%s", names[i]);
        macro_defaults(&tmp);
        if (cfg_read(g_ini, sec, &tmp, NULL)) {
            wsprintfW(msg, L"Ya existe la configuración «%s».\n¿Quieres reemplazarla con la importada?", names[i]);
            if (MessageBoxW(g_hwnd, msg, L"JJEXE", MB_YESNO | MB_ICONQUESTION) != IDYES) continue;
        }
        macro_defaults(&m);
        if (!cfg_read(path, sec, &m, NULL)) continue;
        cfg_write(g_ini, sec, &m, NULL, NULL);
        if (!first[0]) lstrcpyW(first, names[i]);
        done++;
    }
    reload_configs();
    if (done) {
        wsprintfW(msg, L"%d configuración(es)", done);
        toast(L"Importado", msg, 0);
        apply_named(first);
    }
}

static void cfg_next(void)
{
    static WCHAR names[64][CFG_NAME_MAX];
    int n = names_sorted(names, 64), i, cur = -1;
    if (n == 0) { toast(L"No hay configuraciones guardadas", L"Guarda una en la pestaña «Macro»", 1); return; }
    for (i = 0; i < n; i++) if (!lstrcmpiW(names[i], g_last)) cur = i;
    apply_named(names[(cur + 1) % n]);
}

/* ===================================================================== */
/*  Acciones de la interfaz                                               */
/* ===================================================================== */
static void add_step(int type)
{
    Step s;
    int pos;
    if (g_macro.nsteps >= MAXSTEPS) { toast(L"Máximo de pasos alcanzado", NULL, 1); return; }
    s.type = type; s.code = (type == ST_KEY) ? g_macro.key : 0; s.hold = 3; s.after = 1;
    pos = (g_sel >= 0) ? g_sel + 1 : g_macro.nsteps;
    lock();
    memmove(&g_macro.steps[pos + 1], &g_macro.steps[pos], (size_t)(g_macro.nsteps - pos) * sizeof(Step));
    g_macro.steps[pos] = s;
    g_macro.nsteps++;
    unlock();
    g_sel = pos;
}

static void step_changed(int final)
{
    WCHAR b[96];
    if (g_sel >= 0 && g_sel < g_macro.nsteps) {
        step_text(g_sel, b);
        /* actualizar el texto de la fila sin reconstruir toda la lista */
        rebuild_steps();
    }
    ui_invalidate();
    if (final) autosave();
}

static int is_modifier_vk(int vk)
{
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN || (vk >= VK_LSHIFT && vk <= VK_RMENU);
}

static int ui_mods(void)
{
    return ((GetKeyState(VK_MENU) & 0x8000) ? 1 : 0) | ((GetKeyState(VK_CONTROL) & 0x8000) ? 2 : 0) |
           ((GetKeyState(VK_SHIFT) & 0x8000) ? 4 : 0) | (((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) ? 8 : 0);
}

static void hk_captured(int id, int code)
{
    WCHAR n[80];
    if (code > 0) {
        int vk = code & 0xFF, mods = (code >> 8) & 0xF;
        int plain = (mods == 0) && ((vk >= 0x30 && vk <= 0x5A) || vk == VK_SPACE || vk == VK_RETURN || vk == VK_BACK || vk == VK_TAB);
        if (id == ID_HK_TOGGLE) { g_prefs.hk_toggle = code; if (g_prefs.hk_next == code) g_prefs.hk_next = 0; }
        else                    { g_prefs.hk_next = code;   if (g_prefs.hk_toggle == code) g_prefs.hk_toggle = 0; }
        hk_name(code, n, 80);
        if (plain) toast(L"Ojo: esa tecla ya no se escribirá en otros programas", n, 1);
        else       toast(L"Atajo guardado", n, 0);
        autosave();
    }
    register_hotkeys();                                          /* se habian soltado mientras elegias */
    sync_ui();
}

void on_ui_capture(int id, int code)
{
    if (id == ID_HK_TOGGLE || id == ID_HK_NEXT) { hk_captured(id, code); return; }
    if (code >= 0) {
        if (id == ID_KEY) { g_macro.key = code; }
        else if (id == ID_ST_KEY && g_sel >= 0 && g_sel < g_macro.nsteps && g_macro.steps[g_sel].type == ST_KEY)
            g_macro.steps[g_sel].code = code;
        autosave();
    }
    sync_ui();
}

void on_ui(int id, int val, int final)
{
    Step *st = (g_sel >= 0 && g_sel < g_macro.nsteps) ? &g_macro.steps[g_sel] : NULL;
    WCHAR b[MAX_PATH];

    switch (id) {
    /* ---- general ---- */
    case ID_TOGGLE:  set_enabled(!eng_get_enabled()); return;
    case ID_OVERLAY: edits_visible(); return;
    case ID_TAB0: case ID_TAB1: case ID_TAB2: case ID_TAB3: case ID_TAB4: case ID_TAB5:
        if (ID_TAB0 + 3 == id && 0) {}
        edits_visible();
        return;

    /* ---- configuraciones ---- */
    case ID_CFG_DROP:
        if (val < 0) { reload_configs(); return; }
        apply_named(ui_items_get(ID_CFG_DROP, val));
        return;
    case ID_CFG_SAVE:   cfg_save_current(); return;
    case ID_CFG_DEL:    cfg_delete_current(); return;
    case ID_CFG_IMPORT: cfg_import(); return;
    case ID_CFG_EXPORT: cfg_export(); return;

    /* ---- secuencia simple ---- */
    case ID_KEY:    ui_capture_begin(ID_KEY); SetFocus(g_hwnd); return;
    case ID_MS:     g_macro.ms = val; ui_invalidate(); if (final) autosave(); return;
    case ID_REPEAT: g_macro.repeat = val; ui_invalidate(); autosave(); return;

    /* ---- secuencia personalizada ---- */
    case ID_MODE:
        lock(); g_macro.mode = val; unlock();
        if (val && g_macro.nsteps == 0) { lock(); steps_from_simple(); unlock(); sync_ui(); }
        ui_invalidate();
        autosave();
        return;
    case ID_STEPLIST: g_sel = val; sync_step_editor(); ui_invalidate(); return;
    case ID_ST_ADDK:  add_step(ST_KEY);   rebuild_steps(); sync_step_editor(); autosave(); return;
    case ID_ST_ADDC:  add_step(ST_CLICK); rebuild_steps(); sync_step_editor(); autosave(); return;
    case ID_ST_DEL:
        if (!st) return;
        lock();
        memmove(&g_macro.steps[g_sel], &g_macro.steps[g_sel + 1], (size_t)(g_macro.nsteps - g_sel - 1) * sizeof(Step));
        g_macro.nsteps--;
        unlock();
        if (g_sel >= g_macro.nsteps) g_sel = g_macro.nsteps - 1;
        rebuild_steps(); sync_step_editor(); autosave();
        return;
    case ID_ST_UP:
    case ID_ST_DOWN: {
        int o = (id == ID_ST_UP) ? g_sel - 1 : g_sel + 1;
        Step t;
        if (!st || o < 0 || o >= g_macro.nsteps) return;
        lock(); t = g_macro.steps[o]; g_macro.steps[o] = g_macro.steps[g_sel]; g_macro.steps[g_sel] = t; unlock();
        g_sel = o;
        rebuild_steps(); sync_step_editor(); autosave();
        return; }
    case ID_ST_COPY:
        if (g_macro.nsteps > 0 &&
            MessageBoxW(g_hwnd, L"¿Reemplazar los pasos actuales por los de la secuencia simple?", L"JJEXE", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        lock(); steps_from_simple(); unlock();
        rebuild_steps(); sync_step_editor(); autosave();
        return;
    case ID_ST_KEY: if (st && st->type == ST_KEY) { ui_capture_begin(ID_ST_KEY); SetFocus(g_hwnd); } return;
    case ID_ST_B0: case ID_ST_B1: case ID_ST_B2:
        if (st && st->type == ST_CLICK) { st->code = id - ID_ST_B0; step_changed(1); }
        return;
    case ID_ST_HOLD:  if (st) { st->hold  = val; step_changed(final); } return;
    case ID_ST_AFTER: if (st) { st->after = val; step_changed(final); } return;

    /* ---- disparo ---- */
    case ID_BTN_L: g_macro.button = 0; ui_invalidate(); autosave(); return;
    case ID_BTN_R: g_macro.button = 1; ui_invalidate(); autosave(); return;
    case ID_TRG_REL: g_macro.release = 1; autosave(); return;
    case ID_TRG_PRE: g_macro.release = 0; autosave(); return;
    case ID_HK_TOGGLE:
    case ID_HK_NEXT:
        unregister_hotkeys();                                    /* para que tambien se pueda elegir la tecla que ya esta en uso */
        ui_capture_begin(id);
        ui_set_text(id, L"Presiona la tecla…");
        SetFocus(g_hwnd);
        return;
    case ID_HK_TOGGLE_CLR: g_prefs.hk_toggle = 0; register_hotkeys(); sync_ui(); autosave(); return;
    case ID_HK_NEXT_CLR:   g_prefs.hk_next = 0;   register_hotkeys(); sync_ui(); autosave(); return;

    /* ---- ventana elegida ---- */
    case ID_TGT_ON:
        g_macro.tgt_on = val; eng_target_changed(); sync_target_labels(); autosave();
        return;
    case ID_TGT_PICK:
        if (g_pick_left) { pick_stop(); sync_target_labels(); return; }
        g_pick_left = 5;
        SetTimer(g_hwnd, TIMER_PICK, 1000, NULL);
        ui_set_text(ID_TGT_PICK, L"Cancelar");
        pick_labels();
        return;
    case ID_TGT_CLEAR:
        lock(); g_macro.tgt_exe[0] = 0; g_macro.tgt_title[0] = 0; g_macro.tgt_filter[0] = 0; g_macro.tgt_on = 0; unlock();
        SetWindowTextW(g_hEditFilter, L"");
        eng_target_changed(); sync_ui(); autosave();
        return;
    case ID_TGT_REFRESH: reload_wins(); ui_invalidate(); return;
    case ID_TGT_LIST:
        if (val < 0) { reload_wins(); return; }
        if (val < g_nwins) set_target(&g_wins[val]);
        return;

    /* ---- aspecto ---- */
    case ID_TH0: case ID_TH1: case ID_TH2: case ID_TH3: case ID_TH4: case ID_TH5:
        apply_theme(val); autosave();
        return;
    case ID_ANIM_DROP:
        if (val < 0) { reload_anims(); return; }
        if (val == 0) g_prefs.anim[0] = 0;
        else if (val == 1) lstrcpyW(g_prefs.anim, L"*none");
        else if (val - 2 < N_BI) lstrcpyW(g_prefs.anim, k_bi_id[val - 2]);
        else if (val - 2 - N_BI < g_nanims) lstrcpynW(g_prefs.anim, g_anims[val - 2 - N_BI], 128);
        autosave();
        return;
    case ID_ANIM_ADD: {
        WCHAR src[MAX_PATH], dst[MAX_PATH], *f;
        src[0] = 0;
        if (!file_dialog(0, L"Imágenes y GIF (*.gif;*.png;*.jpg;*.jpeg;*.bmp)\0*.gif;*.png;*.jpg;*.jpeg;*.bmp\0Todos los archivos\0*.*\0", NULL, src, L"Elegir tu animación"))
            return;
        f = src + lstrlenW(src);
        while (f > src && f[-1] != L'\\') f--;
        anim_path(f, dst, MAX_PATH);
        if (!CopyFileW(src, dst, FALSE)) { toast(L"No pude copiar el archivo", NULL, 1); return; }
        lstrcpynW(g_prefs.anim, f, 128);
        reload_anims();
        autosave();
        toast(L"Animación agregada", f, 0);
        return; }
    case ID_ANIM_FOLDER:
        wsprintfW(b, L"%s\\Animaciones", ini_dir());
        ShellExecuteW(g_hwnd, L"open", b, NULL, NULL, SW_SHOWNORMAL);
        return;
    case ID_ANIM_TEST:
        if (!lstrcmpW(g_prefs.anim, L"*none")) { toast(L"La animación está desactivada", L"Elige otra en la lista para probarla", 1); return; }
        run_splash(g_inst);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    case ID_TRAYMIN: g_prefs.tray_min = val; autosave(); return;
    case ID_SOUND:   g_prefs.sound = val;    autosave(); return;
    case ID_SND_ON: case ID_SND_OFF: {
        WCHAR *dst = id == ID_SND_ON ? g_prefs.snd_on : g_prefs.snd_off;
        if (val < 0) { reload_sounds(); return; }
        if (val == 0) dst[0] = 0;
        else if (val == 1) lstrcpyW(dst, L"*none");
        else if (val - 2 < g_nsnds) lstrcpynW(dst, g_snds[val - 2], 128);
        autosave();
        if (val != 1) snd_test(id == ID_SND_ON);
        return; }
    case ID_SND_ON_TEST:  snd_test(1); return;
    case ID_SND_OFF_TEST: snd_test(0); return;
    case ID_SND_VOL:
        g_prefs.snd_vol = val; ui_invalidate();
        if (final) { autosave(); snd_test(1); }
        return;
    case ID_SND_ADD: {
        WCHAR src[MAX_PATH], dst[MAX_PATH], *f;
        src[0] = 0;
        if (!file_dialog(0, L"Sonidos WAV (*.wav)\0*.wav\0Todos los archivos\0*.*\0", NULL, src, L"Elegir un sonido"))
            return;
        f = src + lstrlenW(src);
        while (f > src && f[-1] != L'\\') f--;
        snd_scan(g_snds, 1);                                   /* asegura que exista la carpeta Sonidos */
        snd_path(f, dst, MAX_PATH);
        if (!CopyFileW(src, dst, FALSE)) { toast(L"No pude copiar el archivo", NULL, 1); return; }
        lstrcpynW(g_prefs.snd_on, f, 128);                     /* queda elegido para "al activar"; el de desactivar se cambia en su lista */
        reload_sounds();
        autosave();
        toast(L"Sonido agregado", f, 0);
        return; }
    case ID_SND_FOLDER:
        snd_scan(g_snds, 1);
        wsprintfW(b, L"%s\\Sonidos", ini_dir());
        ShellExecuteW(g_hwnd, L"open", b, NULL, NULL, SW_SHOWNORMAL);
        return;
    case ID_UPD_ON:  g_prefs.upd_on = val; autosave(); return;
    case ID_UPD_NOW:
        ui_set_text(ID_UPD_STATUS, L"Buscando…");
        upd_check(g_hwnd, g_prefs.upd_url, 1);
        return;
    case ID_UPD_OPEN:
        if (g_upd_open[0]) ShellExecuteW(g_hwnd, L"open", g_upd_open, NULL, NULL, SW_SHOWNORMAL);
        return;
    case ID_OSD:     g_prefs.osd = val;      autosave(); return;
    case ID_STREAM:
        g_prefs.stream = val; ui_set_enabled(ID_STREAM_MAIN, val); apply_stream(); autosave();
        if (val) toast(L"Modo stream activado", L"Este aviso no sale en grabaciones ni transmisiones", 1);
        return;
    case ID_STREAM_MAIN: g_prefs.stream_main = val; apply_stream(); autosave(); return;
    }
}

/* ===================================================================== */
/*  Ventana principal                                                     */
/* ===================================================================== */
static void tray_menu(void)
{
    HMENU m = CreatePopupMenu();
    POINT pt;
    int cmd;
    AppendMenuW(m, MF_STRING, CMD_SHOW, L"Mostrar JJEXE");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, CMD_TOGGLE, eng_get_enabled() ? L"Desactivar la macro" : L"Activar la macro");
    AppendMenuW(m, MF_STRING, CMD_NEXT, L"Siguiente configuración");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, CMD_EXIT, L"Salir");
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(m);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    switch (cmd) {
    case CMD_SHOW:   show_main(); break;
    case CMD_TOGGLE: set_enabled(!eng_get_enabled()); break;
    case CMD_NEXT:   cfg_next(); break;
    case CMD_EXIT:   g_quitting = 1; DestroyWindow(g_hwnd); break;
    }
}

static HWND make_edit(HWND parent, int id)
{
    HWND e = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(e, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(e, EM_LIMITTEXT, 60, 0);
    return e;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_hEditName   = make_edit(hwnd, 9001);
        g_hEditFilter = make_edit(hwnd, 9002);
        g_hEditUpd    = make_edit(hwnd, 9003);
        SendMessageW(g_hEditName, EM_LIMITTEXT, CFG_NAME_MAX - 1, 0);
        SendMessageW(g_hEditUpd, EM_LIMITTEXT, 250, 0);
        ui_init(hwnd);
        apply_theme(g_prefs.theme);
        sync_ui();
        layout_edits();
        register_hotkeys();
        tray_update();
        apply_stream();
        if (g_prefs.upd_on && g_prefs.upd_url[0]) upd_check(hwnd, g_prefs.upd_url, 0);      /* al abrir: busca en segundo plano */
        return 0; }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        ui_paint(dc, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        return 0; }
    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
        if (!g_tracking) {
            TRACKMOUSEEVENT te;
            te.cbSize = sizeof te; te.dwFlags = TME_LEAVE; te.hwndTrack = hwnd; te.dwHoverTime = 0;
            TrackMouseEvent(&te);
            g_tracking = 1;
        }
        ui_mouse(hwnd, WM_MOUSEMOVE, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_MOUSELEAVE:
        g_tracking = 0;
        ui_mouse(hwnd, WM_MOUSELEAVE, 0, 0);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        ui_mouse(hwnd, WM_LBUTTONDOWN, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        ui_mouse(hwnd, WM_LBUTTONUP, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = (short)LOWORD(lp); pt.y = (short)HIWORD(lp);
        ScreenToClient(hwnd, &pt);
        ui_wheel(GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y);
        return 0; }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && ui_is_hot()) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        break;

    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (ui_capturing()) {                                      /* esperando que presiones una tecla */
            int cid = ui_capture_id();
            if (cid == ID_HK_TOGGLE || cid == ID_HK_NEXT) {         /* atajo: cualquier tecla, sola o con Ctrl / Alt / Shift */
                if (wp == VK_ESCAPE) ui_capture_end(-1);
                else if (!is_modifier_vk((int)wp)) ui_capture_end((int)wp | (ui_mods() << 8));
                return 0;
            }
            if (wp == VK_ESCAPE) ui_capture_end(-1);
            else ui_capture_end((int)(((lp >> 16) & 0xFF) | (((lp >> 24) & 1) << 8)));
            return 0;
        }
        if (ui_key(hwnd, wp, lp)) return 0;
        break;

    case WM_XBUTTONDOWN: case WM_MBUTTONDOWN: {                    /* atajo con un boton del mouse */
        int cid = ui_capturing() ? ui_capture_id() : 0;
        if (cid == ID_HK_TOGGLE || cid == ID_HK_NEXT) {
            int b = (msg == WM_MBUTTONDOWN) ? 0x04 : (HIWORD(wp) == XBUTTON1 ? 0x05 : 0x06);
            ui_capture_end(b | (ui_mods() << 8));
        }
        return (msg == WM_XBUTTONDOWN) ? TRUE : 0; }
    case WM_APP_HOTKEY:
        if (wp == 1) set_enabled(!eng_get_enabled());
        else if (wp == 2) cfg_next();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && ui_capturing()) ui_capture_end(-1);     /* si te vas a otra ventana, se cancela */
        break;

    case WM_CTLCOLOREDIT: {
        const Theme *t = ui_theme();
        SetTextColor((HDC)wp, t->text);
        SetBkColor((HDC)wp, t->input);
        return (LRESULT)g_brInput; }
    case WM_COMMAND:
        if ((HWND)lp == g_hEditFilter) {
            if (HIWORD(wp) == EN_CHANGE) {
                GetWindowTextW(g_hEditFilter, g_macro.tgt_filter, 64);
                eng_target_changed();
                sync_target_labels();
                autosave();
            }
            if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) InvalidateRect(hwnd, NULL, FALSE);
        } else if ((HWND)lp == g_hEditUpd) {
            if (HIWORD(wp) == EN_CHANGE) {
                GetWindowTextW(g_hEditUpd, g_prefs.upd_url, 256);
                autosave();
            }
            if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) InvalidateRect(hwnd, NULL, FALSE);
        } else if ((HWND)lp == g_hEditName) {
            if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_APP_UPDATE: {
        UpdRes *r = (UpdRes *)lp;
        if (!r) return 0;
        if (r->status == 1) {
            WCHAR t[96];
            lstrcpynW(g_upd_open, r->url, 256);
            wsprintfW(t, L"Hay una versión nueva: v%d", r->ver);
            ui_set_text(ID_UPD_STATUS, t);
            ui_set_enabled(ID_UPD_OPEN, g_upd_open[0] != 0);
            toast(t, r->note[0] ? r->note : (g_upd_open[0] ? L"Ábrela desde la pestaña Extras" : NULL), 1);
        } else if (r->status == 0) {
            if (r->manual) { ui_set_text(ID_UPD_STATUS, L"Estás al día"); toast(L"JJEXE está al día", NULL, 1); }
        } else if (r->manual) {
            ui_set_text(ID_UPD_STATUS, r->status == -2 ? L"Falta la dirección https" : L"No se pudo comprobar");
            toast(r->status == -2 ? L"Escribe primero la dirección https" : L"No pude comprobar la versión", r->status == -2 ? NULL : L"Revisa la dirección y tu conexión", 1);
        }
        HeapFree(GetProcessHeap(), 0, r);
        ui_invalidate();
        return 0; }

    case WM_HOTKEY:
        if (wp == HK_TOGGLE) set_enabled(!eng_get_enabled());
        else if (wp == HK_NEXT) cfg_next();
        return 0;

    case WM_TIMER:
        if (wp == TIMER_PICK) {
            if (--g_pick_left > 0) { pick_labels(); return 0; }
            {
                HWND fg = GetForegroundWindow();
                WinItem it;
                DWORD pid = 0;
                pick_stop();
                if (fg) GetWindowThreadProcessId(fg, &pid);
                if (!fg || pid == GetCurrentProcessId() || !win_info(fg, &it)) {
                    sync_target_labels();
                    toast(L"No cambiaste de ventana", L"Pulsa otra vez y ve al juego antes de que llegue a 0", 1);
                } else set_target(&it);
            }
        }
        return 0;

    case WM_APP_TRAY:
        if (lp == WM_LBUTTONUP || lp == WM_LBUTTONDBLCLK) show_main();
        else if (lp == WM_RBUTTONUP) tray_menu();
        return 0;

    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_MINIMIZE && g_prefs.tray_min) { ShowWindow(hwnd, SW_HIDE); return 0; }
        break;
    case WM_CLOSE:
        if (g_prefs.tray_min && !g_quitting) { ShowWindow(hwnd, SW_HIDE); return 0; }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, HK_TOGGLE);
        UnregisterHotKey(hwnd, HK_NEXT);
        tray_remove();
        eng_hook_remove();
        if (g_osd) DestroyWindow(g_osd);
        PostQuitMessage(0);
        return 0;
    default:
        if (g_msgTaskbar && msg == g_msgTaskbar) { g_tray_added = 0; tray_update(); return 0; }
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void make_fonts(void)
{
    NONCLIENTMETRICSW ncm;
    ZeroMemory(&ncm, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0)) {
        LOGFONTW f = ncm.lfMessageFont;
        g_font = CreateFontIndirectW(&f);
        f.lfWeight = FW_BOLD;  g_fontB = CreateFontIndirectW(&f);
        f.lfWeight = FW_NORMAL; f.lfHeight = f.lfHeight * 9 / 10;  g_fontS = CreateFontIndirectW(&f);
        f.lfWeight = FW_BOLD;  g_fontSB = CreateFontIndirectW(&f);
        f = ncm.lfMessageFont; f.lfWeight = FW_BOLD; f.lfHeight = f.lfHeight * 14 / 10; g_fontBig = CreateFontIndirectW(&f);
    }
    if (!g_font)    g_font    = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_fontB)   g_fontB   = g_font;
    if (!g_fontS)   g_fontS   = g_font;
    if (!g_fontSB)  g_fontSB  = g_fontB;
    if (!g_fontBig) g_fontBig = g_fontB;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    const WCHAR *title = L"JJEXE";
    HANDLE mtx;
    WNDCLASSEXW wc;
    RECT r;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    MSG m;
    HDC hdc;
    (void)hPrev; (void)cmd;
    g_inst = hInst;

    mtx = CreateMutexW(NULL, TRUE, L"Local\\JJEXE_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"JJEXE ya está abierto (revisa la bandeja del sistema).", title, MB_ICONINFORMATION);
        return 0;
    }

    {   /* texto nitido en pantallas con escalado */
        typedef BOOL (WINAPI *PFN_DPI)(void);
        HMODULE u = GetModuleHandleW(L"user32.dll");
        PFN_DPI fn = u ? (PFN_DPI)(void *)GetProcAddress(u, "SetProcessDPIAware") : NULL;
        if (fn) fn();
    }
    hdc = GetDC(NULL);
    g_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    if (g_dpi < 96) g_dpi = 96;

    gp_start();
    make_fonts();
    eng_init();
    timer_res_begin();
    ini_init();
    cfg_read(g_ini, L"Actual", &g_macro, &g_prefs);          /* lo ultimo que usaste (incluye tema y animacion) */
    cfg_get_last(g_last, CFG_NAME_MAX);
    g_prefs.theme = clampi(g_prefs.theme, 0, g_ntheme - 1);
    g_msgTaskbar = RegisterWindowMessageW(L"TaskbarCreated");

    g_icoApp   = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(RES_ICON_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    g_icoAppSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(RES_ICON_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    g_icoOn    = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(RES_ICON_ON),  IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    g_icoOff   = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(RES_ICON_OFF), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!g_icoApp)   g_icoApp   = LoadIconW(NULL, IDI_APPLICATION);
    if (!g_icoAppSm) g_icoAppSm = g_icoApp;
    if (!g_icoOn)    g_icoOn    = g_icoAppSm;
    if (!g_icoOff)   g_icoOff   = g_icoAppSm;

    run_splash(hInst);                                      /* animacion de inicio (la tuya, la de JJEXE o ninguna) */

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = g_icoApp;
    wc.hIconSm = g_icoAppSm;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"JJEXEMain";
    RegisterClassExW(&wc);

    r.left = 0; r.top = 0; r.right = S(440); r.bottom = S(560);
    AdjustWindowRect(&r, style, FALSE);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    eng_set_enabled(0);
    eng_shutdown();
    timer_res_end();
    ui_shutdown();
    gp_stop();
    if (mtx) CloseHandle(mtx);
    return (int)m.wParam;
}
