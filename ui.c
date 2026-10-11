/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠​​‌‌‌​​​​​​‌‌‌​​‌‌​​​‌​‌​‌​‌‌​‌​​​​‌‌​‌‌​‌‌‌‌‌‌‌⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 381cc55a1b7f
 */

#include "jj.h"

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_ui[] = "wm:381cc55a1b7f";

/* ===================================================================== */
/*  Temas                                                                 */
/* ===================================================================== */
const Theme g_themes[] = {
 /* nombre        bg                 card               text                 dim                  accent            accent2             border             btn                hot                input              good               bad                 head1         head2 */
 { L"JJEXE Azul",  RGB(10,14,24),  RGB(18,26,42),  RGB(232,240,255), RGB(130,150,185), RGB(0,120,255),  RGB(90,200,255),  RGB(36,52,84),   RGB(26,38,62),  RGB(38,56,92),  RGB(12,18,32),  RGB(40,200,120), RGB(240,80,90),  RGB(0,0,0), RGB(6,16,40) },
 { L"Medianoche",  RGB(14,13,22),  RGB(24,22,38),  RGB(236,232,250), RGB(140,134,170), RGB(150,96,255), RGB(214,150,255), RGB(48,44,76),   RGB(34,31,54),  RGB(50,46,82),  RGB(16,15,28),  RGB(60,210,140), RGB(255,90,110), RGB(0,0,0), RGB(18,10,36) },
 { L"Matrix",      RGB(6,14,10),   RGB(12,26,18),  RGB(210,255,225), RGB(110,160,130), RGB(0,200,100),  RGB(150,255,120), RGB(24,62,42),   RGB(16,38,26),  RGB(24,58,40),  RGB(8,18,12),   RGB(0,220,110),  RGB(255,90,80),  RGB(0,0,0), RGB(0,24,12) },
 { L"Rojo Fuego",  RGB(16,10,10),  RGB(30,18,18),  RGB(255,236,232), RGB(176,140,134), RGB(255,70,60),  RGB(255,170,60),  RGB(70,36,34),   RGB(44,26,26),  RGB(66,38,36),  RGB(20,12,12),  RGB(70,210,120), RGB(255,90,80),  RGB(0,0,0), RGB(40,8,6) },
 { L"Claro",       RGB(240,243,248), RGB(255,255,255), RGB(24,32,48), RGB(100,112,134), RGB(0,112,255),  RGB(0,170,255),   RGB(208,215,228), RGB(232,237,246), RGB(216,226,244), RGB(255,255,255), RGB(20,160,90),  RGB(210,50,60),  RGB(0,0,0), RGB(8,24,60) },
 { L"Océano",      RGB(8,20,24),   RGB(14,34,40),  RGB(226,248,250), RGB(120,160,168), RGB(0,190,200),  RGB(120,240,230), RGB(24,66,74),   RGB(18,46,54),  RGB(26,66,76),  RGB(10,26,30),  RGB(50,220,150), RGB(255,100,100), RGB(0,0,0), RGB(0,30,36) }
};
const int g_ntheme = 6;

/* ===================================================================== */
/*  Controles                                                             */
/* ===================================================================== */
enum { W_NONE, W_LABEL, W_HEAD, W_NOTE, W_SEP, W_CARD, W_BUTTON, W_BIG, W_CHECK, W_SEG, W_SLIDER,
       W_DROP, W_TAB, W_SWATCH, W_LIST, W_FRAME, W_TIMELINE };

typedef struct {
    int   type, id, page;
    int   x, y, w, h;                    /* pixeles logicos (a 96 ppp) */
    WCHAR text[360];
    int   val, min, max, group, enabled, vis;
    int   items;                         /* indice en IT (listas desplegables y lista de pasos) o -1 */
    int   style;                         /* BUTTON: 1 = principal | DROP: 1 = solo flecha | LABEL: 1 tenue, 2 derecha, 3 negrita */
    int   ox, ow;                        /* lista desplegable: desplazamiento y ancho de la lista abierta */
    int   fv;                            /* LIST: primera fila visible */
} Widget;

typedef struct { int n; WCHAR s[64][72]; } Items;

static Widget g_w[220];
static int    nw;
static Items  IT[10];
static int    nIT;

static HWND g_wnd;
static int  g_page = 0, g_theme = 0;
static int  g_hot = -1, g_down = -1, g_focus = -1, g_drag = -1;
static int  g_open = -1, g_ohot = -1, g_ofirst = 0;
static int  g_cap = 0;
static HBITMAP g_emblem, g_word;
static int  g_ew = 1, g_eh = 1, g_ww = 1, g_wh = 1;
static HDC  g_mem;
static HBITMAP g_memb, g_memold;
static int  g_mw, g_mh;

#define WIN_H_LOGICAL 560

static COLORREF mix(COLORREF a, COLORREF b, int pct)
{
    int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * pct / 100;
    int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * pct / 100;
    int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * pct / 100;
    return RGB(r, g, bl);
}

static int add(int type, int id, int page, int x, int y, int w, int h, const WCHAR *text)
{
    Widget *p;
    if (nw >= (int)(sizeof g_w / sizeof g_w[0])) return 0;
    p = &g_w[nw];
    ZeroMemory(p, sizeof *p);
    p->type = type; p->id = id; p->page = page;
    p->x = x; p->y = y; p->w = w; p->h = h;
    p->enabled = 1; p->vis = 1; p->items = -1; p->max = 100;
    if (text) lstrcpynW(p->text, text, 360);
    return nw++;
}

static Widget *wid(int id)
{
    int i;
    for (i = 0; i < nw; i++) if (g_w[i].id == id && id) return &g_w[i];
    return NULL;
}

static int interactive(const Widget *p)
{
    switch (p->type) {
    case W_BUTTON: case W_BIG: case W_CHECK: case W_SEG: case W_SLIDER: case W_DROP:
    case W_TAB: case W_SWATCH: case W_LIST: return 1;
    }
    return 0;
}

static int shown(const Widget *p) { return p->vis && (p->page == -1 || p->page == g_page); }

static RECT wr(const Widget *p)
{
    RECT r;
    r.left = S(p->x); r.top = S(p->y); r.right = S(p->x + p->w); r.bottom = S(p->y + p->h);
    return r;
}

/* ===================================================================== */
/*  Construccion de las pantallas                                         */
/* ===================================================================== */
static int new_items(void) { return nIT < 10 ? nIT++ : 0; }

