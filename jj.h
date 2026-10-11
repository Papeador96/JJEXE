/*
 * JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌​​‌‌​‌​​​​‌​​‌‌​​​‌‌‌​‌​‌‌​​‌​​‌‌​‌​‌‌‌​‌​‌‌​​​⁠
 * Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 9a131d64d758
 */

#ifndef JJ_H
#define JJ_H

#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <commdlg.h>
#include "gdip.h"

#define APP_VER      L"v9"
#define APP_VERNUM   9                   /* numero que se compara con el del archivo de version */
#define MAXSTEPS     24
#define CFG_NAME_MAX 41
#define WM_APP_PICK  (WM_APP + 1)        /* el selector de ventana encontro una (wParam = HWND)  */
#define WM_APP_TRAY  (WM_APP + 2)        /* mensajes del icono de la bandeja                     */
#define WM_APP_UPDATE (WM_APP + 4)       /* resultado de la comprobacion de version (lParam = UpdRes*, se libera al recibirlo) */
#define WM_APP_HOTKEY (WM_APP + 3)       /* atajo de boton del mouse (wParam 1 = activar, 2 = siguiente config) */

/* recursos */
#define RES_ICON_APP   100
#define RES_ICON_ON    101
#define RES_ICON_OFF   102
#define RES_BMP_EMBLEM 201
#define RES_BMP_WORD   202

/* ------------------------------------------------------------------ datos */
enum { ST_KEY = 0, ST_CLICK = 1 };
typedef struct { int type, code, hold, after; } Step;     /* code: tecla (scan | ext<<8) o boton (0 izq, 1 der, 2 medio) */

typedef struct {
    int  mode;                 /* 0 = simple, 1 = personalizada                    */
    int  key;                  /* tecla de la secuencia simple (scan | ext<<8)      */
    int  repeat;               /* simple: volver a pulsar la tecla al final         */
    int  ms;                   /* simple: tiempo total 1..100                       */
    int  button;               /* boton que dispara: 0 izq, 1 der                   */
    int  release;              /* 1 = al soltar el clic, 0 = al presionar           */
    int  nsteps;
    Step steps[MAXSTEPS];
    int  tgt_on;               /* solo funcionar en una ventana                     */
    WCHAR tgt_exe[64];         /* programa de esa ventana (p. ej. javaw.exe)        */
    WCHAR tgt_title[96];       /* titulo al elegirla (informativo)                  */
    WCHAR tgt_filter[64];      /* "el titulo debe contener" (opcional)              */
} Macro;

typedef struct {               /* ajustes generales (no van dentro de las configuraciones con nombre) */
    int  hk_toggle;            /* atajo para activar/desactivar: tecla virtual | (Alt=1,Ctrl=2,Shift=4,Win=8) << 8; 0 = ninguno */
    int  hk_next;              /* atajo para la siguiente configuracion (mismo formato)  */
    int  theme;
    int  tray_min, sound, osd;
    int  stream, stream_main;  /* modo stream: aviso oculto de grabaciones y sin pitido / ocultar tambien la ventana */
    WCHAR anim[128];           /* "" = predeterminada, "*none" = ninguna, "*claude" etc. = integrada, o nombre de archivo */
    WCHAR snd_on[128], snd_off[128];   /* sonido al activar / desactivar: "" = tono predeterminado, "*none" = silencio, o archivo .wav */
    int  snd_vol;              /* volumen 0..100 */
    int  upd_on;               /* avisar de versiones nuevas al abrir */
    WCHAR upd_url[256];        /* direccion https del archivo de version */
} Prefs;

extern Macro g_macro;
extern Prefs g_prefs;
extern CRITICAL_SECTION g_cs;
extern int g_dpi;
extern HFONT g_font, g_fontB, g_fontS, g_fontSB, g_fontBig;
#define S(v) MulDiv((v), g_dpi, 96)

/* ------------------------------------------------------------------ motor (engine.c) */
enum { EV_KDOWN = 0, EV_KUP = 1, EV_MDOWN = 2, EV_MUP = 3 };
typedef struct { int t_us, kind, code; } Ev;
typedef struct { int n, total_us; Ev ev[2 * MAXSTEPS + 8]; } EvList;
typedef struct { HWND h; WCHAR exe[64]; WCHAR title[96]; } WinItem;

void eng_init(void);
void eng_shutdown(void);
int  eng_hook_install(HWND notify);
void eng_hook_remove(void);
void eng_set_enabled(int on);
int  eng_get_enabled(void);
void eng_target_changed(void);
void eng_build(const Macro *m, EvList *L);
int  win_info(HWND h, WinItem *out);
int  win_enum(WinItem *out, int max);
void key_name(int code, WCHAR *out, int n);
void hk_name(int hk, WCHAR *out, int n);          /* "Ctrl+Shift+K", "F8", "Botón lateral 1"... */
int  hk_vk(int hk);
int  hk_mods(int hk);
int  hk_is_mouse(int hk);                          /* botones medio / laterales del mouse */
void eng_set_mouse_hotkeys(int toggle, int next);
void timer_res_begin(void);
void timer_res_end(void);

