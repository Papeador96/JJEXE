#!/bin/bash
# JJEXE · (c) 2026 JamesR and Tom · Todos los derechos reservados / All rights reserved.⁠‌​​​‌‌‌‌‌​​​​‌‌‌​​‌​‌​‌​​‌‌‌​‌‌‌‌​‌‌​​​​​‌‌​‌‌‌‌⁠
# Obra original de JamesR and Tom. ID de autoria JJX-6ee1-7d9c-e042-1299-098e · archivo 8f872a77b06f
# Compila JJEXE (MinGW-w64)
cd "$(dirname "$0")"
x86_64-w64-mingw32-windres app.rc -O coff -o app_res.o || exit 1
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -s -static -mwindows main.c engine.c ui.c cfg.c splash.c gdip.c origin.c sound.c update.c app_res.o \
    -o "${1:-JJEXE_v9.exe}" -lgdiplus -lwininet -lmsimg32 -lwinmm -lcomdlg32 -lshell32 -lgdi32 -luser32 -lkernel32
