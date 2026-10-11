/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e
 */

#include "jj.h"
#include <math.h>

/* ===================================================================== */
/*  Sonidos de activar / desactivar: tono predeterminado o un .wav propio, */
/*  con volumen ajustable (se aplica a las muestras antes de reproducir). */
/* ===================================================================== */
#define SND_RATE   44100
#define SND_MAXWAV (4 * 1024 * 1024)

static BYTE *g_snd;                                  /* sonido que se esta reproduciendo (debe seguir vivo) */

static void  w16(BYTE *p, unsigned v) { p[0] = (BYTE)(v & 255); p[1] = (BYTE)((v >> 8) & 255); }
static void  w32(BYTE *p, DWORD v)    { w16(p, v & 0xFFFF); w16(p + 2, (v >> 16) & 0xFFFF); }
static DWORD r16(const BYTE *p)       { return (DWORD)p[0] | ((DWORD)p[1] << 8); }
static DWORD r32(const BYTE *p)       { return r16(p) | (r16(p + 2) << 16); }

/* tono senoidal corto con entrada y salida suaves (sin "clics") */
static BYTE *make_tone(int hz, int ms, int vol)
{
    int n = SND_RATE / 1000 * ms, i, fade = SND_RATE / 200;
    DWORD sz = 44 + (DWORD)n * 2;
    BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!b) return NULL;
    CopyMemory(b, "RIFF", 4);  w32(b + 4, sz - 8);
    CopyMemory(b + 8, "WAVEfmt ", 8); w32(b + 16, 16);
    w16(b + 20, 1); w16(b + 22, 1); w32(b + 24, SND_RATE); w32(b + 28, SND_RATE * 2); w16(b + 32, 2); w16(b + 34, 16);
    CopyMemory(b + 36, "data", 4); w32(b + 40, (DWORD)n * 2);
    for (i = 0; i < n; i++) {
        double e = 1.0;
        int s;
        if (i < fade) e = (double)i / fade;
        if (n - i < fade) e = (double)(n - i) / fade;
        s = (int)(sin(6.283185307179586 * hz * i / SND_RATE) * 0.55 * (vol / 100.0) * e * 32767.0);
        w16(b + 44 + 2 * i, (unsigned)(s & 0xFFFF));
    }
    return b;
}

/* lee un .wav PCM de 8 o 16 bits y le aplica el volumen; NULL si no se puede */
static BYTE *load_wav(const WCHAR *path, int vol)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD sz, rd = 0, bits = 0, fmt = 0, dlen = 0, i;
    ULONGLONG pos;
    BYTE *b, *data = NULL;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(f, NULL);
    if (sz < 44 || sz > SND_MAXWAV) { CloseHandle(f); return NULL; }
    b = (BYTE *)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!b) { CloseHandle(f); return NULL; }
    if (!ReadFile(f, b, sz, &rd, NULL) || rd != sz) { CloseHandle(f); HeapFree(GetProcessHeap(), 0, b); return NULL; }
    CloseHandle(f);
    if (memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) goto bad;
    for (pos = 12; pos + 8 <= sz; ) {
        DWORD cl = r32(b + pos + 4);
        if (!memcmp(b + pos, "fmt ", 4) && cl >= 16 && pos + 24 <= sz) { fmt = r16(b + pos + 8); bits = r16(b + pos + 22); }
        else if (!memcmp(b + pos, "data", 4)) {
            data = b + pos + 8;
            dlen = (pos + 8 + (ULONGLONG)cl > sz) ? (DWORD)(sz - pos - 8) : cl;
        }
        pos += 8 + (ULONGLONG)cl + (cl & 1);
    }
    if (fmt != 1 || !data || !dlen || (bits != 16 && bits != 8)) goto bad;
    if (bits == 16) {
        for (i = 0; i + 1 < dlen; i += 2) {
            int s = (short)(WORD)r16(data + i);
            s = s * vol / 100;
            w16(data + i, (unsigned)(s & 0xFFFF));
        }
    } else {
        for (i = 0; i < dlen; i++) data[i] = (BYTE)((((int)data[i] - 128) * vol / 100) + 128);
    }
    return b;
bad:
    HeapFree(GetProcessHeap(), 0, b);
    return NULL;
}

void snd_path(const WCHAR *name, WCHAR *out, int n)
{
    wsprintfW(out, L"%s\\Sonidos\\%s", ini_dir(), name);
    out[n - 1] = 0;
}

int snd_scan(WCHAR names[][128], int max)
{
    WCHAR pat[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int n = 0;
    wsprintfW(pat, L"%s\\Sonidos", ini_dir());
    CreateDirectoryW(pat, NULL);
    lstrcatW(pat, L"\\*.wav");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < max) lstrcpynW(names[n++], fd.cFileName, 128);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

void snd_play(int on)
{
    const WCHAR *nm = on ? g_prefs.snd_on : g_prefs.snd_off;
    int vol = g_prefs.snd_vol < 0 ? 0 : (g_prefs.snd_vol > 100 ? 100 : g_prefs.snd_vol);
    BYTE *b = NULL;
    if (!lstrcmpW(nm, L"*none") || vol == 0) return;
    if (nm[0]) {
        WCHAR p[MAX_PATH];
        snd_path(nm, p, MAX_PATH);
        b = load_wav(p, vol);
    }
    if (!b) b = make_tone(on ? 1200 : 600, 80, vol);          /* predeterminado (o si el archivo no sirve) */
    if (!b) return;
    PlaySoundW(NULL, NULL, 0);                                 /* corta el anterior antes de soltar su memoria */
    if (g_snd) HeapFree(GetProcessHeap(), 0, g_snd);
    g_snd = b;
    PlaySoundW((LPCWSTR)g_snd, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
