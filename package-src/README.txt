MINES-PERFECT  -  for the Datafrog SF3500
=========================================

A Minesweeper clone by Christian Czepluch (czeppi.de, version 1.4.0, GPL) with a very clever
solver: it classifies every position into three stages of difficulty, shows what it finds
with the smiley, and can protect you from dead ends (guessing). Besides the square board it
plays on hexagons, triangles and a 3D grid, and it reads extra boards (Parquet, Pentagon,
Triangle14 are included). This is the original game code with a new SDL2 front end for the
handheld.

CONTROLS
--------
  D-pad        move the yellow cursor from cell to cell (hold to repeat)
  A            open the cell. On an opened cell: quick open (see below)
  B            flag the cell. On an opened cell: quick flag
  X            hint (shows cells that can be used / opened / flagged)
  Y            new game
  L1 / R1      undo / redo
  L2 / R2      zoom out / in (big boards scroll with the cursor)
  START        menu
  SELECT + START   leave the game

Quick opening: on an opened number whose mines are all flagged, A opens all the covered
neighbours. Quick flagging: on a number whose covered neighbours must all be mines, B flags
them. Holding A while moving shows what a click would touch, like the mouse version.

MENU (START)
------------
  Resume, New Game
  Level      Beginner, Intermediate, Expert, Selfdefined (rows, columns, mines)
  Mode       Original (no help), Hint, Immune, Lucky, Startup, and Murphy's Law
  Board      Square, Hexagon, Triangle, 3d-Grid, Parquet, Pentagon, Triangle14
  Help tools Hint, Solve One, Solve All (to stage 1/2/3), Solve Auto, Max. stage,
             Show Mines, Undo, Redo, Undo All. Using help switches the clock off,
             no record is made.
  Open game / Save game      replays/saves the whole game as logs/<name>.log
  Best Times  the three records of every board (X deletes one)
  Settings   Sound on/off, Zoom (auto, fit smooth, 1x..4x)
  Help       how it works (7 pages), About
  Quit

NOTES
-----
 * Your records, the level, mode and board are kept in mineperf.ini in this folder; sound and
   zoom in minesperfect.cfg; saved games in logs/. Delete mineperf.ini to start fresh.
 * The sound effects (sound/*.wav: new, open, flag, error, deadend, won, lost) can be replaced by
   your own .wav files with the same names.
 * More boards: put the board's .txt and .bmp files into boards/ (see the three included).
 * log.txt in this folder shows what the game printed if something goes wrong.
 * The original's highscore sending is not part of this port.
