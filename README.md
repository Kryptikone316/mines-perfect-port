# Mines-Perfect for the Data Frog SF3500

A port of **Mines-Perfect 1.4.0** by Christian Czepluch (<http://www.czeppi.de>, <https://sourceforge.net/projects/mines-perfect/>)
to the Data Frog SF3500 handheld (MIPS32r2, TreeFrogUI). Mines-Perfect is a Minesweeper clone with an unusually clever solver:
it grades every position into three stages of difficulty, can protect you from forced guesses, and plays on square, hexagon,
triangle, 3D-grid and custom boards.

The original is a Windows program on wxWidgets, but its source is cleanly layered, so this port keeps the original game code and
replaces only the platform layer:

| folder | what |
|---|---|
| `core/` | the original rules, solver, board geometry, logbook and options (small compatibility patches, see `NOTICE`) |
| `gui/`  | the original controls (head with LCDs and smiley, board), drawn through `core/api.h` (four geometry accessors added for the gamepad cursor) |
| `sdl/`  | new: SDL2 front end implementing `api.h` (bitmaps, drawing, sound, timer), the gamepad "mouse" cursor, menus and dialogs, a tiny software renderer |
| `plat/`, `sdl/plat_sdl.c` | display, pad and audio glue for the SF3500 (also used by the other ports of this collection) |
| `tools/` | font, sound and background baking scripts, test helpers |

## Build
```
./build_host.sh      # desktop SDL2 window -> build-host/minesperfect   (needs g++ and SDL2)
./build_sf3500.sh    # MIPS cross build (Codescape mips-mti-linux-gnu GCC 6.3 toolchain, see script) -> build-sf3500/
./package.sh         # device package: MinesPerfect.sh + minesperfect/
```
Run from a folder that has `boards/` and `sound/` (copy them from a release package). Desktop keys: arrows, `z` A, `x` B, `c` X, `v` Y, `a`/`s` L1/R1, `q`/`w` L2/R2, Enter START, Backspace SELECT.

## Controls on the handheld
D-pad moves the cell cursor, A opens, B flags, X hint (solver on), Y new game, L1/R1 undo/redo, L2/R2 zoom, START menu, SELECT+START quit.
START > Game Configuration switches the solver on or off; START > Settings has background, zoom and sound. See `package-src/README.txt`.

## Tests
`DEV-NOTES.md` lists the test hooks (`MP_*` environment variables). `MP_BOT=1` lets the solver play whole games: 36 of 36 games were won on x86-64 and under QEMU mipsel.

## Licence
GPL-2.0-or-later, like the original (`COPYING`). The seven sound effects are synthesised for the port. Text is set in DejaVu Sans.
The optional **art pack** (six fan-art background pictures) is *not* part of this repository and is not GPL: it is a separate download with its own licence
(free of charge, for use with this port only, no commercial use).

Another, independent Linux/wxWidgets port of the same game exists at <https://github.com/m42a/mines-perfect>; no code from it is used here.