static void build(void)
{
    static const WCHAR *tabs[6] = { L"Macro", L"Secuencia", L"Disparo", L"Ventana", L"Aspecto", L"Extras" };
    int i, k;

    /* siempre visibles */
    add(W_BIG, ID_TOGGLE, -1, 16, 90, 408, 44, L"DESACTIVADO");
    i = add(W_LABEL, ID_TGLHINT, -1, 16, 90, 408, 44, L""); g_w[i].style = 2;
    for (k = 0; k < 6; k++) add(W_TAB, ID_TAB0 + k, -1, 16 + k * 68, 146, 68, 32, tabs[k]);

    /* ---- pagina 0: Macro ---- */
    add(W_CARD, 0, 0, 16, 188, 408, 98, NULL);
    add(W_HEAD, 0, 0, 30, 196, 240, 16, L"CONFIGURACIONES");
    add(W_FRAME, ID_CFG_FRAME, 0, 30, 216, 214, 30, NULL);
    i = add(W_DROP, ID_CFG_DROP, 0, 248, 216, 30, 30, L"");
    g_w[i].style = 1; g_w[i].ox = -218; g_w[i].ow = 250; g_w[i].items = new_items(); g_w[i].val = -1;
    i = add(W_BUTTON, ID_CFG_SAVE, 0, 284, 216, 64, 30, L"Guardar"); g_w[i].style = 1;
    add(W_BUTTON, ID_CFG_DEL, 0, 352, 216, 58, 30, L"Borrar");
    add(W_BUTTON, ID_CFG_IMPORT, 0, 30, 254, 100, 26, L"Importar…");
    add(W_BUTTON, ID_CFG_EXPORT, 0, 136, 254, 100, 26, L"Exportar…");
    add(W_HEAD, 0, 0, 16, 300, 300, 16, L"SECUENCIA SIMPLE");
    add(W_LABEL, 0, 0, 16, 328, 80, 20, L"Tecla");
    add(W_BUTTON, ID_KEY, 0, 100, 322, 160, 32, L"2");
    add(W_LABEL, 0, 0, 16, 372, 84, 20, L"Tiempo total");
    i = add(W_SLIDER, ID_MS, 0, 100, 366, 324, 32, L"%d ms"); g_w[i].min = 1; g_w[i].max = 100; g_w[i].val = 10;
    add(W_CHECK, ID_REPEAT, 0, 16, 408, 408, 24, L"Volver a presionar la tecla al final");
    add(W_HEAD, 0, 0, 16, 442, 300, 16, L"LÍNEA DE TIEMPO DE LA SECUENCIA ACTIVA");
    add(W_TIMELINE, ID_TL0, 0, 16, 462, 408, 52, NULL);

    /* ---- pagina 1: Secuencia personalizada ---- */
    add(W_CHECK, ID_MODE, 1, 16, 190, 408, 24, L"Usar una secuencia personalizada en lugar de la simple");
    i = add(W_LIST, ID_STEPLIST, 1, 16, 220, 408, 126, L"Aún no hay pasos. Agrega uno con «+ Tecla» o «+ Clic».");
    g_w[i].items = new_items(); g_w[i].val = -1;
    add(W_BUTTON, ID_ST_ADDK, 1, 16, 354, 78, 28, L"+ Tecla");
    add(W_BUTTON, ID_ST_ADDC, 1, 98, 354, 70, 28, L"+ Clic");
    add(W_BUTTON, ID_ST_DEL, 1, 172, 354, 64, 28, L"Quitar");
    add(W_BUTTON, ID_ST_UP, 1, 240, 354, 34, 28, L"▲");
    add(W_BUTTON, ID_ST_DOWN, 1, 278, 354, 34, 28, L"▼");
    add(W_BUTTON, ID_ST_COPY, 1, 316, 354, 108, 28, L"Copiar la simple");
    add(W_NOTE, ID_ST_HINT, 1, 16, 400, 408, 40, L"Selecciona un paso de la lista para cambiar su tecla o botón, cuánto tiempo lo mantiene y cuánto espera después.");
    add(W_LABEL, ID_ST_LBL, 1, 16, 394, 80, 20, L"Tecla");
    add(W_BUTTON, ID_ST_KEY, 1, 100, 388, 160, 32, L"2");
    i = add(W_SEG, ID_ST_B0, 1, 100, 388, 52, 32, L"Izq."); g_w[i].group = 3; g_w[i].val = 1;
    i = add(W_SEG, ID_ST_B1, 1, 156, 388, 52, 32, L"Der."); g_w[i].group = 3;
    i = add(W_SEG, ID_ST_B2, 1, 212, 388, 62, 32, L"Medio"); g_w[i].group = 3;
    add(W_LABEL, ID_ST_L2, 1, 16, 434, 84, 20, L"Mantener");
    i = add(W_SLIDER, ID_ST_HOLD, 1, 100, 428, 324, 32, L"%d ms"); g_w[i].min = 0; g_w[i].max = 200; g_w[i].val = 3;
    add(W_LABEL, ID_ST_L3, 1, 16, 468, 84, 20, L"Luego esperar");
    i = add(W_SLIDER, ID_ST_AFTER, 1, 100, 462, 324, 32, L"%d ms"); g_w[i].min = 0; g_w[i].max = 200; g_w[i].val = 1;
    add(W_TIMELINE, ID_TL1, 1, 16, 498, 408, 32, NULL);

    /* ---- pagina 2: Disparo ---- */
    add(W_HEAD, 0, 2, 16, 192, 300, 16, L"BOTÓN QUE DISPARA LA MACRO");
    i = add(W_SEG, ID_BTN_L, 2, 16, 214, 150, 34, L"Izquierdo"); g_w[i].group = 1; g_w[i].val = 1;
    i = add(W_SEG, ID_BTN_R, 2, 170, 214, 150, 34, L"Derecho"); g_w[i].group = 1;
    add(W_HEAD, 0, 2, 16, 262, 300, 16, L"CUÁNDO SE DISPARA");
    i = add(W_SEG, ID_TRG_REL, 2, 16, 284, 190, 34, L"Al soltar el clic"); g_w[i].group = 2; g_w[i].val = 1;
    i = add(W_SEG, ID_TRG_PRE, 2, 210, 284, 190, 34, L"Al presionar el clic"); g_w[i].group = 2;
    add(W_HEAD, 0, 2, 16, 332, 400, 16, L"ATAJOS (FUNCIONAN EN CUALQUIER VENTANA)");
    add(W_LABEL, 0, 2, 16, 364, 170, 20, L"Activar / desactivar");
    i = add(W_BUTTON, ID_HK_TOGGLE, 2, 190, 358, 170, 32, L"F8");
    i = add(W_BUTTON, ID_HK_TOGGLE_CLR, 2, 366, 358, 34, 32, L"×");
    add(W_LABEL, 0, 2, 16, 406, 180, 20, L"Siguiente configuración");
    i = add(W_BUTTON, ID_HK_NEXT, 2, 190, 400, 170, 32, L"(ninguna)");
    i = add(W_BUTTON, ID_HK_NEXT_CLR, 2, 366, 400, 34, 32, L"×");
    add(W_NOTE, 0, 2, 16, 444, 408, 80, L"Pulsa el botón y luego la tecla que quieras (sola o con Ctrl, Alt o Shift) o un botón lateral del mouse. Esa tecla ya no llegará al programa mientras JJEXE esté abierto: si eliges una letra o un número solos, dejarán de escribirse en otros programas. «Siguiente configuración» pasa a la próxima de tu lista guardada.");

    /* ---- pagina 3: Ventana ---- */
    add(W_CHECK, ID_TGT_ON, 3, 16, 190, 408, 24, L"Funcionar solo cuando esta ventana esté al frente");
    add(W_HEAD, 0, 3, 16, 224, 300, 16, L"VENTANA ELEGIDA");
    add(W_CARD, 0, 3, 16, 244, 408, 54, NULL);
    i = add(W_LABEL, ID_TGT_L1, 3, 30, 250, 384, 22, L"(ninguna)"); g_w[i].style = 3;
    i = add(W_LABEL, ID_TGT_L2, 3, 30, 273, 384, 20, L"Sin ventana elegida: la macro funciona en cualquier lado"); g_w[i].style = 1;
    i = add(W_BUTTON, ID_TGT_PICK, 3, 16, 306, 190, 32, L"Detectar ventana activa"); g_w[i].style = 1;
    add(W_BUTTON, ID_TGT_CLEAR, 3, 212, 306, 80, 32, L"Quitar");
    add(W_HEAD, 0, 3, 16, 350, 300, 16, L"O ELÍGELA DE LA LISTA");
    i = add(W_DROP, ID_TGT_LIST, 3, 16, 372, 328, 32, L"Ventanas abiertas…"); g_w[i].items = new_items(); g_w[i].val = -1;
    add(W_BUTTON, ID_TGT_REFRESH, 3, 350, 372, 74, 32, L"Actualizar");
    add(W_HEAD, 0, 3, 16, 418, 400, 16, L"EL TÍTULO DEBE CONTENER (OPCIONAL)");
    add(W_FRAME, ID_TGT_FRAME, 3, 16, 440, 408, 30, NULL);
    add(W_NOTE, 0, 3, 16, 478, 408, 48, L"Se reconoce el programa de la ventana (por ejemplo javaw.exe). Si el programa tiene varias ventanas, el título ayuda a distinguirlas.");

    /* ---- pagina 4: Aspecto ---- */
    add(W_HEAD, 0, 4, 16, 192, 200, 16, L"TEMA");
    for (k = 0; k < g_ntheme; k++) {
        i = add(W_SWATCH, ID_TH0 + k, 4, 16 + k * 68, 212, 62, 58, g_themes[k].name);
        g_w[i].val = k;
    }
    add(W_HEAD, 0, 4, 16, 286, 300, 16, L"ANIMACIÓN DE INICIO");
    i = add(W_DROP, ID_ANIM_DROP, 4, 16, 308, 244, 32, L"JJEXE (predeterminada)"); g_w[i].items = new_items(); g_w[i].val = 0;
    add(W_BUTTON, ID_ANIM_ADD, 4, 266, 308, 82, 32, L"Agregar…");
    add(W_BUTTON, ID_ANIM_FOLDER, 4, 352, 308, 72, 32, L"Carpeta");
    add(W_BUTTON, ID_ANIM_TEST, 4, 16, 346, 150, 30, L"Probar animación");
    add(W_NOTE, 0, 4, 172, 348, 252, 34, L"Acepta GIF (animado), PNG, JPG y BMP. Cada quien puede poner la suya.");
    add(W_HEAD, 0, 4, 16, 386, 200, 16, L"SISTEMA");
    add(W_CHECK, ID_TRAYMIN, 4, 16, 406, 408, 24, L"Minimizar a la bandeja al cerrar la ventana");
    add(W_CHECK, ID_OSD, 4, 16, 430, 408, 24, L"Aviso en pantalla al cambiar de estado o de configuración");
    add(W_CHECK, ID_STREAM, 4, 16, 454, 408, 24, L"Modo stream (aviso oculto en grabaciones y sin pitido)");
    add(W_CHECK, ID_STREAM_MAIN, 4, 16, 478, 408, 24, L"En modo stream, ocultar también esta ventana de JJEXE");

    /* ---- pagina 5: Extras (sonidos y aviso de version) ---- */
    add(W_HEAD, 0, 5, 16, 192, 300, 16, L"SONIDOS");
    add(W_CHECK, ID_SOUND, 5, 16, 212, 408, 24, L"Sonido al activar y desactivar (en modo stream no suena)");
    add(W_LABEL, 0, 5, 16, 242, 90, 32, L"Al activar");
    i = add(W_DROP, ID_SND_ON, 5, 108, 242, 236, 32, L"Tono predeterminado"); g_w[i].items = new_items(); g_w[i].val = 0;
    add(W_BUTTON, ID_SND_ON_TEST, 5, 350, 242, 74, 32, L"Probar");
    add(W_LABEL, 0, 5, 16, 278, 90, 32, L"Al desactivar");
    i = add(W_DROP, ID_SND_OFF, 5, 108, 278, 236, 32, L"Tono predeterminado"); g_w[i].items = new_items(); g_w[i].val = 0;
    add(W_BUTTON, ID_SND_OFF_TEST, 5, 350, 278, 74, 32, L"Probar");
    add(W_LABEL, 0, 5, 16, 314, 90, 32, L"Volumen");
    i = add(W_SLIDER, ID_SND_VOL, 5, 108, 314, 316, 32, L"%d"); g_w[i].min = 0; g_w[i].max = 100; g_w[i].val = 70;
    add(W_BUTTON, ID_SND_ADD, 5, 16, 352, 140, 30, L"Agregar .wav…");
    add(W_BUTTON, ID_SND_FOLDER, 5, 162, 352, 80, 30, L"Carpeta");
    add(W_NOTE, 0, 5, 250, 355, 174, 30, L"Acepta .wav comunes (PCM).");
    add(W_HEAD, 0, 5, 16, 392, 300, 16, L"AVISO DE VERSIÓN NUEVA");
    add(W_CHECK, ID_UPD_ON, 5, 16, 412, 408, 24, L"Buscar versión nueva al abrir JJEXE");
    add(W_NOTE, 0, 5, 16, 440, 408, 16, L"Enlace https a un texto: línea 1 = versión, línea 2 = descarga.");
    add(W_FRAME, ID_UPD_FRAME, 5, 16, 460, 408, 30, NULL);
    add(W_BUTTON, ID_UPD_NOW, 5, 16, 498, 120, 30, L"Buscar ahora");
    add(W_BUTTON, ID_UPD_OPEN, 5, 142, 498, 130, 30, L"Abrir descarga");
    i = add(W_LABEL, ID_UPD_STATUS, 5, 278, 498, 146, 30, L""); g_w[i].style = 1;
}

