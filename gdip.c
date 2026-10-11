/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠​‌​​‌​​​​​​​​‌​​​​​​‌​‌‌​‌​‌‌​​‌​‌‌‌‌​‌‌​​‌‌‌​‌​⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 48040b597b3a
 */

#include "gdip.h"

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_gdip[] = "wm:48040b597b3a";

static ULONG_PTR g_tok = 0;
static int g_ok = 0;

int gp_start(void)
{
    GpStartupInput in;
    if (g_ok) return 1;
    ZeroMemory(&in, sizeof in);
    in.GdiplusVersion = 1;
    g_ok = (GdiplusStartup(&g_tok, &in, NULL) == 0);
    return g_ok;
}

void gp_stop(void)
{
    if (g_ok) GdiplusShutdown(g_tok);
    g_ok = 0;
}

int gp_ok(void) { return g_ok; }

static DWORD argb(COLORREF c, int a)
{
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return ((DWORD)a << 24) | ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | (DWORD)GetBValue(c);
}

void *gp_begin(HDC dc)
{
    void *g = NULL;
    if (!g_ok) return NULL;
    if (GdipCreateFromHDC(dc, &g) != 0) return NULL;
    GdipSetSmoothingMode(g, 4);                 /* SmoothingModeAntiAlias */
    return g;
}

void gp_end(void *g)
{
    if (g) GdipDeleteGraphics(g);
}

static void *rr_path(float x, float y, float w, float h, float r)
{
    void *p = NULL;
    float d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    if (d < 1) d = 1;
    GdipCreatePath(0, &p);
    if (!p) return NULL;
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}

void gp_fill_rr(void *g, float x, float y, float w, float h, float r, COLORREF c, int a)
{
    void *b = NULL, *p;
    if (!g) return;
    GdipCreateSolidFill(argb(c, a), &b);
    p = rr_path(x, y, w, h, r);
    if (b && p) GdipFillPath(g, b, p);
    if (p) GdipDeletePath(p);
    if (b) GdipDeleteBrush(b);
}

void gp_fill_rr_grad(void *g, float x, float y, float w, float h, float r, COLORREF c1, COLORREF c2, int a, int vertical)
{
    void *b = NULL, *p;
    GpPointF p1, p2;
    if (!g) return;
    p1.X = x; p1.Y = y;
    if (vertical) { p2.X = x; p2.Y = y + h; } else { p2.X = x + w; p2.Y = y; }
    GdipCreateLineBrush(&p1, &p2, argb(c1, a), argb(c2, a), 0, &b);
    p = rr_path(x, y, w, h, r);
    if (b && p) GdipFillPath(g, b, p);
    if (p) GdipDeletePath(p);
    if (b) GdipDeleteBrush(b);
}

void gp_stroke_rr(void *g, float x, float y, float w, float h, float r, COLORREF c, int a, float pw)
{
    void *pen = NULL, *p;
    if (!g) return;
    GdipCreatePen1(argb(c, a), pw, 2, &pen);
    p = rr_path(x, y, w, h, r);
    if (pen && p) GdipDrawPath(g, pen, p);
    if (p) GdipDeletePath(p);
    if (pen) GdipDeletePen(pen);
}

void gp_fill_ellipse(void *g, float x, float y, float w, float h, COLORREF c, int a)
{
    void *b = NULL;
    if (!g) return;
    GdipCreateSolidFill(argb(c, a), &b);
    if (b) { GdipFillEllipse(g, b, x, y, w, h); GdipDeleteBrush(b); }
}

void gp_stroke_ellipse(void *g, float x, float y, float w, float h, COLORREF c, int a, float pw)
{
    void *pen = NULL;
    if (!g) return;
    GdipCreatePen1(argb(c, a), pw, 2, &pen);
    if (pen) { GdipDrawEllipse(g, pen, x, y, w, h); GdipDeletePen(pen); }
}

void gp_line(void *g, float x1, float y1, float x2, float y2, COLORREF c, int a, float pw)
{
    void *pen = NULL;
    if (!g) return;
    GdipCreatePen1(argb(c, a), pw, 2, &pen);
    if (pen) { GdipDrawLine(g, pen, x1, y1, x2, y2); GdipDeletePen(pen); }
}
