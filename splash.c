/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠​‌​‌‌​‌​​​​‌‌​​‌​​​​​‌​​​‌‌‌​‌‌‌​‌​‌​​‌‌‌‌‌​​‌​‌⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 5a19047753e5
 */

#include "jj.h"
#include <math.h>

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_splash[] = "wm:5a19047753e5";

/* ===================================================================== */
/*  Animacion de arranque: pantalla de carga con los colores del logo.    */
/*  Clic o cualquier tecla la omite.                                      */
/* ===================================================================== */
#define SPL_W     520                /* medidas logicas de la ventana          */
#define SPL_H     292
#define SPL_RUN   2400               /* ms que tarda en llenarse la barra      */
#define SPL_IN    250                /* ms de aparicion gradual                */
#define SPL_HOLD  300                /* ms que se queda al 100 %               */
#define SPL_OUT   260                /* ms de desaparicion gradual             */
#define SPL_LINES 64                 /* lineas de "codigo" por columna         */

typedef struct {
    int       w, h;                          /* tamano real en pixeles                    */
    ULONGLONG t0;                            /* instante de inicio                        */
    LONGLONG  skip_at;                       /* ms en que se pidio omitir (-1 = no)       */
    unsigned  rng;
    LONG      next_glitch, glitch_until;
    int       last_alpha, ew, eh;
    HDC       mem, bg, ovl, blk, tmp, edc;   /* lienzo, fondo, capa CRT, negro, franja, emblema */
    HBITMAP   mem_b, bg_b, ovl_b, blk_b, tmp_b, emblem;
    HFONT     fCode, fTitle, fPct, fSmall;
    WCHAR     lines[2][SPL_LINES][44];
} Splash;

static Splash g_spl;

static const WCHAR *k_msgs[] = {
    L"JJEXE  macro engine",
    L"[ OK ] timer resolution 1 ms",
    L"[ OK ] settings loaded",
    L"mouse.hook ........ ready",
    L"macro.engine ...... ready",
    L"hotkey ............ armed",
    L"seq: key > click > key",
    L"step 1/6   dt = 0.24 ms",
    L"cfg: JJEXE.ini  [Actual]",
    L"[ OK ] sendinput online",
    L"input.queue ....... empty",
    L"0x7FF6A1C0  mov rax,[rbp-8]"
};

static unsigned spl_rnd(unsigned n)
{
    g_spl.rng = g_spl.rng * 1664525u + 1013904223u;
    return (g_spl.rng >> 8) % n;
}

static HDC spl_dc(HDC ref, int w, int h, HBITMAP *bmp)
{
    HDC dc = CreateCompatibleDC(ref);
    *bmp = CreateCompatibleBitmap(ref, w, h);
    if (dc && *bmp) SelectObject(dc, *bmp);
    return dc;
}

static void spl_fill(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r;
    HBRUSH b = CreateSolidBrush(c);
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void spl_frame(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r;
    HBRUSH b = CreateSolidBrush(c);
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    FrameRect(dc, &r, b);
    DeleteObject(b);
}

static HFONT spl_font(int px, int weight)
{
    return CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       FIXED_PITCH | FF_MODERN, L"Consolas");
}

/* Texto con un halo azul alrededor (efecto neon). */
static void spl_text(HDC dc, int x, int y, const WCHAR *t, int n, COLORREF c, COLORREF glow)
{
    int d = S(1) > 1 ? S(1) : 1;
    SetTextColor(dc, glow);
    TextOutW(dc, x - d, y, t, n);
    TextOutW(dc, x + d, y, t, n);
    TextOutW(dc, x, y - d, t, n);
    TextOutW(dc, x, y + d, t, n);
    SetTextColor(dc, c);
    TextOutW(dc, x, y, t, n);
}

/* Avance de la barra en milesimas (0..1000): rapido, con un par de pausas, como una carga real. */
static int spl_progress(ULONGLONG el)
{
    static const int tt[8] = { 0, 300, 700, 1000, 1500, 1800, 2150, SPL_RUN };
    static const int pp[8] = { 0, 140, 330,  345,  620,  640,  930, 1000 };
    int i;
    if (el >= (ULONGLONG)SPL_RUN) return 1000;
    for (i = 1; i < 8; i++)
        if (el < (ULONGLONG)tt[i]) break;
    return pp[i - 1] + (int)((pp[i] - pp[i - 1]) * (LONGLONG)(el - (ULONGLONG)tt[i - 1]) / (tt[i] - tt[i - 1]));
}