/* ===================================================================== */
/*  Dibujo                                                                */
/* ===================================================================== */
static void txt(HDC dc, const WCHAR *s, RECT r, COLORREF c, HFONT f, UINT fmt)
{
    HGDIOBJ o = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &r, fmt | DT_NOPREFIX);
    SelectObject(dc, o);
}

static void fill_rr(void *g, RECT r, int rad, COLORREF c, int a)
{
    gp_fill_rr(g, (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)rad, c, a);
}

static void grad_rr(void *g, RECT r, int rad, COLORREF c1, COLORREF c2, int vertical)
{
    gp_fill_rr_grad(g, (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)rad, c1, c2, 255, vertical);
}

static void stroke_rr(void *g, RECT r, int rad, COLORREF c, int a, float pw)
{
    gp_stroke_rr(g, r.left + 0.5f, r.top + 0.5f, (float)(r.right - r.left) - 1.0f, (float)(r.bottom - r.top) - 1.0f, (float)rad, c, a, pw);
}

static void chevron(void *g, int cx, int cy, COLORREF c)
{
    float d = (float)S(4);
    gp_line(g, cx - d, cy - d * 0.5f, (float)cx, cy + d * 0.5f, c, 255, (float)S(2));
    gp_line(g, (float)cx, cy + d * 0.5f, cx + d, cy - d * 0.5f, c, 255, (float)S(2));
}

