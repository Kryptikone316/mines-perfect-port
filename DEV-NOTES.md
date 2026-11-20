# Mines-Perfect (SF3500) - dev notes

Port of Mines-Perfect 1.4.0 (Christian Czepluch, GPL-2.0-or-later, sourceforge.net/projects/mines-perfect). The original is wxWidgets
on Windows but cleanly layered: core/ (rules, solver, board geometry, logbook, options), gui/ (controls drawn through core/api.h),
wxwin/ (the platform). Port = core/ + gui/ kept (tiny patches, listed in NOTICE.md) + a new SDL2 front end in sdl/ implementing api.h.
Originals: ~/handhelds/Development/Datafrog/SF3500/ports/mines-perfect/Installer (mines140 = binary dist with extra boards/help, mines140_src, sound.zip).

## Build
    ./build_host.sh        # desktop, build-host/minesperfect (SDL2 window, keyboard z/x/c/v, arrows, Enter=START, Backspace=SELECT)
    ./build_sf3500.sh      # MIPS, build-sf3500/minesperfect.stripped.mipsel (GCC 6.3 toolchain in sf3000-work/buildroot, -std=gnu++11 -O2)
    ./package.sh           # package/MinesPerfect.sh + package/minesperfect/ ; staged copy in ports/mines-perfect/Port
    python3 tools/make_sounds.py; tools/bake_font.py (needs Pillow: ~/venvs/pydev/bin/python); tools/make_boxart.py <frame.bmp> package-src/boxart.png
Run from a folder holding boards/ and sound/ (work/run is a scratch copy).

## Layout
- core/compat.h (force-included): clock() -> wall ms (CLK_TCK 1000), itoa, stricmp, _VARIANT_=3 (the VC6/wx2.40 release). The globals
  (VERSION 140, MOD_VAL 0x2321, rand_int...) are defined in gui/gamectrl.cpp as in the original.
- sdl/api_sdl.cpp  api.h: bitmaps (XPM parsed, board BMPs read by hand, colour key = bottom-left pixel), WinSetSize/WinDrawBitmap/WinDrawBevel
  draw into the "window canvas" g_win (RGB565), sound (WAV any rate -> 48 kHz stereo, plat_snd_*), timer (polled), FindFiles, dialogs -> ui.cpp.
- sdl/main.cpp     screen: canvas scaled (auto = integer, or smooth bilinear when an integer leaves >25% unused; settings: auto/fit/1x-4x,
  L2/R2), camera follows the cursor on big boards, scene cached. Pad -> emulated mouse: cursor = a cell (d-pad picks the nearest cell in
  that direction by cell centres, works on hex/triangle/pentagon boards), A = left button (open on release), B = right (flag on press), A held +
  move = MOVE event. X hint, Y new game, L1 undo, R1 redo, START menu, SELECT+START quit. Menus: Level, Mode, Board, Help tools, Open/Save, Best Times, Settings, Help.
- sdl/ui.cpp       Win9x-style menus/dialogs, OSK (names: letters digits - _ , min 3 chars like the original), selfdefined board, best times, 7 help pages.
- sdl/plat_sdl.c + plat/  copied from Gweled (display/pad/audio glue; Prosty's OK, see memory glue-code-permission); env hooks renamed MP_*.
- gui/boardctrl.* got numCells/cellRect/cellCentre/cellAt; gui/gamectrl.h getBoardCtrl()/getState().
- core/board.cpp, options.cpp: `!in` for streams (C++11), trailing CR stripped when reading board files and the ini.

## Files the game writes (in its folder): mineperf.ini (level, mode, board, records), minesperfect.cfg (sound, zoom), logs/*.log (saved games), log.txt (launcher).

## Test hooks (host and QEMU)
MP_HEADLESS=1, MP_SHOTS=<dir> + MP_SHOT_FRAMES="f1,f2" (BMP), MP_JOY="frame:mask,..." (tools/joy.py), MP_MAXFRAMES, MP_SEED=<n> (fixed boards; host and
MIPS differ for the same seed: random_shuffle differs between libstdc++ versions - each is deterministic), MP_START="board,level[,h,w,mines]",
MP_LISTBOARDS=1, MP_UI=newrecord|best|selfdef|help|about|save|open|settings|tools|mode, MP_BOT=1 (opens the middle, solveAll(3), guesses when stuck, prints BOT result).
Bot result 2026-11-20: 36/36 games WON (square beginner/intermediate/expert, hexagon, triangle, 3d-grid selfdefined, seeds 1-3) on host x86-64 AND
QEMU mipsel, so the solver/Lucky mode work on the 32-bit target. Screenshots go to work/shots (not /tmp); shots.py makes a contact sheet and deletes BMPs.

## Decisions
- Sounds: the original project's sound.zip has no stated licence/provenance -> NOT shipped; own synthesised effects (tools/make_sounds.py). Same file names, so users can drop in theirs.
- Highscore sending/web, mouse, help.html (shown as 7 in-game pages), Homepage item: dropped.
- Box art: own render from a game screenshot (tools/make_boxart.py).

## Open
- Not device-tested: pad feel, cursor visibility, zoom choice (auto smooth vs integer), sound volumes (plat_snd_play gain 1.0, peak 0.6), time to solve on the 74Kc (solver time on Expert in Hint/Lucky mode).
- Licence of the extra boards (Parquet, Pentagon, Triangle14 from the author's binary dist): shipped as data under the author's GPL; SF project says GPL.
- No repo/PR yet.