static void spl_draw(ULONGLONG el)
{
    static const COLORREF pal[5] = { RGB(12, 46, 100), RGB(18, 64, 140), RGB(26, 88, 184),
                                     RGB(36, 114, 228), RGB(110, 176, 255) };
    Splash *s = &g_spl;
    HDC m = s->mem;
    const int w = s->w, h = s->h;
    const int p = spl_progress(el);
    const COLORREF WHITE = RGB(238, 247, 255), HALO = RGB(0, 70, 170);
    int i, col;
    BLENDFUNCTION bf;
    int pw = S(300), ph = S(152), px = (w - pw) / 2, py = S(80);

    bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0; bf.AlphaFormat = 0; bf.SourceConstantAlpha = 255;

    BitBlt(m, 0, 0, w, h, s->bg, 0, 0, SRCCOPY);
    SetBkMode(m, TRANSPARENT);

    /* 1) "codigo" que sube por los dos lados */
    SelectObject(m, s->fCode);
    {
        int lh = S(14), total = SPL_LINES * lh;
        for (col = 0; col < 2; col++) {
            int off = (int)((el * (ULONGLONG)S(col ? 38 : 24) / 1000) % (ULONGLONG)total);
            int x = col ? w / 2 + S(10) : S(10);
            for (i = 0; i <= h / lh + 1; i++) {
                int idx = (off / lh + i) % SPL_LINES;
                const WCHAR *t = s->lines[col][idx];
                SetTextColor(m, t[0] == L'[' ? RGB(78, 168, 255) : pal[(idx * 7 + col * 3) % 5]);
                TextOutW(m, x, i * lh - off % lh, t, lstrlenW(t));
            }
        }
    }

    /* 2) emblema del logo (el negro cuenta como transparente) */
    if (s->emblem) {
        int dh = S(62), dw = dh * s->ew / s->eh;
        int ex = (w - dw) / 2, ey = S(8);
        bf.SourceConstantAlpha = 175;
        AlphaBlend(m, ex - S(16), ey - S(4), dw + S(32), dh + S(8), s->blk, 0, 0, dw + S(32), dh + S(8), bf);
        SetStretchBltMode(m, HALFTONE);
        SetBrushOrgEx(m, 0, 0, NULL);
        StretchBlt(m, ex, ey, dw, dh, s->edc, 0, 0, s->ew, s->eh, SRCPAINT);
    }

    /* 3) panel central translucido con borde azul y esquinas blancas */
    {
        int c = S(16), t = S(2) > 1 ? S(2) : 1;
        const COLORREF wc = RGB(230, 244, 255);
        bf.SourceConstantAlpha = 205;
        AlphaBlend(m, px, py, pw, ph, s->blk, 0, 0, pw, ph, bf);
        spl_frame(m, px - S(3), py - S(3), pw + S(6), ph + S(6), RGB(0, 24, 62));
        spl_frame(m, px - S(2), py - S(2), pw + S(4), ph + S(4), RGB(0, 46, 116));
        spl_frame(m, px - S(1), py - S(1), pw + S(2), ph + S(2), RGB(0, 84, 200));
        spl_frame(m, px, py, pw, ph, RGB(0, 120, 255));
        spl_fill(m, px - t, py - t, c, t, wc);               spl_fill(m, px - t, py - t, t, c, wc);
        spl_fill(m, px + pw - c + t, py - t, c, t, wc);      spl_fill(m, px + pw, py - t, t, c, wc);
        spl_fill(m, px - t, py + ph, c, t, wc);              spl_fill(m, px - t, py + ph - c + t, t, c, wc);
        spl_fill(m, px + pw - c + t, py + ph, c, t, wc);     spl_fill(m, px + pw, py + ph - c + t, t, c, wc);
    }

    /* titulo "Loading.." con puntos que van apareciendo */
    {
        SIZE a, b;
        int nd = (int)((el / 330) % 4), tx;
        SelectObject(m, s->fTitle);
        GetTextExtentPoint32W(m, L"Loading...", 10, &a);
        GetTextExtentPoint32W(m, L"Loading", 7, &b);
        tx = px + (pw - a.cx) / 2;
        spl_text(m, tx, py + S(14), L"Loading", 7, WHITE, HALO);
        if (nd) spl_text(m, tx + b.cx, py + S(14), L"...", nd, WHITE, HALO);
    }

    /* barra de progreso con bloques (azul -> celeste, el ultimo bloque brilla) */
    {
        int bx = px + S(24), by = py + S(62), bw = pw - S(48), bh = S(28);
        int N = 20, gap = S(3), pad = S(6), inner = bw - 2 * pad, k;
        int sw = (inner - gap * (N - 1)) / N, used = N * sw + gap * (N - 1);
        int sx = bx + pad + (inner - used) / 2, filled = p * N / 1000, frac = (p * N) % 1000;
        HPEN pen = CreatePen(PS_SOLID, S(2) > 1 ? S(2) : 1, RGB(226, 238, 255));
        HGDIOBJ op = SelectObject(m, pen), ob = SelectObject(m, GetStockObject(NULL_BRUSH));
        RoundRect(m, bx, by, bx + bw, by + bh, S(8), S(8));
        SelectObject(m, ob);
        SelectObject(m, op);
        DeleteObject(pen);
        for (k = 0; k < N; k++) {
            COLORREF c;
            if (k < filled || p >= 1000) c = RGB(k * 6, 96 + k * 6, 255);
            else if (k == filled)        { int v = 40 + frac * 170 / 1000; c = RGB(0, v / 2, v + 30); }
            else                         c = RGB(8, 22, 50);
            if (k == filled - 1 && p < 1000) c = RGB(200, 232, 255);        /* "cabeza" de la barra */
            spl_fill(m, sx + k * (sw + gap), by + pad, sw, bh - 2 * pad, c);
        }
    }

    /* porcentaje */
    {
        WCHAR t[12];
        SIZE z;
        wsprintfW(t, L"%d", p / 10);
        lstrcatW(t, L"%");
        SelectObject(m, s->fPct);
        GetTextExtentPoint32W(m, t, lstrlenW(t), &z);
        spl_text(m, px + (pw - z.cx) / 2, py + S(106), t, lstrlenW(t), WHITE, HALO);
    }

    /* 4) fallo de imagen ocasional: franjas que se corren de lado */
    if ((LONG)el >= s->next_glitch) {
        s->glitch_until = (LONG)el + 90 + (LONG)spl_rnd(90);
        s->next_glitch  = (LONG)el + 1000 + (LONG)spl_rnd(1400);
    }
    if ((LONG)el < s->glitch_until) {
        for (i = 0; i < 2; i++) {
            int sh = S(4) + (int)spl_rnd((unsigned)S(12));
            int sy = (int)spl_rnd((unsigned)(h - sh));
            int dx = S(6) + (int)spl_rnd((unsigned)S(16));
            if (spl_rnd(2)) dx = -dx;
            BitBlt(s->tmp, 0, 0, w, sh, m, 0, sy, SRCCOPY);
            BitBlt(m, dx, sy, w, sh, s->tmp, 0, 0, SRCCOPY);
        }
    }

    /* 5) efecto pantalla vieja: lineas finas y bordes oscuros */
    bf.SourceConstantAlpha = 255; bf.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(m, 0, 0, w, h, s->ovl, 0, 0, w, h, bf);
    spl_frame(m, 0, 0, w, h, RGB(0, 70, 170));

    /* 6) estado abajo a la izquierda y credito abajo a la derecha (nitidos, sobre la capa) */
    {
        const WCHAR *st = p < 150 ? L"starting JJEXE..."
                        : p < 350 ? L"loading settings..."
                        : p < 600 ? L"preparing macro engine..."
                        : p < 850 ? L"calibrating timers..."
                        : p < 1000 ? L"almost ready..." : L"ready.";
        const WCHAR *cr = L"Developers: Claudio and la perla negra";
        WCHAR t[64];
        SIZE z;
        bf.SourceConstantAlpha = 205; bf.AlphaFormat = 0;                    /* franja oscura para que se lea bien */
        AlphaBlend(m, 1, h - S(34), w - 2, S(33), s->blk, 0, 0, w - 2, S(33), bf);
        SelectObject(m, s->fSmall);
        wsprintfW(t, L"> %s%s", st, ((el / 400) & 1) ? L"_" : L" ");
        SetTextColor(m, RGB(70, 160, 255));
        TextOutW(m, S(14), h - S(28), t, lstrlenW(t));
        GetTextExtentPoint32W(m, cr, lstrlenW(cr), &z);
        SetTextColor(m, RGB(130, 160, 200));
        TextOutW(m, w - S(14) - z.cx, h - S(28), cr, lstrlenW(cr));
    }
}

static LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Splash *s = &g_spl;
    switch (msg) {
    case WM_TIMER: {
        LONGLONG el = (LONGLONG)(GetTickCount64() - s->t0);
        LONGLONG fo = s->skip_at >= 0 ? s->skip_at : (LONGLONG)(SPL_RUN + SPL_HOLD);   /* cuando empieza a desvanecerse */
        int a = 255;
        if (el < SPL_IN) a = (int)(el * 255 / SPL_IN);
        if (el > fo) {
            int b = 255 - (int)((el - fo) * 255 / SPL_OUT);
            if (b < a) a = b;
        }
        if (a < 0) a = 0;
        if (a != s->last_alpha) {
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)a, LWA_ALPHA);
            s->last_alpha = a;
        }
        if (el >= fo + SPL_OUT) { DestroyWindow(hwnd); return 0; }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        spl_draw(GetTickCount64() - s->t0);
        BitBlt(dc, 0, 0, s->w, s->h, s->mem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_KEYDOWN: case WM_CLOSE:
        if (s->skip_at < 0) {
            LONGLONG el = (LONGLONG)(GetTickCount64() - s->t0);
            if (el < SPL_RUN + SPL_HOLD) s->skip_at = el;          /* omitir: empieza a desvanecerse ya */
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void run_splash_builtin(HINSTANCE hInst)
{
    Splash *s = &g_spl;
    WNDCLASSEXW wc;
    BITMAPINFO bi;
    DWORD *bits = NULL;
    RECT wa;
    HDC scr;
    HWND hs;
    MSG msg;
    int x, y, i, col;

    ZeroMemory(s, sizeof *s);
    s->skip_at = -1;
    s->next_glitch = 800;
    s->last_alpha = -1;
    s->rng = GetTickCount() | 1u;
    s->w = S(SPL_W);
    s->h = S(SPL_H);

    scr = GetDC(NULL);
    s->mem = spl_dc(scr, s->w, s->h, &s->mem_b);
    s->bg  = spl_dc(scr, s->w, s->h, &s->bg_b);
    s->blk = spl_dc(scr, s->w, s->h, &s->blk_b);
    s->tmp = spl_dc(scr, s->w, S(20), &s->tmp_b);
    s->edc = CreateCompatibleDC(scr);

    ZeroMemory(&bi, sizeof bi);                                    /* capa de lineas finas: DIB de 32 bits con transparencia */
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = s->w;
    bi.bmiHeader.biHeight = -s->h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s->ovl_b = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    s->ovl = CreateCompatibleDC(scr);
    ReleaseDC(NULL, scr);

    if (!s->mem_b || !s->bg_b || !s->blk_b || !s->tmp_b || !s->ovl_b || !bits || !s->ovl || !s->edc)
        goto cleanup;                                              /* si algo falla, simplemente no hay animacion */
    SelectObject(s->ovl, s->ovl_b);

    /* fondo: negro que se vuelve azul noche hacia abajo; negro para los translucidos */
    for (y = 0; y < s->h; y++)
        spl_fill(s->bg, 0, y, s->w, 1, RGB(0, 3 + 11 * y / s->h, 8 + 26 * y / s->h));
    spl_fill(s->blk, 0, 0, s->w, s->h, RGB(0, 0, 0));

    /* capa "pantalla vieja": lineas finas + bordes oscuros (negro premultiplicado) */
    {
        int step = S(3) > 2 ? S(3) : 2, tf = S(40), bfd = S(56), sf = S(28);
        for (y = 0; y < s->h; y++) {
            for (x = 0; x < s->w; x++) {
                int a = (y % step == 0) ? 46 : 0, d = x < s->w - 1 - x ? x : s->w - 1 - x;
                if (y < tf) a += 210 * (tf - y) / tf;
                if (y > s->h - bfd) a += 190 * (y - (s->h - bfd)) / bfd;
                if (d < sf) a += 110 * (sf - d) / sf;
                if (a > 255) a = 255;
                bits[y * s->w + x] = (DWORD)a << 24;
            }
        }
    }

    /* emblema del logo (recurso 201) */
    s->emblem = (HBITMAP)LoadImageW(hInst, MAKEINTRESOURCEW(201), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (s->emblem) {
        BITMAP bm;
        if (GetObjectW(s->emblem, sizeof bm, &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            s->ew = bm.bmWidth;
            s->eh = bm.bmHeight;
            SelectObject(s->edc, s->emblem);
        } else {
            DeleteObject(s->emblem);
            s->emblem = NULL;
        }
    }

    s->fCode  = spl_font(11, FW_NORMAL);
    s->fTitle = spl_font(24, FW_BOLD);
    s->fPct   = spl_font(20, FW_BOLD);
    s->fSmall = spl_font(12, FW_NORMAL);

    /* lineas de "codigo": mensajes de la propia macro mezclados con volcados hexadecimales */
    for (col = 0; col < 2; col++) {
        for (i = 0; i < SPL_LINES; i++) {
            if (i % 3 == 0) {
                lstrcpynW(s->lines[col][i], k_msgs[(i / 3 + col * 5) % (int)(sizeof k_msgs / sizeof k_msgs[0])], 44);
            } else {
                unsigned a = (spl_rnd(65536) << 16) | spl_rnd(65536);
                wsprintfW(s->lines[col][i], L"%08X  %02X %02X %02X %02X %02X %02X %02X", a,
                          spl_rnd(256), spl_rnd(256), spl_rnd(256), spl_rnd(256),
                          spl_rnd(256), spl_rnd(256), spl_rnd(256));
            }
        }
    }

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"JJEXESplash";
    RegisterClassExW(&wc);

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    hs = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"JJEXE", WS_POPUP,
                         wa.left + (wa.right - wa.left - s->w) / 2, wa.top + (wa.bottom - wa.top - s->h) / 2,
                         s->w, s->h, NULL, NULL, hInst, NULL);
    if (hs) {
        SetLayeredWindowAttributes(hs, 0, 0, LWA_ALPHA);            /* empieza invisible y aparece poco a poco */
        s->last_alpha = 0;
        s->t0 = GetTickCount64();
        ShowWindow(hs, SW_SHOW);
        SetForegroundWindow(hs);
        SetTimer(hs, 1, 16, NULL);
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

cleanup:
    if (s->fCode)  DeleteObject(s->fCode);
    if (s->fTitle) DeleteObject(s->fTitle);
    if (s->fPct)   DeleteObject(s->fPct);
    if (s->fSmall) DeleteObject(s->fSmall);
    if (s->mem) DeleteDC(s->mem);
    if (s->bg)  DeleteDC(s->bg);
    if (s->ovl) DeleteDC(s->ovl);
    if (s->blk) DeleteDC(s->blk);
    if (s->tmp) DeleteDC(s->tmp);
    if (s->edc) DeleteDC(s->edc);
    if (s->mem_b)  DeleteObject(s->mem_b);
    if (s->bg_b)   DeleteObject(s->bg_b);
    if (s->ovl_b)  DeleteObject(s->ovl_b);
    if (s->blk_b)  DeleteObject(s->blk_b);
    if (s->tmp_b)  DeleteObject(s->tmp_b);
    if (s->emblem) DeleteObject(s->emblem);
    UnregisterClassW(L"JJEXESplash", hInst);
}


/* ===================================================================== */
/*  Animaciones propias: GIF animado, PNG, JPG o BMP que cada quien       */
/*  pone en la carpeta  %APPDATA%\JJEXE\Animaciones                       */
/* ===================================================================== */
typedef struct { UINT id; ULONG length; WORD type; void *value; } PItem;     /* = PropertyItem de GDI+ */

#define CA_MAXF 160
typedef struct {
    int       n, w, h, total_ms, dur_ms, last_alpha;
    HBITMAP   f[CA_MAXF];
    int       end_ms[CA_MAXF];           /* instante en que termina cada cuadro dentro de una vuelta */
    ULONGLONG t0;
    LONGLONG  skip_at;
    HDC       mem;
} CAnim;

static CAnim g_ca;

void anim_path(const WCHAR *name, WCHAR *out, int n)
{
    (void)n;
    wsprintfW(out, L"%s\\Animaciones\\%s", ini_dir(), name);
}

static int has_ext(const WCHAR *name)
{
    static const WCHAR *ex[5] = { L".gif", L".png", L".jpg", L".jpeg", L".bmp" };
    const WCHAR *d = name + lstrlenW(name);
    int i;
    while (d > name && *d != L'.') d--;
    if (*d != L'.') return 0;
    for (i = 0; i < 5; i++) if (!lstrcmpiW(d, ex[i])) return 1;
    return 0;
}

int anim_scan(WCHAR names[][128], int max)
{
    WCHAR pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int n = 0, i, j;
    wsprintfW(pat, L"%s\\Animaciones\\*", ini_dir());
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && has_ext(fd.cFileName) &&
            lstrlenW(fd.cFileName) < 128 && n < max)
            lstrcpyW(names[n++], fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (i = 1; i < n; i++) {                                    /* orden alfabetico */
        WCHAR t[128];
        lstrcpyW(t, names[i]);
        for (j = i - 1; j >= 0 && lstrcmpiW(names[j], t) > 0; j--) lstrcpyW(names[j + 1], names[j]);
        lstrcpyW(names[j + 1], t);
    }
    return n;
}

static void ca_free(void)
{
    int i;
    for (i = 0; i < g_ca.n; i++) if (g_ca.f[i]) DeleteObject(g_ca.f[i]);
    g_ca.n = 0;
}

/* Carga todos los cuadros ya reducidos al tamano en que se van a mostrar. */
static int ca_load(const WCHAR *path)
{
    static const GUID dimTime = { 0x6aedbd6d, 0x3fb5, 0x418a, { 0x83, 0xa6, 0x7f, 0x45, 0x22, 0x9d, 0xc8, 0x72 } };
    CAnim *a = &g_ca;
    void *img = NULL, *thumb;
    UINT W = 0, H = 0, count = 1, i, sz = 0;
    int tw, th, maxw = S(640), maxh = S(360), maxframes, total = 0, loops;
    BYTE *pb = NULL;
    LONG *dl = NULL;
    UINT ndl = 0;

    if (!gp_ok()) return 0;
    if (GdipCreateBitmapFromFile(path, &img) != 0 || !img) return 0;
    GdipGetImageWidth(img, &W);
    GdipGetImageHeight(img, &H);
    if (!W || !H) { GdipDisposeImage(img); return 0; }
    if (GdipImageGetFrameCount(img, &dimTime, &count) != 0 || count < 1) count = 1;

    tw = (int)W; th = (int)H;                                    /* cabe en 640x360, sin agrandar demasiado */
    if (tw > maxw) { th = th * maxw / tw; tw = maxw; }
    if (th > maxh) { tw = tw * maxh / th; th = maxh; }
    if (tw < S(240) && th < S(135)) {                            /* imagenes chicas: se amplian un poco */
        int k = 4;
        while (k > 1 && (tw * k > S(240) * 2 || th * k > S(135) * 2)) k--;
        tw *= k; th *= k;
    }
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    maxframes = (96 * 1024 * 1024) / (tw * th * 4);              /* tope de memoria ~96 MB */
    if (maxframes < 1) maxframes = 1;
    if (count > CA_MAXF) count = CA_MAXF;
    if ((int)count > maxframes) count = (UINT)maxframes;

    if (count > 1 && GdipGetPropertyItemSize(img, 0x5100, &sz) == 0 && sz > sizeof(PItem)) {
        pb = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
        if (pb && GdipGetPropertyItem(img, 0x5100, sz, pb) == 0) {
            dl = (LONG *)((PItem *)pb)->value;
            ndl = ((PItem *)pb)->length / 4;
        }
    }

    for (i = 0; i < count; i++) {
        HBITMAP hb = NULL;
        int ms = 100;
        thumb = NULL;
        if (count > 1) GdipImageSelectActiveFrame(img, &dimTime, i);
        if (GdipGetImageThumbnail(img, (UINT)tw, (UINT)th, &thumb, NULL, NULL) != 0 || !thumb) break;
        if (GdipCreateHBITMAPFromBitmap(thumb, &hb, 0xFF000000) != 0 || !hb) { GdipDisposeImage(thumb); break; }
        GdipDisposeImage(thumb);
        if (dl && i < ndl) { ms = (int)dl[i] * 10; if (ms < 20) ms = 100; }     /* hundredths of a second */
        total += ms;
        a->f[a->n] = hb;
        a->end_ms[a->n] = total;
        a->n++;
    }
    GdipDisposeImage(img);
    if (pb) HeapFree(GetProcessHeap(), 0, pb);
    if (a->n == 0) return 0;

    a->w = tw; a->h = th;
    a->total_ms = total > 0 ? total : 1;
    if (a->n == 1) a->dur_ms = 2500;                             /* imagen fija: 2,5 s */
    else {                                                       /* animada: unas 2 s como minimo, 6 s como maximo */
        loops = (2000 + a->total_ms - 1) / a->total_ms;
        if (loops < 1) loops = 1;
        a->dur_ms = a->total_ms * loops;
        if (a->dur_ms > 6000) a->dur_ms = 6000;
    }
    return 1;
}

static LRESULT CALLBACK CAProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    CAnim *a = &g_ca;
    switch (msg) {
    case WM_TIMER: {
        LONGLONG el = (LONGLONG)(GetTickCount64() - a->t0);
        LONGLONG fo = a->skip_at >= 0 ? a->skip_at : (LONGLONG)a->dur_ms;
        int al = 255;
        if (el < SPL_IN) al = (int)(el * 255 / SPL_IN);
        if (el > fo) {
            int b = 255 - (int)((el - fo) * 255 / SPL_OUT);
            if (b < al) al = b;
        }
        if (al < 0) al = 0;
        if (al != a->last_alpha) {
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)al, LWA_ALPHA);
            a->last_alpha = al;
        }
        if (el >= fo + SPL_OUT) { DestroyWindow(hwnd); return 0; }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int pos = (int)((GetTickCount64() - a->t0) % (ULONGLONG)a->total_ms), i;
        for (i = 0; i < a->n - 1 && pos >= a->end_ms[i]; i++) {}
        SelectObject(a->mem, a->f[i]);
        BitBlt(dc, 0, 0, a->w, a->h, a->mem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_KEYDOWN: case WM_CLOSE:
        if (a->skip_at < 0) {
            LONGLONG el = (LONGLONG)(GetTickCount64() - a->t0);
            if (el < a->dur_ms) a->skip_at = el;
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* Devuelve 1 si la animacion se pudo mostrar. */
static int run_splash_custom(HINSTANCE hInst, const WCHAR *path)
{
    CAnim *a = &g_ca;
    WNDCLASSEXW wc;
    RECT wa;
    HWND hs;
    HDC scr;
    MSG msg;

    ZeroMemory(a, sizeof *a);
    a->skip_at = -1;
    a->last_alpha = -1;
    if (!ca_load(path)) { ca_free(); return 0; }

    scr = GetDC(NULL);
    a->mem = CreateCompatibleDC(scr);
    ReleaseDC(NULL, scr);

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = CAProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"JJEXEAnim";
    RegisterClassExW(&wc);

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    hs = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"JJEXE", WS_POPUP,
                         wa.left + (wa.right - wa.left - a->w) / 2, wa.top + (wa.bottom - wa.top - a->h) / 2,
                         a->w, a->h, NULL, NULL, hInst, NULL);
    if (hs) {
        SetLayeredWindowAttributes(hs, 0, 0, LWA_ALPHA);
        a->last_alpha = 0;
        a->t0 = GetTickCount64();
        ShowWindow(hs, SW_SHOW);
        SetForegroundWindow(hs);
        SetTimer(hs, 1, 16, NULL);
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    DeleteDC(a->mem);
    ca_free();
    UnregisterClassW(L"JJEXEAnim", hInst);
    return 1;
}

/* ===================================================================== */
/*  Animaciones integradas extra: Claude, Roblox, Minecraft y Synthwave.  */
/*  Son estilos propios inspirados en cada tema (no usan logos oficiales). */
/*  Clic o cualquier tecla las omite, igual que la predeterminada.        */
/* ===================================================================== */
typedef void (*A2Draw)(HDC m, void *g, int w, int h, ULONGLONG el, int p);

typedef struct {
    A2Draw    draw;
    int       w, h, last_alpha;
    ULONGLONG t0;
    LONGLONG  skip_at;
    HDC       mem;
    HBITMAP   mem_b;
    HFONT     fSmall, fMid, fBig;
} Anim2;

static Anim2 g_a2;

static COLORREF a2_mix(COLORREF a, COLORREF b, int t)            /* t: 0..256 */
{
    return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 256,
               GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 256,
               GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 256);
}

static unsigned a2_hash(unsigned x, unsigned y)
{
    unsigned h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void a2_bands(HDC m, int w, int y0, int y1, COLORREF c0, COLORREF c1)   /* degradado vertical en bandas */
{
    int n = 28, i;
    for (i = 0; i < n; i++) {
        int ya = y0 + (y1 - y0) * i / n, yb = y0 + (y1 - y0) * (i + 1) / n;
        spl_fill(m, 0, ya, w, yb - ya + 1, a2_mix(c0, c1, i * 256 / (n - 1)));
    }
}

static void a2_text(HDC m, HFONT f, const WCHAR *t, int x, int y, COLORREF c, COLORREF sh, int d)
{
    SelectObject(m, f);
    SetBkMode(m, TRANSPARENT);
    if (d) {
        SetTextColor(m, sh);
        TextOutW(m, x + d, y + d, t, lstrlenW(t));
    }
    SetTextColor(m, c);
    TextOutW(m, x, y, t, lstrlenW(t));
}

static void a2_text_c(HDC m, HFONT f, const WCHAR *t, int cx, int y, COLORREF c, COLORREF sh, int d)
{
    SIZE z;
    SelectObject(m, f);
    GetTextExtentPoint32W(m, t, lstrlenW(t), &z);
    a2_text(m, f, t, cx - z.cx / 2, y, c, sh, d);
}

static void a2_bar(HDC m, void *g, int x, int y, int w, int h, int p, COLORREF track, COLORREF c1, COLORREF c2)
{
    gp_fill_rr(g, (float)x, (float)y, (float)w, (float)h, (float)h / 2, track, 255);
    if (p > 0) gp_fill_rr_grad(g, (float)x, (float)y, (float)(w * p / 1000 < h ? h : w * p / 1000), (float)h, (float)h / 2, c1, c2, 255, 0);
    (void)m;
}

/* franja de abajo: estado a la izquierda y credito a la derecha */
static void a2_footer(HDC m, int w, int h, ULONGLONG el, int p, COLORREF strip, COLORREF tc, COLORREF cc)
{
    const WCHAR *st = p < 150 ? L"starting JJEXE..."
                    : p < 350 ? L"loading settings..."
                    : p < 600 ? L"preparing macro engine..."
                    : p < 850 ? L"calibrating timers..."
                    : p < 1000 ? L"almost ready..." : L"ready.";
    const WCHAR *cr = L"Developers: Claudio and la perla negra";
    WCHAR t[64];
    SIZE z;
    spl_fill(m, 0, h - S(34), w, S(34), strip);
    SelectObject(m, g_a2.fSmall);
    SetBkMode(m, TRANSPARENT);
    wsprintfW(t, L"> %s%s", st, ((el / 400) & 1) ? L"_" : L" ");
    SetTextColor(m, tc);
    TextOutW(m, S(14), h - S(28), t, lstrlenW(t));
    GetTextExtentPoint32W(m, cr, lstrlenW(cr), &z);
    SetTextColor(m, cc);
    TextOutW(m, w - S(14) - z.cx, h - S(28), cr, lstrlenW(cr));
}

/* ---------------------------------------------------------------- Claude: chispa naranja y terminal */
static void draw_claude(HDC m, void *g, int w, int h, ULONGLONG el, int p)
{
    const COLORREF OR = RGB(217, 119, 87), OR2 = RGB(244, 170, 130), CREAM = RGB(238, 232, 222);
    static const WCHAR *ln[4] = { L"> claude start JJEXE", L"* reading your settings...", L"* macro engine ........ ready", L"* all set. have fun!" };
    int i, cx = w / 2, cy = S(76), tx = S(70), ty = S(122), tw = w - S(140), th = S(104), total = 0, shown, k;
    double t = (double)el / 1000.0, sc = el < 500 ? (double)el / 500.0 : 1.0;

    a2_bands(m, w, 0, h, RGB(31, 29, 26), RGB(18, 17, 15));

    for (i = 0; i < 12; i++) {                                   /* chispa: doce rayos que giran y laten */
        double a = t * 0.9 + i * 3.14159265358979 / 6.0, pulse = 0.5 + 0.5 * sin(t * 3.2 + i * 0.9);
        double r0 = S(9) * sc, r1 = (S(24) + S(17) * pulse) * sc;
        gp_line(g, (float)(cx + cos(a) * r0), (float)(cy + sin(a) * r0), (float)(cx + cos(a) * r1), (float)(cy + sin(a) * r1),
                (i & 1) ? OR2 : OR, 255, (float)S(4));
    }
    gp_fill_ellipse(g, (float)(cx - S(6)), (float)(cy - S(6)), (float)S(12), (float)S(12), OR, 255);

    gp_fill_rr(g, (float)tx, (float)ty, (float)tw, (float)th, (float)S(10), RGB(13, 12, 11), 255);
    gp_stroke_rr(g, (float)tx, (float)ty, (float)tw, (float)th, (float)S(10), RGB(78, 70, 62), 255, 1.0f);
    gp_fill_ellipse(g, (float)(tx + S(12)), (float)(ty + S(9)), (float)S(8), (float)S(8), RGB(255, 95, 86), 255);
    gp_fill_ellipse(g, (float)(tx + S(26)), (float)(ty + S(9)), (float)S(8), (float)S(8), RGB(255, 189, 46), 255);
    gp_fill_ellipse(g, (float)(tx + S(40)), (float)(ty + S(9)), (float)S(8), (float)S(8), RGB(39, 201, 63), 255);

    for (k = 0; k < 4; k++) total += lstrlenW(ln[k]);           /* texto que se escribe solo */
    shown = (int)(el * (ULONGLONG)total / 1900u);
    if (shown > total) shown = total;
    for (k = 0; k < 4; k++) {
        int len = lstrlenW(ln[k]), n = shown < len ? shown : len;
        if (n > 0) {
            SelectObject(m, g_a2.fSmall);
            SetBkMode(m, TRANSPARENT);
            SetTextColor(m, k == 0 ? CREAM : (k == 3 ? OR2 : RGB(190, 150, 128)));
            TextOutW(m, tx + S(14), ty + S(30) + k * S(17), ln[k], n);
            if (shown < total && shown <= len && ((el / 350) & 1)) {      /* cursor */
                SIZE z;
                GetTextExtentPoint32W(m, ln[k], n, &z);
                spl_fill(m, tx + S(14) + z.cx + S(2), ty + S(31) + k * S(17), S(7), S(13), OR);
            }
        }
        shown -= len;
        if (shown < 0) shown = 0;
    }
    a2_bar(m, g, tx, ty + th + S(14), tw, S(5), p, RGB(52, 47, 42), OR, OR2);
    a2_footer(m, w, h, el, p, RGB(12, 11, 10), OR, RGB(150, 138, 126));
}

/* ---------------------------------------------------------------- Roblox: bloques que caen y arman "JJ" */
static void draw_roblox(HDC m, void *g, int w, int h, ULONGLONG el, int p)
{
    static const char *J[7] = { "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." };
    static const COLORREF pal[5] = { RGB(231, 76, 60), RGB(52, 152, 219), RGB(241, 196, 15), RGB(46, 204, 113), RGB(236, 240, 241) };
    const int cs = S(22), gx = (w - 11 * cs) / 2, gy = S(26);
    int r, c, idx = 0, i;

    a2_bands(m, w, 0, h, RGB(22, 26, 38), RGB(36, 42, 62));
    for (i = 1; i < 9; i++)                                       /* cuadricula tenue de fondo */
        spl_fill(m, 0, h * i / 9, w, 1, RGB(40, 47, 68));
    spl_fill(m, gx - S(14), gy + 7 * cs + S(2), 11 * cs + S(28), S(10), RGB(66, 156, 82));   /* base verde */
    spl_fill(m, gx - S(14), gy + 7 * cs + S(12), 11 * cs + S(28), S(4), RGB(40, 104, 54));

    for (r = 0; r < 7; r++) {
        for (c = 0; c < 11; c++) {
            int jc = c < 5 ? c : (c > 5 ? c - 6 : -1);
            if (jc < 0 || J[r][jc] != '#') continue;
            {
                LONGLONG t = (LONGLONG)el - 250 - idx * 70;
                int ty = gy + r * cs, x = gx + c * cs, y;
                COLORREF col = pal[(r + c) % 5];
                idx++;
                if (t < 0) continue;
                if (t < 380) { double u = (double)t / 380.0; y = (int)(ty - S(220) * (1.0 - u * u)); }
                else if (t < 560) { double u = (double)(t - 380) / 180.0; y = ty - (int)(sin(3.14159265358979 * u) * S(9)); }
                else y = ty;
                spl_fill(m, x, y, cs - S(1), cs - S(1), col);
                spl_fill(m, x, y, cs - S(1), S(4), a2_mix(col, RGB(255, 255, 255), 90));      /* cara de arriba */
                spl_fill(m, x, y + cs - S(5), cs - S(1), S(4), a2_mix(col, RGB(0, 0, 0), 90)); /* sombra */
                gp_fill_ellipse(g, (float)(x + cs / 2 - S(4)), (float)(y + S(6)), (float)S(8), (float)S(8), a2_mix(col, RGB(255, 255, 255), 60), 255);
            }
        }
    }
    a2_bar(m, g, (w - S(300)) / 2, S(224), S(300), S(8), p, RGB(18, 21, 30), RGB(52, 152, 219), RGB(241, 196, 15));
    a2_footer(m, w, h, el, p, RGB(14, 16, 24), RGB(46, 204, 113), RGB(130, 140, 165));
}

/* ---------------------------------------------------------------- Minecraft: pixeles, barra de experiencia y hotbar */
static void draw_minecraft(HDC m, void *g, int w, int h, ULONGLONG el, int p)
{
    static const COLORREF item[9] = { RGB(77, 237, 229), RGB(252, 238, 75), RGB(216, 216, 216), RGB(190, 30, 20), RGB(23, 221, 98),
                                      RGB(31, 70, 200), RGB(60, 60, 60), RGB(160, 110, 60), RGB(210, 70, 50) };
    const int T = S(16), gy = S(198), sl = S(30), gap = S(2);
    const int hx = (w - (9 * sl + 8 * gap)) / 2, hy = S(150);
    int i, x, y, bx, bw, by;
    WCHAR lv[8];

    a2_bands(m, w, 0, gy, RGB(10, 12, 36), RGB(70, 84, 140));
    for (i = 0; i < 26; i++) {                                    /* estrellas cuadradas */
        unsigned hv = a2_hash((unsigned)i, 7u);
        if (((el / 300) + (ULONGLONG)i) % 4u == 0) continue;
        spl_fill(m, (int)(hv % (unsigned)w), (int)((hv >> 9) % (unsigned)(gy - S(40))), S(2), S(2), RGB(235, 240, 255));
    }
    for (y = 0; gy + y * T < h; y++) {                            /* suelo: cesped y tierra con ruido */
        for (x = 0; x * T < w; x++) {
            unsigned hv = a2_hash((unsigned)x, (unsigned)y), v = hv % 5u;
            COLORREF c = y == 0 ? RGB(88 + v * 6, 160 + v * 8, 50 + v * 3) : RGB(112 + v * 7, 78 + v * 5, 50 + v * 3);
            spl_fill(m, x * T, gy + y * T, T, T, c);
            spl_fill(m, x * T, gy + y * T, T, 1, a2_mix(c, RGB(0, 0, 0), 60));
            spl_fill(m, x * T, gy + y * T, 1, T, a2_mix(c, RGB(0, 0, 0), 60));
        }
    }
    a2_text_c(m, g_a2.fBig, L"JJEXE", w / 2, S(26), RGB(240, 240, 240), RGB(50, 50, 50), S(3));

    wsprintfW(lv, L"%d", p / 10);                                 /* nivel de experiencia que sube con la carga */
    a2_text_c(m, g_a2.fMid, lv, w / 2, S(86), RGB(128, 255, 32), RGB(0, 0, 0), S(2));
    bw = S(300); bx = (w - bw) / 2; by = S(116);
    spl_fill(m, bx - 1, by - 1, bw + 2, S(10) + 2, RGB(0, 0, 0));
    spl_fill(m, bx, by, bw, S(10), RGB(60, 60, 60));
    spl_fill(m, bx, by, bw * p / 1000, S(10), RGB(110, 230, 25));
    spl_fill(m, bx, by, bw * p / 1000, S(3), RGB(190, 255, 120));

    for (i = 0; i < 9; i++) {                                     /* hotbar */
        int sx = hx + i * (sl + gap);
        spl_fill(m, sx, hy, sl, sl, RGB(55, 55, 55));
        spl_fill(m, sx + 2, hy + 2, sl - 4, sl - 4, RGB(139, 139, 139));
        spl_fill(m, sx + 2, hy + 2, sl - 4, 2, RGB(200, 200, 200));
        spl_fill(m, sx + 2, hy + sl - 4, sl - 4, 2, RGB(90, 90, 90));
        if (p > i * 105) {
            int q = S(16), ox = sx + (sl - q) / 2, oy = hy + (sl - q) / 2;
            spl_fill(m, ox, oy, q, q, a2_mix(item[i], RGB(0, 0, 0), 70));
            spl_fill(m, ox, oy, q - S(2), q - S(2), item[i]);
            spl_fill(m, ox + S(2), oy + S(2), S(4), S(4), a2_mix(item[i], RGB(255, 255, 255), 110));
        }
    }
    {   int s = (int)((el / 420) % 9u), sx = hx + s * (sl + gap);     /* marco blanco de la casilla elegida */
        spl_frame(m, sx - 2, hy - 2, sl + 4, sl + 4, RGB(255, 255, 255));
        spl_frame(m, sx - 1, hy - 1, sl + 2, sl + 2, RGB(255, 255, 255)); }
    a2_footer(m, w, h, el, p, RGB(24, 18, 12), RGB(128, 255, 32), RGB(190, 180, 160));
    (void)g;
}

/* ---------------------------------------------------------------- Synthwave: sol dorado y cuadricula azul */
static COLORREF sw_sky(int y, int hz) { return a2_mix(RGB(8, 10, 36), RGB(30, 70, 150), y * 256 / (hz > 0 ? hz : 1)); }

static void draw_synth(HDC m, void *g, int w, int h, ULONGLONG el, int p)
{
    const COLORREF GOLD = RGB(255, 196, 40), BLUE = RGB(36, 114, 228);
    const int hz = S(166), cx = w / 2, fl = h - S(34);
    int i, k;
    double fr = (double)(el % 1100u) / 1100.0;

    a2_bands(m, w, 0, hz, sw_sky(0, hz), sw_sky(hz, hz));
    for (i = 0; i < 18; i++) {
        unsigned hv = a2_hash((unsigned)i, 3u);
        if (((el / 260) + (ULONGLONG)i) % 3u == 0) continue;
        spl_fill(m, (int)(hv % (unsigned)w), (int)((hv >> 8) % (unsigned)(hz - S(50))), S(1) + 1, S(1) + 1, RGB(210, 225, 255));
    }
    gp_fill_ellipse(g, (float)(cx - S(86)), (float)(hz - S(88)), (float)S(172), (float)S(172), GOLD, 26);       /* resplandor y sol */
    gp_fill_ellipse(g, (float)(cx - S(74)), (float)(hz - S(76)), (float)S(148), (float)S(148), GOLD, 44);
    gp_fill_ellipse(g, (float)(cx - S(62)), (float)(hz - S(64)), (float)S(124), (float)S(124), GOLD, 255);
    gp_fill_ellipse(g, (float)(cx - S(46)), (float)(hz - S(58)), (float)S(92), (float)S(60), RGB(255, 236, 140), 70);
    for (k = 0; k < 6; k++) {                                     /* rendijas del sol */
        int y = hz - S(34) + k * S(10);
        spl_fill(m, cx - S(66), y, S(132), S(1 + k / 2) + 1, sw_sky(y, hz));
    }
    spl_fill(m, 0, hz, w, h - hz, RGB(6, 8, 28));                 /* suelo */

    for (i = -9; i <= 9; i++)                                     /* lineas que salen del horizonte */
        gp_line(g, (float)cx, (float)hz, (float)(cx + i * S(64)), (float)fl, BLUE, 190, 1.4f);
    for (k = 0; k < 9; k++) {                                     /* lineas que se acercan */
        double z = (k + fr) / 9.0, y = hz + (fl - hz) * z * z;
        gp_line(g, 0.0f, (float)y, (float)w, (float)y, BLUE, (int)(70 + 150 * z), 1.4f);
    }
    gp_line(g, 0.0f, (float)hz, (float)w, (float)hz, GOLD, 230, 2.0f);
    a2_text_c(m, g_a2.fBig, L"JJEXE", cx, S(22), RGB(255, 226, 150), RGB(0, 70, 170), S(3));
    spl_fill(m, (w - S(300)) / 2 - S(4), fl - S(22), S(308), S(16), RGB(6, 8, 28));
    a2_bar(m, g, (w - S(300)) / 2, fl - S(18), S(300), S(8), p, RGB(24, 40, 90), BLUE, GOLD);
    a2_footer(m, w, h, el, p, RGB(5, 6, 20), GOLD, RGB(130, 160, 210));
}

static LRESULT CALLBACK A2Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Anim2 *a = &g_a2;
    switch (msg) {
    case WM_TIMER: {
        LONGLONG el = (LONGLONG)(GetTickCount64() - a->t0);
        LONGLONG fo = a->skip_at >= 0 ? a->skip_at : (LONGLONG)(SPL_RUN + SPL_HOLD);
        int al = 255;
        if (el < SPL_IN) al = (int)(el * 255 / SPL_IN);
        if (el > fo) {
            int b = 255 - (int)((el - fo) * 255 / SPL_OUT);
            if (b < al) al = b;
        }
        if (al < 0) al = 0;
        if (al != a->last_alpha) { SetLayeredWindowAttributes(hwnd, 0, (BYTE)al, LWA_ALPHA); a->last_alpha = al; }
        if (el >= fo + SPL_OUT) { DestroyWindow(hwnd); return 0; }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0; }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        ULONGLONG el = GetTickCount64() - a->t0;
        void *g = gp_begin(a->mem);
        a->draw(a->mem, g, a->w, a->h, el, spl_progress(el));
        gp_end(g);
        spl_frame(a->mem, 0, 0, a->w, a->h, RGB(80, 80, 90));
        BitBlt(dc, 0, 0, a->w, a->h, a->mem, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0; }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_KEYDOWN: case WM_CLOSE:
        if (a->skip_at < 0) {
            LONGLONG el = (LONGLONG)(GetTickCount64() - a->t0);
            if (el < SPL_RUN + SPL_HOLD) a->skip_at = el;
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void run_theme_anim(HINSTANCE hInst, A2Draw fn)
{
    Anim2 *a = &g_a2;
    WNDCLASSEXW wc;
    RECT wa;
    HDC scr;
    HWND hs;
    MSG msg;

    ZeroMemory(a, sizeof *a);
    a->draw = fn; a->skip_at = -1; a->last_alpha = -1;
    a->w = S(SPL_W); a->h = S(SPL_H);
    scr = GetDC(NULL);
    a->mem = spl_dc(scr, a->w, a->h, &a->mem_b);
    ReleaseDC(NULL, scr);
    if (!a->mem || !a->mem_b) goto done;
    a->fSmall = spl_font(12, FW_NORMAL);
    a->fMid   = spl_font(20, FW_BOLD);
    a->fBig   = spl_font(30, FW_BOLD);

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = A2Proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"JJEXETheme";
    RegisterClassExW(&wc);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    hs = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"JJEXE", WS_POPUP,
                         wa.left + (wa.right - wa.left - a->w) / 2, wa.top + (wa.bottom - wa.top - a->h) / 2,
                         a->w, a->h, NULL, NULL, hInst, NULL);
    if (hs) {
        SetLayeredWindowAttributes(hs, 0, 0, LWA_ALPHA);
        a->t0 = GetTickCount64();
        ShowWindow(hs, SW_SHOW);
        SetForegroundWindow(hs);
        SetTimer(hs, 1, 16, NULL);
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    UnregisterClassW(L"JJEXETheme", hInst);
done:
    if (a->fSmall) DeleteObject(a->fSmall);
    if (a->fMid)   DeleteObject(a->fMid);
    if (a->fBig)   DeleteObject(a->fBig);
    if (a->mem)    DeleteDC(a->mem);
    if (a->mem_b)  DeleteObject(a->mem_b);
}

/* Muestra la animacion elegida en los ajustes: la de JJEXE, una propia, o ninguna. */
void run_splash(HINSTANCE hInst)
{
    if (!lstrcmpW(g_prefs.anim, L"*none")) return;
    if (g_prefs.anim[0] == L'*') {                          /* animaciones integradas */
        A2Draw f = NULL;
        if (!lstrcmpW(g_prefs.anim, L"*claude"))         f = draw_claude;
        else if (!lstrcmpW(g_prefs.anim, L"*roblox"))    f = draw_roblox;
        else if (!lstrcmpW(g_prefs.anim, L"*minecraft")) f = draw_minecraft;
        else if (!lstrcmpW(g_prefs.anim, L"*synth"))     f = draw_synth;
        if (f) { run_theme_anim(hInst, f); return; }
    }
    if (g_prefs.anim[0]) {
        WCHAR p[MAX_PATH];
        anim_path(g_prefs.anim, p, MAX_PATH);
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES && run_splash_custom(hInst, p)) return;
    }
    run_splash_builtin(hInst);
}
