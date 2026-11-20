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
 * The seven sound effects in sound/ (new, open, flag, error, deadend, won, lost) were made
   for this port. See "ORIGINAL GAME SOUNDS" below to use the original game's instead.
 * More boards: put the board's .txt and .bmp files into boards/ (see the three included).
 * log.txt in this folder shows what the game printed if something goes wrong.
 * The original's highscore sending is not part of this port.

ORIGINAL GAME SOUNDS
--------------------
The original Mines-Perfect has its own sound pack, offered as a separate download ("sound.zip").
It is not included here because the download does not state its licence. If you want those sounds:

  1. Get it from the project page: https://sourceforge.net/projects/mines-perfect/
     (files section; the author's page is www.czeppi.de). The Windows download "mines140.zip"
     is not needed, only the sound pack.
  2. Unzip it. It contains seven files: FLAG.WAV, LOST.WAV, ERROR.WAV, NEW.WAV, Won.wav,
     OPEN.WAV and Deadend.wav (inside a folder called sound).
  3. Copy those seven files into the sound folder of this port on the SD card
     (roms/ports/minesperfect/sound/) and let them REPLACE the files that are there
     (flag.wav, lost.wav, error.wav, new.wav, won.wav, open.wav, deadend.wav). Capital letters in
     the names do not matter on the card. If your computer asks whether to replace, say yes. If
     you copy them to another name, delete ours first, otherwise ours are the ones played.
  4. Start the game. Any single sound you leave out simply keeps the port's own.

You can also use any .wav files of your own the same way (8 or 16 bit, mono or stereo, any sample
rate). To turn all sounds off: START > Settings > Sound.
