/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e
 */

#include "jj.h"
#include <wininet.h>

/* ===================================================================== */
/*  Aviso de version nueva.                                               */
/*  Lee un archivo de texto (solo https) con este formato:                */
/*     linea 1: numero de la ultima version (9, v10...)                   */
/*     linea 2: direccion https de descarga (opcional)                    */
/*     linea 3: nota corta (opcional)                                     */
/* ===================================================================== */
typedef struct { WCHAR url[256]; HWND hwnd; int manual; } UpdJob;

static int is_https(const WCHAR *s) { return !_wcsnicmp(s, L"https://", 8) && s[8]; }

static void line_w(const char *s, int len, WCHAR *out, int n)
{
    int k = MultiByteToWideChar(CP_UTF8, 0, s, len, out, n - 1);
    out[k > 0 ? k : 0] = 0;
}

static void parse_version(char *buf, DWORD total, UpdRes *r)
{
    const char *s = buf, *e = buf + total, *nl;
    int ver = 0, line = 0, any = 0;
    buf[total] = 0;
    if ((unsigned char)s[0] == 0xEF && total >= 3) s += 3;                      /* BOM */
    while (s < e && line < 3) {
        int len;
        nl = (const char *)memchr(s, '\n', (size_t)(e - s));
        len = (int)((nl ? nl : e) - s);
        while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == ' ')) len--;
        while (len > 0 && (*s == ' ' || *s == '\t')) { s++; len--; }
        if (line == 0) {
            if (len > 0 && (*s == 'v' || *s == 'V')) { s++; len--; }
            while (len > 0 && *s >= '0' && *s <= '9' && ver < 100000) { ver = ver * 10 + (*s - '0'); any = 1; s++; len--; }
        } else if (line == 1) {
            WCHAR u[256];
            line_w(s, len, u, 256);
            if (is_https(u)) lstrcpynW(r->url, u, 256);                           /* solo enlaces https */
        } else {
            line_w(s, len, r->note, 128);
        }
        line++;
        s = nl ? nl + 1 : e;
    }
    if (any) { r->ver = ver; r->status = ver > APP_VERNUM ? 1 : 0; }
}

static DWORD WINAPI UpdThread(LPVOID p)
{
    UpdJob *j = (UpdJob *)p;
    UpdRes *r = (UpdRes *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *r);
    char buf[2048];
    DWORD got = 0, total = 0, tmo = 7000;
    HINTERNET hi, hu;

    if (!r) { HeapFree(GetProcessHeap(), 0, j); return 0; }
    r->status = -1; r->manual = j->manual;
    hi = InternetOpenW(L"JJEXE/" APP_VER, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hi) {
        InternetSetOptionW(hi, INTERNET_OPTION_CONNECT_TIMEOUT, &tmo, sizeof tmo);
        InternetSetOptionW(hi, INTERNET_OPTION_RECEIVE_TIMEOUT, &tmo, sizeof tmo);
        hu = InternetOpenUrlW(hi, j->url, NULL, 0,
                              INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_SECURE, 0);
        if (hu) {
            while (total < sizeof buf - 1 && InternetReadFile(hu, buf + total, (DWORD)(sizeof buf - 1 - total), &got) && got) total += got;
            InternetCloseHandle(hu);
        }
        InternetCloseHandle(hi);
    }
    if (total) parse_version(buf, total, r);
    if (!PostMessageW(j->hwnd, WM_APP_UPDATE, 0, (LPARAM)r)) HeapFree(GetProcessHeap(), 0, r);
    HeapFree(GetProcessHeap(), 0, j);
    return 0;
}

void upd_check(HWND notify, const WCHAR *url, int manual)
{
    UpdJob *j;
    HANDLE h;
    if (!url || !is_https(url)) {
        if (manual) {                                    /* sin direccion valida: avisar de una vez */
            UpdRes *r = (UpdRes *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *r);
            if (r) { r->status = -2; r->manual = 1; if (!PostMessageW(notify, WM_APP_UPDATE, 0, (LPARAM)r)) HeapFree(GetProcessHeap(), 0, r); }
        }
        return;
    }
    j = (UpdJob *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *j);
    if (!j) return;
    lstrcpynW(j->url, url, 256); j->hwnd = notify; j->manual = manual;
    h = CreateThread(NULL, 0, UpdThread, j, 0, NULL);
    if (h) CloseHandle(h); else HeapFree(GetProcessHeap(), 0, j);
}