static void overlay_geom(const Widget *p, RECT *r, int *rows)
{
    int n = IT[p->items].n, vis = n < 8 ? n : 8, x, w, y, hg;
    if (vis < 1) vis = 1;
    x = p->x + p->ox;
    w = p->ow ? p->ow : p->w;
    y = p->y + p->h + 2;
    hg = vis * 24 + 8;
    if (y + hg > WIN_H_LOGICAL - 4) y = p->y - hg - 2;           /* si no cabe abajo, se abre hacia arriba */
    r->left = S(x); r->top = S(y); r->right = S(x + w); r->bottom = S(y + hg);
    *rows = vis;
}

static void draw_timeline(HDC m, void *g, const Widget *p, RECT R)
{
    const Theme *t = &g_themes[g_theme];
    EvList L;
    WCHAR b[40];
    int i, j, pad = S(10), x0 = R.left + pad, x1 = R.right - pad, ytop = R.top + S(6);
    int laneh = (R.bottom - R.top) > S(40) ? S(12) : S(7), gap = S(4);
    (void)p;
    fill_rr(g, R, S(8), t->input, 255);
    stroke_rr(g, R, S(8), t->border, 255, 1.0f);
    eng_build(&g_macro, &L);
    if (L.n == 0 || L.total_us <= 0) {
        RECT q = R;
        txt(m, L"Sin pasos: la macro no hace nada", q, t->dim, g_fontS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    for (i = 0; i < L.n; i++) {
        int kind = L.ev[i].kind, xa, xb, lane;
        if (kind != EV_KDOWN && kind != EV_MDOWN) continue;
        for (j = i + 1; j < L.n; j++) if (L.ev[j].kind == kind + 1 && L.ev[j].code == L.ev[i].code) break;
        if (j >= L.n) continue;
        xa = x0 + (int)((long long)(x1 - x0) * L.ev[i].t_us / L.total_us);
        xb = x0 + (int)((long long)(x1 - x0) * L.ev[j].t_us / L.total_us);
        if (xb - xa < S(3)) xb = xa + S(3);
        lane = (kind == EV_KDOWN) ? 0 : 1;
        gp_fill_rr_grad(g, (float)xa, (float)(ytop + lane * (laneh + gap)), (float)(xb - xa), (float)laneh, 3.0f,
                        lane ? t->accent2 : t->accent, lane ? mix(t->accent2, t->accent, 40) : mix(t->accent, t->accent2, 40), 255, 1);
    }
    {
        RECT q;
        int ms10 = L.total_us / 100;
        q.left = x0; q.right = x1; q.bottom = R.bottom - S(2); q.top = q.bottom - S(14);
        txt(m, L"0", q, t->dim, g_fontS, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        wsprintfW(b, L"%d,%d ms", ms10 / 10, ms10 % 10);
        txt(m, b, q, t->dim, g_fontS, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        txt(m, L"■ tecla   ■ clic", q, t->dim, g_fontS, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
}

static void draw_widget(HDC m, void *g, int i)
{
    const Theme *t = &g_themes[g_theme];
    Widget *p = &g_w[i];
    RECT R = wr(p), r;
    int hot = (i == g_hot), dn = (i == g_down && hot);
    COLORREF tc;

    switch (p->type) {
    case W_CARD:
        fill_rr(g, R, S(10), t->card, 255);
        stroke_rr(g, R, S(10), t->border, 255, 1.0f);
        break;

    case W_HEAD:
        txt(m, p->text, R, mix(t->accent, t->text, 25), g_fontSB, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break;

    case W_LABEL: {
        UINT f = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
        HFONT fo = g_font;
        tc = t->text;
        r = R;
        if (p->style == 1) tc = t->dim;
        if (p->style == 2) {
            Widget *tg = wid(ID_TOGGLE);
            tc = (tg && tg->val) ? RGB(222, 255, 236) : t->dim;
            fo = g_fontS; r.right -= S(16); f |= DT_RIGHT;
        }
        else f |= DT_LEFT;
        if (p->style == 3) fo = g_fontB;
        txt(m, p->text, r, tc, fo, f);
        break; }

    case W_NOTE:
        txt(m, p->text, R, t->dim, g_fontS, DT_LEFT | DT_TOP | DT_WORDBREAK);
        break;

    case W_SEP:
        gp_line(g, (float)R.left, (float)R.top, (float)R.right, (float)R.top, t->border, 255, 1.0f);
        break;

    case W_FRAME: {
        int foc = ui_edit_focus(p->id);
        fill_rr(g, R, S(7), t->input, 255);
        stroke_rr(g, R, S(7), foc ? t->accent : t->border, 255, foc ? 1.6f : 1.0f);
        break; }

    case W_BUTTON: {
        int en = p->enabled;
        tc = t->text;
        if (!en) { fill_rr(g, R, S(7), t->btn, 110); tc = t->dim; }
        else if (p->style == 1) {
            COLORREF a = dn ? mix(t->accent, RGB(0,0,0), 35) : hot ? mix(t->accent, RGB(255,255,255), 14) : t->accent;
            grad_rr(g, R, S(7), a, mix(a, RGB(0,0,0), 28), 1);
            tc = RGB(255, 255, 255);
        } else {
            fill_rr(g, R, S(7), dn ? mix(t->hot, RGB(0,0,0), 20) : hot ? t->hot : t->btn, 255);
            stroke_rr(g, R, S(7), hot ? mix(t->border, t->accent, 60) : t->border, 255, 1.0f);
        }
        txt(m, p->text, R, tc, g_fontB, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break; }

    case W_BIG: {
        int on = p->val;
        COLORREF dot;
        if (on) {
            COLORREF a = hot ? mix(t->good, RGB(255,255,255), 12) : t->good;
            grad_rr(g, R, S(11), a, mix(a, RGB(0,0,0), 38), 1);
            dot = RGB(255, 255, 255); tc = RGB(255, 255, 255);
        } else {
            fill_rr(g, R, S(11), dn ? mix(t->hot, RGB(0,0,0), 20) : hot ? t->hot : t->btn, 255);
            stroke_rr(g, R, S(11), mix(t->border, t->accent, hot ? 90 : 45), 255, 1.2f);
            dot = t->bad; tc = t->text;
        }
        gp_fill_ellipse(g, (float)(R.left + S(16)), (float)((R.top + R.bottom) / 2 - S(6)), (float)S(12), (float)S(12), dot, 255);
        r = R; r.left += S(38);
        txt(m, p->text, r, tc, g_fontBig, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        break; }

    case W_CHECK: {
        int bs = S(18), bx = R.left, by = R.top + (R.bottom - R.top - bs) / 2;
        RECT b;
        b.left = bx; b.top = by; b.right = bx + bs; b.bottom = by + bs;
        if (p->val) {
            grad_rr(g, b, S(5), t->accent, t->accent2, 1);
            gp_line(g, bx + bs * 0.24f, by + bs * 0.52f, bx + bs * 0.43f, by + bs * 0.72f, RGB(255,255,255), 255, (float)S(2));
            gp_line(g, bx + bs * 0.43f, by + bs * 0.72f, bx + bs * 0.77f, by + bs * 0.30f, RGB(255,255,255), 255, (float)S(2));
        } else {
            fill_rr(g, b, S(5), t->input, 255);
            stroke_rr(g, b, S(5), hot ? t->accent : t->border, 255, 1.4f);
        }
        r = R; r.left = bx + bs + S(10);
        txt(m, p->text, r, t->text, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break; }

    case W_SEG:
        if (p->val) {
            grad_rr(g, R, S(8), t->accent, mix(t->accent, t->accent2, 45), 0);
            tc = RGB(255, 255, 255);
        } else {
            fill_rr(g, R, S(8), dn ? mix(t->hot, RGB(0,0,0), 20) : hot ? t->hot : t->btn, 255);
            stroke_rr(g, R, S(8), t->border, 255, 1.0f);
            tc = t->text;
        }
        txt(m, p->text, R, tc, g_fontB, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;

    case W_SLIDER: {
        int x0 = R.left + S(8), x1 = R.right - S(62), cy = (R.top + R.bottom) / 2, tx, th = S(10);
        float frac = p->max > p->min ? (float)(p->val - p->min) / (float)(p->max - p->min) : 0.0f;
        WCHAR b[40];
        RECT q;
        tx = x0 + (int)((x1 - x0) * frac);
        gp_fill_rr(g, (float)x0, (float)(cy - S(3)), (float)(x1 - x0), (float)S(6), (float)S(3), mix(t->border, t->bg, 20), 255);
        if (tx > x0) gp_fill_rr_grad(g, (float)x0, (float)(cy - S(3)), (float)(tx - x0), (float)S(6), (float)S(3), t->accent, t->accent2, 255, 0);
        if (i == g_drag || hot) th = S(11);
        gp_fill_ellipse(g, (float)(tx - th), (float)(cy - th), (float)(th * 2), (float)(th * 2), RGB(0,0,0), 60);
        gp_fill_ellipse(g, (float)(tx - th), (float)(cy - th - 1), (float)(th * 2), (float)(th * 2), mix(RGB(255,255,255), t->text, 5), 255);
        gp_stroke_ellipse(g, (float)(tx - th) + 1.0f, (float)(cy - th), (float)(th * 2) - 2.0f, (float)(th * 2) - 2.0f, t->accent, 255, (float)S(2));
        wsprintfW(b, p->text, p->val);
        q = R; q.left = x1 + S(12);
        txt(m, b, q, t->text, g_fontB, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        break; }

    case W_DROP: {
        int open = (g_open == i), cx;
        fill_rr(g, R, S(7), open ? t->hot : hot ? mix(t->input, t->hot, 50) : t->input, 255);
        stroke_rr(g, R, S(7), (open || hot) ? t->accent : t->border, 255, (open) ? 1.6f : 1.0f);
        cx = R.right - S(15);
        chevron(g, cx, (R.top + R.bottom) / 2, open ? t->accent2 : t->dim);
        if (p->style != 1) {
            const WCHAR *s = p->text;
            if (p->items >= 0 && p->val >= 0 && p->val < IT[p->items].n) s = IT[p->items].s[p->val];
            r = R; r.left += S(12); r.right -= S(30);
            txt(m, s, r, (p->items >= 0 && p->val >= 0) ? t->text : t->dim, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        break; }

    case W_TAB: {
        int sel = (p->id - ID_TAB0 == g_page);
        r = R;
        txt(m, p->text, r, sel ? t->text : (hot ? t->text : t->dim), sel ? g_fontB : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (sel) {
            RECT u = R;
            u.left += S(10); u.right -= S(10); u.top = R.bottom - S(3); u.bottom = R.bottom;
            grad_rr(g, u, S(2), t->accent, t->accent2, 0);
        } else if (hot) {
            RECT u = R;
            u.left += S(22); u.right -= S(22); u.top = R.bottom - S(2); u.bottom = R.bottom;
            fill_rr(g, u, S(1), t->border, 255);
        }
        break; }

    case W_SWATCH: {
        const Theme *k = &g_themes[p->val];
        RECT sw = R, band;
        int sel = (p->val == g_theme);
        sw.bottom = R.top + S(38);
        fill_rr(g, sw, S(9), k->bg, 255);
        band = sw; band.top = sw.bottom - S(12); band.left += S(6); band.right -= S(6); band.bottom -= S(6);
        grad_rr(g, band, S(4), k->accent, k->accent2, 0);
        band = sw; band.left += S(6); band.right = band.left + S(22); band.top += S(6); band.bottom = band.top + S(10);
        fill_rr(g, band, S(3), k->card, 255);
        stroke_rr(g, sw, S(9), sel ? t->accent : (hot ? mix(t->border, t->accent, 60) : t->border), 255, sel ? 2.2f : 1.0f);
        r = R; r.top = sw.bottom + S(2);
        txt(m, p->text, r, sel ? t->text : t->dim, g_fontS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break; }

    case W_LIST: {
        Items *it = &IT[p->items];
        int rh = S(22), vis = (R.bottom - R.top - S(12)) / rh, k;
        fill_rr(g, R, S(8), t->input, 255);
        stroke_rr(g, R, S(8), t->border, 255, 1.0f);
        if (it->n == 0) {
            r = R; r.left += S(12); r.right -= S(12);
            txt(m, p->text, r, t->dim, g_fontS, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            break;
        }
        for (k = 0; k < vis && p->fv + k < it->n; k++) {
            int idx = p->fv + k;
            RECT row;
            row.left = R.left + S(6); row.right = R.right - S(10);
            row.top = R.top + S(6) + k * rh; row.bottom = row.top + rh;
            if (idx == p->val) {
                fill_rr(g, row, S(5), t->accent, 70);
                stroke_rr(g, row, S(5), t->accent, 255, 1.0f);
            }
            r = row; r.left += S(8); r.right -= S(4);
            txt(m, it->s[idx], r, idx == p->val ? t->text : mix(t->text, t->dim, 25), g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        if (it->n > vis) {
            int th2 = (R.bottom - R.top - S(12)), bh = th2 * vis / it->n, by = R.top + S(6) + (th2 - bh) * p->fv / (it->n - vis);
            gp_fill_rr(g, (float)(R.right - S(7)), (float)by, (float)S(3), (float)bh, (float)S(1), t->dim, 180);
        }
        break; }

    case W_TIMELINE:
        draw_timeline(m, g, p, R);
        break;
    }
}

static void draw_banner(HDC m, void *g, int Wp)
{
    const Theme *t = &g_themes[g_theme];
    int bh = S(76), eh = S(62), ew = eh * g_ew / g_eh, ww = S(190), wh = ww * g_wh / g_ww;
    RECT line;
    HDC b;
    gp_fill_rr_grad(g, 0.0f, 0.0f, (float)Wp, (float)bh, 0.0f, t->head1, t->head2, 255, 1);
    SetStretchBltMode(m, HALFTONE);
    SetBrushOrgEx(m, 0, 0, NULL);
    b = CreateCompatibleDC(m);
    if (g_emblem) {
        HGDIOBJ o = SelectObject(b, g_emblem);
        StretchBlt(m, S(22), (bh - eh) / 2, ew, eh, b, 0, 0, g_ew, g_eh, SRCPAINT);
        SelectObject(b, o);
    }
    if (g_word) {
        HGDIOBJ o = SelectObject(b, g_word);
        StretchBlt(m, S(22) + ew + S(16), (bh - wh) / 2, ww, wh, b, 0, 0, g_ww, g_wh, SRCPAINT);
        SelectObject(b, o);
    }
    DeleteDC(b);
    line.left = 0; line.right = Wp; line.top = bh; line.bottom = bh + S(3);
    gp_fill_rr_grad(g, 0.0f, (float)line.top, (float)Wp, (float)S(3), 0.0f, t->accent, t->accent2, 255, 0);
}

static void draw_overlay(HDC m, void *g)
{
    const Theme *t = &g_themes[g_theme];
    Widget *p;
    RECT R, row;
    int rows, k, n, rh = S(24);
    if (g_open < 0) return;
    p = &g_w[g_open];
    overlay_geom(p, &R, &rows);
    n = IT[p->items].n;
    {
        RECT sh = R;
        sh.left -= 2; sh.top += S(2); sh.right += 2; sh.bottom += S(5);
        fill_rr(g, sh, S(9), RGB(0, 0, 0), 70);
    }
    fill_rr(g, R, S(9), t->card, 255);
    stroke_rr(g, R, S(9), t->accent, 255, 1.2f);
    if (n == 0) {
        row = R;
        txt(m, L"(vacío)", row, t->dim, g_fontS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    for (k = 0; k < rows && g_ofirst + k < n; k++) {
        int idx = g_ofirst + k;
        row.left = R.left + S(4); row.right = R.right - S(4);
        row.top = R.top + S(4) + k * rh; row.bottom = row.top + rh;
        if (idx == g_ohot) fill_rr(g, row, S(6), t->accent, 90);
        else if (idx == p->val) fill_rr(g, row, S(6), t->hot, 255);
        row.left += S(8); row.right -= S(6);
        txt(m, IT[p->items].s[idx], row, t->text, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (n > rows) {
        int th2 = R.bottom - R.top - S(8), bh = th2 * rows / n, by = R.top + S(4) + (th2 - bh) * g_ofirst / (n - rows);
        gp_fill_rr(g, (float)(R.right - S(7)), (float)by, (float)S(3), (float)bh, (float)S(1), t->dim, 200);
    }
}

void ui_paint(HDC dc, int Wp, int Hp)
{
    const Theme *t = &g_themes[g_theme];
    HBRUSH b;
    RECT rc, foot;
    void *g;
    int i;

    if (!g_mem || g_mw != Wp || g_mh != Hp) {
        if (g_mem) { SelectObject(g_mem, g_memold); DeleteObject(g_memb); DeleteDC(g_mem); }
        g_mem = CreateCompatibleDC(dc);
        g_memb = CreateCompatibleBitmap(dc, Wp, Hp);
        g_memold = (HBITMAP)SelectObject(g_mem, g_memb);
        g_mw = Wp; g_mh = Hp;
    }
    rc.left = 0; rc.top = 0; rc.right = Wp; rc.bottom = Hp;
    b = CreateSolidBrush(t->bg);
    FillRect(g_mem, &rc, b);
    DeleteObject(b);

    g = gp_begin(g_mem);
    draw_banner(g_mem, g, Wp);
    for (i = 0; i < nw; i++) {
        if (!shown(&g_w[i])) continue;
        draw_widget(g_mem, g, i);
    }
    /* anillo de foco (teclado) */
    if (g_focus >= 0 && g_focus < nw && shown(&g_w[g_focus]) && interactive(&g_w[g_focus]) && g_w[g_focus].type != W_LIST) {
        RECT f = wr(&g_w[g_focus]);
        f.left -= S(2); f.top -= S(2); f.right += S(2); f.bottom += S(2);
        stroke_rr(g, f, S(9), t->accent2, 255, 1.6f);
    }
    /* pie */
    foot.left = S(16); foot.right = Wp - S(16); foot.top = S(536); foot.bottom = S(556);
    txt(g_mem, L"Developers: Claudio and la perla negra", foot, t->dim, g_fontS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    txt(g_mem, L"JJEXE " APP_VER, foot, t->dim, g_fontS, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    draw_overlay(g_mem, g);
    gp_end(g);
    BitBlt(dc, 0, 0, Wp, Hp, g_mem, 0, 0, SRCCOPY);
}

/* ===================================================================== */
/*  Entrada del usuario                                                   */
/* ===================================================================== */
void ui_invalidate(void) { if (g_wnd) InvalidateRect(g_wnd, NULL, FALSE); }

static int hit(int lx, int ly)
{
    int i;
    for (i = nw - 1; i >= 0; i--) {
        const Widget *p = &g_w[i];
        if (!shown(p) || !interactive(p) || !p->enabled) continue;
        if (lx >= p->x && lx < p->x + p->w && ly >= p->y && ly < p->y + p->h) return i;
    }
    return -1;
}

static void drop_open(int i)
{
    Widget *p = &g_w[i];
    RECT R;
    int rows;
    on_ui(p->id, -1, 0);                                         /* "se va a abrir": la ventana principal puede refrescar la lista */
    g_open = i;
    g_ohot = p->val;
    overlay_geom(p, &R, &rows);
    g_ofirst = 0;
    if (p->val >= rows) g_ofirst = p->val - rows + 1;
    on_ui(ID_OVERLAY, 1, 1);
    ui_invalidate();
}

void ui_drop_close(void)
{
    if (g_open >= 0) { g_open = -1; g_ohot = -1; on_ui(ID_OVERLAY, 0, 1); ui_invalidate(); }
}

int ui_drop_open(void) { return g_open >= 0; }

static void drop_pick(int idx)
{
    Widget *p;
    int id;
    if (g_open < 0) return;
    p = &g_w[g_open];
    id = p->id;
    if (idx >= 0 && idx < IT[p->items].n) p->val = idx;
    ui_drop_close();
    if (idx >= 0) on_ui(id, p->val, 1);
}

static void slider_set(int i, int lx)
{
    Widget *p = &g_w[i];
    int x0 = p->x + 8, x1 = p->x + p->w - 62, v;
    if (x1 <= x0) return;
    if (lx <= x0) v = p->min;
    else if (lx >= x1) v = p->max;
    else v = p->min + ((lx - x0) * (p->max - p->min) + (x1 - x0) / 2) / (x1 - x0);
    if (v < p->min) v = p->min;
    if (v > p->max) v = p->max;
    if (v != p->val) { p->val = v; on_ui(p->id, v, 0); ui_invalidate(); }
}

static void activate(int i)
{
    Widget *p = &g_w[i];
    int j;
    switch (p->type) {
    case W_BUTTON: case W_BIG:
        on_ui(p->id, 0, 1);
        break;
    case W_CHECK:
        p->val = !p->val;
        on_ui(p->id, p->val, 1);
        break;
    case W_SEG:
        for (j = 0; j < nw; j++) if (g_w[j].type == W_SEG && g_w[j].group == p->group) g_w[j].val = 0;
        p->val = 1;
        on_ui(p->id, 1, 1);
        break;
    case W_TAB:
        g_page = p->id - ID_TAB0;
        g_focus = -1;
        on_ui(p->id, g_page, 1);
        break;
    case W_SWATCH:
        on_ui(p->id, p->val, 1);
        break;
    case W_DROP:
        drop_open(i);
        break;
    }
    ui_invalidate();
}

static void list_click(int i, int ly)
{
    Widget *p = &g_w[i];
    int row = (ly - (p->y + 6)) / 22 + p->fv;
    if (row >= 0 && row < IT[p->items].n) { p->val = row; on_ui(p->id, row, 1); }
}

static int overlay_mouse(HWND hwnd, UINT msg, int px, int py)
{
    Widget *p = &g_w[g_open];
    RECT R;
    int rows, inside, idx = -1;
    (void)hwnd;
    overlay_geom(p, &R, &rows);
    inside = px >= R.left && px < R.right && py >= R.top && py < R.bottom;
    if (inside) {
        idx = g_ofirst + (py - R.top - S(4)) / S(24);
        if (idx < 0 || idx >= IT[p->items].n || idx >= g_ofirst + rows) idx = -1;
    }
    switch (msg) {
    case WM_MOUSEMOVE:
        if (idx != g_ohot) { g_ohot = idx; ui_invalidate(); }
        break;
    case WM_LBUTTONDOWN:
        if (inside) { if (idx >= 0) drop_pick(idx); }
        else {                                                   /* clic fuera: cerrar (y si fue sobre su boton, no reabrir) */
            ui_drop_close();
            g_down = -1;
        }
        break;
    case WM_LBUTTONUP:
        break;
    }
    return 1;
}

int ui_mouse(HWND hwnd, UINT msg, int px, int py)
{
    int lx = px * 96 / g_dpi, ly = py * 96 / g_dpi, i;

    if (msg == WM_MOUSELEAVE) {
        if (g_hot != -1) { g_hot = -1; ui_invalidate(); }
        return 0;
    }
    if (g_open >= 0) return overlay_mouse(hwnd, msg, px, py);

    switch (msg) {
    case WM_MOUSEMOVE:
        if (g_drag >= 0) { slider_set(g_drag, lx); return 1; }
        i = hit(lx, ly);
        if (i != g_hot) { g_hot = i; ui_invalidate(); }
        return 0;
    case WM_LBUTTONDOWN:
        i = hit(lx, ly);
        if (i < 0) return 0;
        SetCapture(hwnd);
        g_down = i;
        g_focus = i;
        if (g_w[i].type == W_SLIDER) { g_drag = i; slider_set(i, lx); }
        else if (g_w[i].type == W_LIST) list_click(i, ly);
        ui_invalidate();
        return 1;
    case WM_LBUTTONUP:
        if (g_drag >= 0) {
            int d = g_drag;
            g_drag = -1; g_down = -1;
            ReleaseCapture();
            on_ui(g_w[d].id, g_w[d].val, 1);
            ui_invalidate();
            return 1;
        }
        if (g_down >= 0) {
            int d = g_down;
            g_down = -1;
            ReleaseCapture();
            if (hit(lx, ly) == d && g_w[d].type != W_LIST) activate(d);
            ui_invalidate();
            return 1;
        }
        return 0;
    }
    return 0;
}

static void focus_step(int dir)
{
    int i, n = 0, cur = g_focus;
    for (i = 0; i < nw; i++) {
        int k = (cur + dir * (i + 1) + nw * 4) % nw;
        const Widget *p = &g_w[k];
        if (shown(p) && interactive(p) && p->enabled && p->type != W_TAB) { g_focus = k; n = 1; break; }
    }
    if (!n) g_focus = -1;
}

int ui_key(HWND hwnd, WPARAM vk, LPARAM lp)
{
    (void)hwnd; (void)lp;
    if (g_open >= 0) {                                           /* lista desplegable abierta */
        Widget *p = &g_w[g_open];
        RECT R;
        int rows, n = IT[p->items].n;
        overlay_geom(p, &R, &rows);
        switch (vk) {
        case VK_ESCAPE: ui_drop_close(); return 1;
        case VK_RETURN: case VK_SPACE: drop_pick(g_ohot); return 1;
        case VK_UP:   if (g_ohot > 0) g_ohot--; else g_ohot = 0; break;
        case VK_DOWN: if (g_ohot < n - 1) g_ohot++; break;
        default: return 1;
        }
        if (g_ohot < g_ofirst) g_ofirst = g_ohot;
        if (g_ohot >= g_ofirst + rows) g_ofirst = g_ohot - rows + 1;
        ui_invalidate();
        return 1;
    }
    if (vk == VK_TAB) { focus_step((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1); ui_invalidate(); return 1; }
    if (g_focus >= 0 && g_focus < nw && shown(&g_w[g_focus])) {
        Widget *p = &g_w[g_focus];
        if (vk == VK_SPACE || vk == VK_RETURN) {
            if (p->type != W_SLIDER && p->type != W_LIST) { activate(g_focus); return 1; }
        }
        if (p->type == W_SLIDER && (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT)) {
            int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1, v = p->val;
            if (vk == VK_LEFT) v -= step;
            else if (vk == VK_RIGHT) v += step;
            else if (vk == VK_PRIOR) v += 10;
            else if (vk == VK_NEXT) v -= 10;
            else if (vk == VK_HOME) v = p->min;
            else v = p->max;
            if (v < p->min) v = p->min;
            if (v > p->max) v = p->max;
            if (v != p->val) { p->val = v; on_ui(p->id, v, 0); }
            on_ui(p->id, p->val, 1);
            ui_invalidate();
            return 1;
        }
        if (p->type == W_LIST && (vk == VK_UP || vk == VK_DOWN)) {
            int n = IT[p->items].n, v = p->val, vis = (S(p->h) - S(12)) / S(22);
            if (n == 0) return 1;
            if (vk == VK_UP) v = v <= 0 ? 0 : v - 1; else v = v < 0 ? 0 : (v < n - 1 ? v + 1 : n - 1);
            p->val = v;
            if (v < p->fv) p->fv = v;
            if (v >= p->fv + vis) p->fv = v - vis + 1;
            on_ui(p->id, v, 1);
            ui_invalidate();
            return 1;
        }
        if (p->type == W_DROP && (vk == VK_UP || vk == VK_DOWN)) {
            int n = IT[p->items].n, v = p->val;
            if (n == 0) return 1;
            if (vk == VK_UP) v = v <= 0 ? 0 : v - 1; else v = v < n - 1 ? v + 1 : n - 1;
            if (v != p->val) { p->val = v; on_ui(p->id, v, 1); ui_invalidate(); }
            return 1;
        }
    }
    return 0;
}

void ui_wheel(int delta, int px, int py)
{
    int step = delta < 0 ? 1 : -1;
    if (g_open >= 0) {
        Widget *p = &g_w[g_open];
        RECT R;
        int rows, n = IT[p->items].n;
        overlay_geom(p, &R, &rows);
        g_ofirst += step * 2;
        if (g_ofirst > n - rows) g_ofirst = n - rows;
        if (g_ofirst < 0) g_ofirst = 0;
        ui_invalidate();
        return;
    }
    {
        int i = hit(px * 96 / g_dpi, py * 96 / g_dpi);
        if (i >= 0 && g_w[i].type == W_LIST) {
            Widget *p = &g_w[i];
            int n = IT[p->items].n, vis = (S(p->h) - S(12)) / S(22);
            p->fv += step;
            if (p->fv > n - vis) p->fv = n - vis;
            if (p->fv < 0) p->fv = 0;
            ui_invalidate();
        }
    }
}

/* ===================================================================== */
/*  Acceso desde la ventana principal                                     */
/* ===================================================================== */
void ui_init(HWND hwnd)
{
    BITMAP bm;
    g_wnd = hwnd;
    g_emblem = (HBITMAP)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_BMP_EMBLEM), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    g_word   = (HBITMAP)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(RES_BMP_WORD), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (g_emblem && GetObjectW(g_emblem, sizeof bm, &bm)) { g_ew = bm.bmWidth; g_eh = bm.bmHeight; }
    if (g_word && GetObjectW(g_word, sizeof bm, &bm))     { g_ww = bm.bmWidth; g_wh = bm.bmHeight; }
    if (g_ew < 1) g_ew = 1;
    if (g_eh < 1) g_eh = 1;
    if (g_ww < 1) g_ww = 1;
    if (g_wh < 1) g_wh = 1;
    nw = 0; nIT = 0;
    build();
}

void ui_shutdown(void)
{
    if (g_mem) { SelectObject(g_mem, g_memold); DeleteObject(g_memb); DeleteDC(g_mem); g_mem = NULL; }
    if (g_emblem) DeleteObject(g_emblem);
    if (g_word) DeleteObject(g_word);
}

void ui_set_page(int p)  { g_page = p < 0 ? 0 : (p > 5 ? 5 : p); ui_invalidate(); }
int  ui_get_page(void)   { return g_page; }
void ui_set_theme(int t) { g_theme = (t < 0 || t >= g_ntheme) ? 0 : t; ui_invalidate(); }
const Theme *ui_theme(void) { return &g_themes[g_theme]; }

void ui_set_val(int id, int v)           { Widget *p = wid(id); if (p) { p->val = v; ui_invalidate(); } }
int  ui_get_val(int id)                  { Widget *p = wid(id); return p ? p->val : 0; }
void ui_set_text(int id, const WCHAR *t) { Widget *p = wid(id); if (p) { lstrcpynW(p->text, t, 360); ui_invalidate(); } }
void ui_set_vis(int id, int v)           { Widget *p = wid(id); if (p) { p->vis = v; ui_invalidate(); } }
void ui_set_enabled(int id, int e)       { Widget *p = wid(id); if (p) { p->enabled = e; ui_invalidate(); } }

void ui_items_clear(int id)
{
    Widget *p = wid(id);
    if (p && p->items >= 0) { IT[p->items].n = 0; p->fv = 0; }
}

void ui_items_add(int id, const WCHAR *t)
{
    Widget *p = wid(id);
    Items *it;
    if (!p || p->items < 0) return;
    it = &IT[p->items];
    if (it->n < 64) lstrcpynW(it->s[it->n++], t, 72);
}

int ui_items_count(int id)
{
    Widget *p = wid(id);
    return (p && p->items >= 0) ? IT[p->items].n : 0;
}

const WCHAR *ui_items_get(int id, int i)
{
    Widget *p = wid(id);
    if (!p || p->items < 0 || i < 0 || i >= IT[p->items].n) return L"";
    return IT[p->items].s[i];
}

void ui_rect(int id, RECT *r)
{
    Widget *p = wid(id);
    if (p) { r->left = p->x; r->top = p->y; r->right = p->x + p->w; r->bottom = p->y + p->h; }
}

void ui_capture_begin(int id)
{
    Widget *p = wid(id);
    g_cap = id;
    if (p) lstrcpynW(p->text, L"Presiona una tecla…", 360);
    ui_invalidate();
}

int ui_capturing(void) { return g_cap != 0; }
int ui_capture_id(void) { return g_cap; }

void ui_capture_end(int code)
{
    int id = g_cap;
    g_cap = 0;
    if (id) on_ui_capture(id, code);
    ui_invalidate();
}

void ui_list_reveal(int id)
{
    Widget *p = wid(id);
    int vis;
    if (!p || p->items < 0) return;
    vis = (p->h - 12) / 22;
    if (vis < 1) vis = 1;
    if (p->val < p->fv) p->fv = p->val < 0 ? 0 : p->val;
    if (p->val >= p->fv + vis) p->fv = p->val - vis + 1;
    ui_invalidate();
}

int ui_is_hot(void) { return g_hot >= 0 || g_open >= 0; }

int ui_edit_focus(int id)
{
    HWND h = (id == ID_CFG_FRAME) ? g_hEditName : (id == ID_TGT_FRAME ? g_hEditFilter : NULL);
    return h && GetFocus() == h;
}
