/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌​‌​​​‌‌​‌​​​​​‌​‌‌​‌‌​​‌‌‌‌‌​‌‌​‌​‌‌‌‌‌​​​‌‌‌​‌⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo a3416cfb5f1d
 */

#include "jj.h"

/* marca de autoria: no afecta el funcionamiento */
__attribute__((used)) static const char jj_wm_cfg[] = "wm:a3416cfb5f1d";

WCHAR g_ini[MAX_PATH];
static WCHAR g_dir[MAX_PATH];

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int has_prefix(const WCHAR *s, const WCHAR *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

/* Crea el archivo en UTF-16 (con BOM) si no existe: asi los nombres con acentos o ñ se guardan bien. */
void ini_prepare(const WCHAR *path)
{
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            static const unsigned char bom[2] = { 0xFF, 0xFE };
            DWORD w;
            WriteFile(h, bom, 2, &w, NULL);
            CloseHandle(h);
        }
    }
}

void ini_init(void)
{
    WCHAR a[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", g_dir, MAX_PATH - 40);
    if (n > 0 && n < MAX_PATH - 40) {
        lstrcatW(g_dir, L"\\JJEXE");
        CreateDirectoryW(g_dir, NULL);
    } else {                                           /* sin APPDATA: carpeta del programa */
        n = GetModuleFileNameW(NULL, g_dir, MAX_PATH - 40);
        while (n > 0 && g_dir[n - 1] != L'\\') n--;
        if (n > 0) g_dir[n - 1] = 0; else g_dir[0] = 0;
    }
    if (g_dir[0]) wsprintfW(g_ini, L"%s\\JJEXE.ini", g_dir);
    else          lstrcpyW(g_ini, L"JJEXE.ini");
    ini_prepare(g_ini);
    wsprintfW(a, L"%s\\Animaciones", g_dir);
    CreateDirectoryW(a, NULL);
}

const WCHAR *ini_dir(void) { return g_dir; }

void macro_defaults(Macro *m)
{
    ZeroMemory(m, sizeof *m);
    m->mode = 0; m->key = 0x03; m->repeat = 1; m->ms = 10; m->button = 0; m->release = 1;
}

void prefs_defaults(Prefs *p)
{
    ZeroMemory(p, sizeof *p);
    p->hk_toggle = VK_F8; p->hk_next = 0; p->theme = 0; p->tray_min = 0; p->sound = 1; p->osd = 1; p->stream = 0; p->stream_main = 1;
    p->snd_vol = 70; p->upd_on = 1;
}

static int gi(const WCHAR *ini, const WCHAR *sec, const WCHAR *key, int def)
{
    return (int)GetPrivateProfileIntW(sec, key, (UINT)def, ini);
}

/* "K,codigo,mantener,esperar" o "C,boton,mantener,esperar" */
static int parse_step(const WCHAR *s, Step *st)
{
    int v[3] = { 0, 0, 0 }, i, type;
    if (*s == L'K') type = ST_KEY;
    else if (*s == L'C') type = ST_CLICK;
    else return 0;
    s++;
    for (i = 0; i < 3; i++) {
        int neg = 0, val = 0, any = 0;
        if (*s != L',') return 0;
        s++;
        if (*s == L'-') { neg = 1; s++; }
        while (*s >= L'0' && *s <= L'9') {
            val = val * 10 + (*s - L'0');
            if (val > 100000) val = 100000;
            s++; any = 1;
        }
        if (!any) return 0;
        v[i] = neg ? -val : val;
    }
    st->type  = type;
    st->code  = (type == ST_KEY) ? (v[0] & 0x1FF) : clampi(v[0], 0, 2);
    st->hold  = clampi(v[1], 0, 1000);
    st->after = clampi(v[2], 0, 1000);
    if (type == ST_KEY && !st->code) return 0;
    return 1;
}

/* Devuelve 1 si la seccion existe. Lo que el archivo no trae se queda como estaba en m / p. */
int cfg_read(const WCHAR *ini, const WCHAR *sec, Macro *m, Prefs *p)
{
    WCHAR probe[4], b[128], k[16];
    int i, v, n;
    if (GetPrivateProfileSectionW(sec, probe, 4, ini) == 0) return 0;

    m->mode    = clampi(gi(ini, sec, L"Modo", 0), 0, 1);
    v = gi(ini, sec, L"CodigoTecla", -1);
    if (v < 0) v = 0x02 + clampi(gi(ini, sec, L"Tecla", 1), 0, 9);      /* archivos de versiones anteriores */
    m->key     = v & 0x1FF;
    if (!m->key) m->key = 0x03;
    m->repeat  = clampi(gi(ini, sec, L"Repetir", 1), 0, 1);
    m->ms      = clampi(gi(ini, sec, L"Tiempo", 10), 1, 100);
    m->button  = clampi(gi(ini, sec, L"Boton", 0), 0, 1);
    m->release = clampi(gi(ini, sec, L"Disparo", 1), 0, 1);

    n = clampi(gi(ini, sec, L"Pasos", 0), 0, MAXSTEPS);
    m->nsteps = 0;
    for (i = 0; i < n; i++) {
        wsprintfW(k, L"Paso%d", i + 1);
        GetPrivateProfileStringW(sec, k, L"", b, 128, ini);
        if (parse_step(b, &m->steps[m->nsteps])) m->nsteps++;
    }

    v = gi(ini, sec, L"VentanaOn", -1);
    if (v >= 0) {
        m->tgt_on = (v != 0);
        GetPrivateProfileStringW(sec, L"VentanaExe",    L"", m->tgt_exe,    64, ini);
        GetPrivateProfileStringW(sec, L"VentanaTitulo", L"", m->tgt_title,  96, ini);
        GetPrivateProfileStringW(sec, L"VentanaFiltro", L"", m->tgt_filter, 64, ini);
    }

    if (p) {
        v = gi(ini, sec, L"AtajoTecla", -1);
        if (v >= 0) p->hk_toggle = v & 0xFFF;
        else { v = gi(ini, sec, L"Atajo", -1); if (v >= 0 && v <= 11) p->hk_toggle = VK_F1 + v; }      /* versiones anteriores: F1..F12 */
        v = gi(ini, sec, L"AtajoSigTecla", -1);
        if (v >= 0) p->hk_next = v & 0xFFF;
        else { v = gi(ini, sec, L"AtajoSig", -2); if (v == -1) p->hk_next = 0; else if (v >= 0 && v <= 11) p->hk_next = VK_F1 + v; }
        v = gi(ini, sec, L"Tema", -1);      if (v >= 0) p->theme = v;
        v = gi(ini, sec, L"Bandeja", -1);   if (v >= 0) p->tray_min = (v != 0);
        v = gi(ini, sec, L"Sonido", -1);    if (v >= 0) p->sound = (v != 0);
        v = gi(ini, sec, L"Aviso", -1);     if (v >= 0) p->osd = (v != 0);
        v = gi(ini, sec, L"Stream", -1);        if (v >= 0) p->stream = (v != 0);
        v = gi(ini, sec, L"StreamVentana", -1); if (v >= 0) p->stream_main = (v != 0);
        v = gi(ini, sec, L"SonidoVolumen", -1); if (v >= 0) p->snd_vol = v > 100 ? 100 : v;
        v = gi(ini, sec, L"AvisoVersion", -1);  if (v >= 0) p->upd_on = (v != 0);
        GetPrivateProfileStringW(sec, L"SonidoActivar", L"?", b, 128, ini);
        if (lstrcmpW(b, L"?") != 0) lstrcpynW(p->snd_on, b, 128);
        GetPrivateProfileStringW(sec, L"SonidoDesactivar", L"?", b, 128, ini);
        if (lstrcmpW(b, L"?") != 0) lstrcpynW(p->snd_off, b, 128);
        {
            WCHAR u[256];
            GetPrivateProfileStringW(sec, L"URLVersion", L"?", u, 256, ini);
            if (lstrcmpW(u, L"?") != 0) lstrcpynW(p->upd_url, u, 256);
        }
        GetPrivateProfileStringW(sec, L"Animacion", L"?", b, 128, ini);
        if (lstrcmpW(b, L"?") != 0) lstrcpynW(p->anim, b, 128);
    }
    return 1;
}

void cfg_write(const WCHAR *ini, const WCHAR *sec, const Macro *m, const Prefs *p, const WCHAR *last)
{
    static WCHAR buf[8192];
    WCHAR *q = buf;
    int i;
#define ADD1(f, a)            do { q += wsprintfW(q, f, a); q++; } while (0)
    ADD1(L"Modo=%d", m->mode);
    ADD1(L"CodigoTecla=%d", m->key);
    ADD1(L"Repetir=%d", m->repeat);
    ADD1(L"Tiempo=%d", m->ms);
    ADD1(L"Boton=%d", m->button);
    ADD1(L"Disparo=%d", m->release);
    ADD1(L"Pasos=%d", m->nsteps);
    for (i = 0; i < m->nsteps; i++) {
        q += wsprintfW(q, L"Paso%d=%s,%d,%d,%d", i + 1, m->steps[i].type == ST_KEY ? L"K" : L"C",
                       m->steps[i].code, m->steps[i].hold, m->steps[i].after);
        q++;
    }
    ADD1(L"VentanaOn=%d", m->tgt_on);
    ADD1(L"VentanaExe=%s", m->tgt_exe);
    ADD1(L"VentanaTitulo=%s", m->tgt_title);
    ADD1(L"VentanaFiltro=%s", m->tgt_filter);
    if (p) {
        ADD1(L"AtajoTecla=%d", p->hk_toggle);
        ADD1(L"AtajoSigTecla=%d", p->hk_next);
        ADD1(L"Tema=%d", p->theme);
        ADD1(L"Bandeja=%d", p->tray_min);
        ADD1(L"Sonido=%d", p->sound);
        ADD1(L"Aviso=%d", p->osd);
        ADD1(L"Stream=%d", p->stream);
        ADD1(L"StreamVentana=%d", p->stream_main);
        ADD1(L"Animacion=%s", p->anim);
        ADD1(L"SonidoActivar=%s", p->snd_on);
        ADD1(L"SonidoDesactivar=%s", p->snd_off);
        ADD1(L"SonidoVolumen=%d", p->snd_vol);
        ADD1(L"AvisoVersion=%d", p->upd_on);
        ADD1(L"URLVersion=%s", p->upd_url);
    }
    if (last && last[0]) ADD1(L"Ultima=%s", last);
#undef ADD1
    *q = 0;                                            /* doble NUL al final */
    WritePrivateProfileSectionW(sec, buf, ini);
}

int cfg_names(const WCHAR *ini, WCHAR names[][CFG_NAME_MAX], int max)
{
    static WCHAR buf[32768];
    WCHAR *p = buf;
    int n = 0;
    buf[0] = 0;
    GetPrivateProfileSectionNamesW(buf, 32768, ini);
    while (*p && n < max) {
        if (has_prefix(p, L"Config:") && p[7] && lstrlenW(p + 7) < CFG_NAME_MAX)
            lstrcpyW(names[n++], p + 7);
        p += lstrlenW(p) + 1;
    }
    return n;
}

void cfg_delete(const WCHAR *ini, const WCHAR *name)
{
    WCHAR sec[CFG_NAME_MAX + 8];
    wsprintfW(sec, L"Config:%s", name);
    WritePrivateProfileStringW(sec, NULL, NULL, ini);
}

int cfg_get_last(WCHAR *out, int n)
{
    GetPrivateProfileStringW(L"Actual", L"Ultima", L"", out, n, g_ini);
    return out[0] != 0;
}