/* ------------------------------------------------------------------ configuraciones (cfg.c) */
extern WCHAR g_ini[MAX_PATH];
void ini_init(void);
const WCHAR *ini_dir(void);
void ini_prepare(const WCHAR *path);
void macro_defaults(Macro *m);
void prefs_defaults(Prefs *p);
int  cfg_read(const WCHAR *ini, const WCHAR *sec, Macro *m, Prefs *p);
void cfg_write(const WCHAR *ini, const WCHAR *sec, const Macro *m, const Prefs *p, const WCHAR *last);
int  cfg_names(const WCHAR *ini, WCHAR names[][CFG_NAME_MAX], int max);
void cfg_delete(const WCHAR *ini, const WCHAR *name);
int  cfg_get_last(WCHAR *out, int n);

/* ------------------------------------------------------------------ sonidos (sound.c) */
void snd_play(int on);                            /* 1 = activar, 0 = desactivar; usa el sonido y el volumen elegidos */
int  snd_scan(WCHAR names[][128], int max);       /* archivos .wav de la carpeta Sonidos */
void snd_path(const WCHAR *name, WCHAR *out, int n);

/* ------------------------------------------------------------------ version nueva (update.c) */
typedef struct { int status, ver, manual; WCHAR url[256]; WCHAR note[128]; } UpdRes;   /* status: 1 hay nueva, 0 al dia, -1 error */
void upd_check(HWND notify, const WCHAR *url, int manual);

/* ------------------------------------------------------------------ animaciones (splash.c) */
void run_splash(HINSTANCE hInst);                 /* segun g_prefs.anim */
int  anim_scan(WCHAR names[][128], int max);      /* archivos de la carpeta Animaciones */
void anim_path(const WCHAR *name, WCHAR *out, int n);

/* ------------------------------------------------------------------ interfaz (ui.c) */
typedef struct {
    const WCHAR *name;
    COLORREF bg, card, text, dim, accent, accent2, border, btn, hot, input, good, bad, head1, head2;
} Theme;
extern const Theme g_themes[];
extern const int g_ntheme;

enum {
    ID_TOGGLE = 100, ID_TGLHINT,
    ID_TAB0 = 110, ID_TAB1, ID_TAB2, ID_TAB3, ID_TAB4, ID_TAB5,
    ID_CFG_FRAME = 120, ID_CFG_DROP, ID_CFG_SAVE, ID_CFG_DEL, ID_CFG_IMPORT, ID_CFG_EXPORT,
    ID_KEY, ID_MS, ID_REPEAT, ID_TL0,
    ID_MODE, ID_STEPLIST, ID_ST_ADDK, ID_ST_ADDC, ID_ST_DEL, ID_ST_UP, ID_ST_DOWN, ID_ST_COPY,
    ID_ST_LBL, ID_ST_L2, ID_ST_L3, ID_ST_HINT, ID_ST_KEY, ID_ST_B0, ID_ST_B1, ID_ST_B2, ID_ST_HOLD, ID_ST_AFTER, ID_TL1,
    ID_BTN_L, ID_BTN_R, ID_TRG_REL, ID_TRG_PRE, ID_HK_TOGGLE, ID_HK_TOGGLE_CLR, ID_HK_NEXT, ID_HK_NEXT_CLR,
    ID_TGT_ON, ID_TGT_L1, ID_TGT_L2, ID_TGT_PICK, ID_TGT_CLEAR, ID_TGT_LIST, ID_TGT_REFRESH, ID_TGT_FRAME,
    ID_TH0, ID_TH1, ID_TH2, ID_TH3, ID_TH4, ID_TH5,
    ID_ANIM_DROP, ID_ANIM_ADD, ID_ANIM_FOLDER, ID_ANIM_TEST,
    ID_TRAYMIN, ID_SOUND, ID_OSD, ID_STREAM, ID_STREAM_MAIN,
    ID_SND_ON, ID_SND_OFF, ID_SND_ON_TEST, ID_SND_OFF_TEST, ID_SND_VOL, ID_SND_ADD, ID_SND_FOLDER,
    ID_UPD_ON, ID_UPD_FRAME, ID_UPD_NOW, ID_UPD_OPEN, ID_UPD_STATUS,
    ID_OVERLAY                                   /* aviso: se abrio (1) o cerro (0) una lista desplegable */
};

void ui_init(HWND hwnd);
void ui_shutdown(void);
void ui_paint(HDC dc, int w, int h);
int  ui_mouse(HWND hwnd, UINT msg, int px, int py);
int  ui_key(HWND hwnd, WPARAM vk, LPARAM lp);
void ui_wheel(int delta, int px, int py);
void ui_set_page(int p);
int  ui_get_page(void);
void ui_set_theme(int t);
const Theme *ui_theme(void);
void ui_set_val(int id, int v);
int  ui_get_val(int id);
void ui_set_text(int id, const WCHAR *t);
void ui_set_vis(int id, int v);
void ui_set_enabled(int id, int e);
void ui_items_clear(int id);
void ui_items_add(int id, const WCHAR *t);
int  ui_items_count(int id);
const WCHAR *ui_items_get(int id, int i);
void ui_rect(int id, RECT *r);
void ui_capture_begin(int id);
int  ui_capturing(void);
int  ui_capture_id(void);
void ui_capture_end(int code);
void ui_invalidate(void);
int  ui_drop_open(void);
void ui_drop_close(void);
void ui_list_reveal(int id);
int  ui_is_hot(void);
int  ui_edit_focus(int id);                     /* 1 si el cuadro de texto asociado tiene el foco */

/* implementadas en main.c */
void on_ui(int id, int val, int final);
void on_ui_capture(int id, int code);
extern HWND g_hEditName, g_hEditFilter;

#endif
