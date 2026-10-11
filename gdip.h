/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌​​​‌‌​​​​‌​​​​​‌​‌​‌‌‌‌‌‌​‌​​​‌​​‌‌​​‌‌‌‌‌​‌​‌​⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 8c20afd133ea
 */

#ifndef GDIP_H
#define GDIP_H
#include <windows.h>

/* Declaraciones minimas de la API "plana" de GDI+ (gdiplus.dll), para usarla desde C. */
typedef int GpStatus;
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GpStartupInput;
typedef struct { float X, Y; } GpPointF;

GpStatus WINAPI GdiplusStartup(ULONG_PTR *token, const GpStartupInput *input, void *output);
void     WINAPI GdiplusShutdown(ULONG_PTR token);
GpStatus WINAPI GdipCreateFromHDC(HDC hdc, void **graphics);
GpStatus WINAPI GdipDeleteGraphics(void *graphics);
GpStatus WINAPI GdipSetSmoothingMode(void *graphics, int mode);
GpStatus WINAPI GdipCreateSolidFill(DWORD argb, void **brush);
GpStatus WINAPI GdipDeleteBrush(void *brush);
GpStatus WINAPI GdipCreatePen1(DWORD argb, float width, int unit, void **pen);
GpStatus WINAPI GdipDeletePen(void *pen);
GpStatus WINAPI GdipCreatePath(int fillMode, void **path);
GpStatus WINAPI GdipDeletePath(void *path);
GpStatus WINAPI GdipAddPathArc(void *path, float x, float y, float w, float h, float start, float sweep);
GpStatus WINAPI GdipClosePathFigure(void *path);
GpStatus WINAPI GdipFillPath(void *graphics, void *brush, void *path);
GpStatus WINAPI GdipDrawPath(void *graphics, void *pen, void *path);
GpStatus WINAPI GdipFillEllipse(void *graphics, void *brush, float x, float y, float w, float h);
GpStatus WINAPI GdipDrawEllipse(void *graphics, void *pen, float x, float y, float w, float h);
GpStatus WINAPI GdipDrawLine(void *graphics, void *pen, float x1, float y1, float x2, float y2);
GpStatus WINAPI GdipCreateLineBrush(const GpPointF *p1, const GpPointF *p2, DWORD c1, DWORD c2, int wrap, void **brush);
/* imagenes (animaciones propias) */
GpStatus WINAPI GdipCreateBitmapFromFile(const WCHAR *file, void **bitmap);
GpStatus WINAPI GdipGetImageWidth(void *image, UINT *w);
GpStatus WINAPI GdipGetImageHeight(void *image, UINT *h);
GpStatus WINAPI GdipImageGetFrameCount(void *image, const GUID *dim, UINT *count);
GpStatus WINAPI GdipImageSelectActiveFrame(void *image, const GUID *dim, UINT index);
GpStatus WINAPI GdipGetPropertyItemSize(void *image, UINT id, UINT *size);
GpStatus WINAPI GdipGetPropertyItem(void *image, UINT id, UINT size, void *buf);
GpStatus WINAPI GdipGetImageThumbnail(void *image, UINT w, UINT h, void **thumb, void *cb, void *cbdata);
GpStatus WINAPI GdipCreateHBITMAPFromBitmap(void *bitmap, HBITMAP *hbm, DWORD background);
GpStatus WINAPI GdipDisposeImage(void *image);

/* ayudantes */
int   gp_start(void);                       /* 1 si GDI+ arranco */
void  gp_stop(void);
int   gp_ok(void);
void *gp_begin(HDC dc);                     /* Graphics con suavizado */
void  gp_end(void *g);
void  gp_fill_rr(void *g, float x, float y, float w, float h, float r, COLORREF c, int a);
void  gp_fill_rr_grad(void *g, float x, float y, float w, float h, float r, COLORREF c1, COLORREF c2, int a, int vertical);
void  gp_stroke_rr(void *g, float x, float y, float w, float h, float r, COLORREF c, int a, float pw);
void  gp_fill_ellipse(void *g, float x, float y, float w, float h, COLORREF c, int a);
void  gp_stroke_ellipse(void *g, float x, float y, float w, float h, COLORREF c, int a, float pw);
void  gp_line(void *g, float x1, float y1, float x2, float y2, COLORREF c, int a, float pw);
#endif
