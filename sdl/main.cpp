// main.cpp - SF3500 front end for Mines-Perfect: screen, gamepad cursor, menus.
// The game itself (core/ + gui/) is the original; this replaces its wxWidgets application file.
// Not part of the original Mines-Perfect. GPL-2.0-or-later.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>

#include "app.h"
#include "../gui/gamectrl.h"
#include "../core/api.h"

using namespace MinesPerfect;
using std::string;
using std::vector;

static const CellNr INVALID_CELL = -1;     // same value as the original's (gui/boardctrl.cpp)

namespace MinesPerfect { namespace Glob { extern RandInt rand_int; } }

Surf g_scr;
PadState g_pad;

static Options* g_opt = 0;
static GameCtrl* g_game = 0;
static bool g_sound = true;
static int g_zoom_pref = 0;             // 0 = fit the whole board, else 1..4
static CellNr g_cur = INVALID_CELL;
static bool g_a_down = false, g_b_down = false;
static Point g_prev_pos(-1, -1);
static int g_cam_x = 0, g_cam_y = 0;
static bool g_resized = true;
static const char* CFG = "minesperfect.cfg";

bool app_sound_on() { return g_sound; }
void app_show_time(int secs) { if (g_game) g_game->showTime(secs); }
void app_window_resized() { g_resized = true; }

//******************************************************************************
// settings kept by the port (the original's own settings stay in mineperf.ini)
//------------------------------------------------------------------------------
static void cfg_load()
{
    FILE* f = fopen(CFG, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f))
    {
        int v;
        if (sscanf(line, "sound=%d", &v) == 1) g_sound = v != 0;
        else if (sscanf(line, "zoom=%d", &v) == 1 && v >= 0 && v <= 5) g_zoom_pref = v;
    }
    fclose(f);
}

static void cfg_save()
{
    FILE* f = fopen(CFG, "w");
    if (!f) return;
    fprintf(f, "sound=%d\nzoom=%d\n", g_sound ? 1 : 0, g_zoom_pref);
    fclose(f);
}

void app_quit(int code)
{
    try { if (g_opt) g_opt->saveIni(); } catch (...) {}
    cfg_save();
    plat_quit(code);
}

//******************************************************************************
// pad
//------------------------------------------------------------------------------
void pad_poll()
{
    static unsigned prev = 0, rep_bits = 0, next_rep = 0;
    unsigned now = plat_buttons(), t = plat_ticks();
    unsigned newly = now & ~prev;
    g_pad.held = now;
    g_pad.up = prev & ~now;
    g_pad.down = newly;
    const unsigned dirs = B_LEFT | B_RIGHT | B_UP | B_DOWN | B_L1 | B_R1;
    if (newly & dirs) { rep_bits = newly & dirs; next_rep = t + 380; }
    else if ((now & rep_bits) && t >= next_rep) { g_pad.down |= (now & rep_bits); next_rep = t + 70; }
    else if (!(now & rep_bits)) rep_bits = 0;
    prev = now;
    if ((now & B_START) && (now & B_SELECT)) app_quit(0);
}

bool ui_frame()
{
    app_poll_timer();
    plat_present();
    pad_poll();
    return true;
}

//******************************************************************************
// scene: the original's window scaled onto the screen, with the cursor
//------------------------------------------------------------------------------
static BoardCtrl* board_ctrl() { return g_game->getBoardCtrl(); }

// zoom: 0 = auto (integer, or smooth when an integer would leave much unused), 1..4 = that integer, 5 = smooth fit
struct View { float s; bool smooth; int vwc, vhc, dw, dh; };

static View view_now()
{
    View v;
    float fit = std::min((float) SCR_W / std::max(1, g_win.w), (float) SCR_H / std::max(1, g_win.h));
    int pref = g_zoom_pref;
    if (pref == 0)
    {
        int zi = (int) fit;
        if (zi < 1) zi = 1;
        pref = (fit >= 1.0f && fit / zi > 1.25f && zi < 4) ? 5 : zi;
        if (pref > 4 && pref != 5) pref = 4;
    }
    if (pref == 5) { v.s = fit < 1.0f ? 1.0f : fit; v.smooth = v.s != (float) (int) v.s; }
    else { v.s = (float) pref; v.smooth = false; }
    if (v.s > 4.0f) { v.s = 4.0f; v.smooth = false; }
    v.vwc = std::min(g_win.w, (int) (SCR_W / v.s));
    v.vhc = std::min(g_win.h, (int) (SCR_H / v.s));
    v.dw = v.smooth ? (int) (v.vwc * v.s + 0.5f) : v.vwc * (int) v.s;
    v.dh = v.smooth ? (int) (v.vhc * v.s + 0.5f) : v.vhc * (int) v.s;
    if (v.dw > SCR_W) v.dw = SCR_W;
    if (v.dh > SCR_H) v.dh = SCR_H;
    return v;
}

static int zoom_int_now() { View v = view_now(); return std::max(1, std::min(4, (int) (v.s + 0.5f))); }

static Surf g_scene;                 // cache of the rendered scene (the pad menus draw over copies of it)
static bool g_scene_dirty = true;
static CellNr g_scene_cur = -2;
static int g_scene_zoom = -1, g_scene_camx = -1, g_scene_camy = -1;

static void render_scene()
{
    Surf& d = g_scene;
    g_fill(d, 0, 0, SCR_W, SCR_H, 0x006060);
    if (g_win.w <= 0) return;
    View v = view_now();

    Rect cr;
    bool have_cur = g_cur != INVALID_CELL && board_ctrl()->cellRect(g_cur, cr);
    if (have_cur)   // keep the cursor cell in view with some margin
    {
        int mx = std::min(cr.getWidth() * 2, v.vwc / 4), my = std::min(cr.getHeight() * 2, v.vhc / 4);
        if (cr.getLeft() - mx < g_cam_x) g_cam_x = cr.getLeft() - mx;
        if (cr.getLeft() + cr.getWidth() + mx > g_cam_x + v.vwc) g_cam_x = cr.getLeft() + cr.getWidth() + mx - v.vwc;
        if (cr.getTop() - my < g_cam_y) g_cam_y = cr.getTop() - my;
        if (cr.getTop() + cr.getHeight() + my > g_cam_y + v.vhc) g_cam_y = cr.getTop() + cr.getHeight() + my - v.vhc;
    }
    g_cam_x = std::max(0, std::min(g_cam_x, g_win.w - v.vwc));
    g_cam_y = std::max(0, std::min(g_cam_y, g_win.h - v.vhc));

    int ox = (SCR_W - v.dw) / 2, oy = (SCR_H - v.dh) / 2;
    if (v.smooth) g_scale_smooth(d, g_win, ox, oy, v.dw, v.dh, g_cam_x, g_cam_y, v.vwc, v.vhc);
    else g_scale_blit(d, g_win, (int) v.s, ox, oy, g_cam_x, g_cam_y, v.vwc, v.vhc);

    if (have_cur)
    {
        int x = ox + (int) ((cr.getLeft() - g_cam_x) * v.s), y = oy + (int) ((cr.getTop() - g_cam_y) * v.s);
        int w = (int) (cr.getWidth() * v.s + 0.5f), h = (int) (cr.getHeight() * v.s + 0.5f);
        for (int a = 0; a < 2; a++)
        {
            g_hline(d, x + a, x + w - 1 - a, y + a, 0xFFFF00);
            g_hline(d, x + a, x + w - 1 - a, y + h - 1 - a, 0xFFFF00);
            g_vline(d, x + a, y + a, y + h - 1 - a, 0xFFFF00);
            g_vline(d, x + w - 1 - a, y + a, y + h - 1 - a, 0xFFFF00);
        }
        g_hline(d, x - 1, x + w, y - 1, 0); g_hline(d, x - 1, x + w, y + h, 0);
        g_vline(d, x - 1, y - 1, y + h, 0); g_vline(d, x + w, y - 1, y + h, 0);
    }

    // footer hint when the board leaves room
    if (SCR_H - (oy + v.dh) >= 26)
        g_text_c(d, font_small(), SCR_W / 2, SCR_H - 22, 0xC0E0E0, "A open   B flag   X hint   Y new   L1 undo   R1 redo   START menu");
}

void app_draw_scene()
{
    if (g_scene.w == 0) g_scene.make(SCR_W, SCR_H);
    int zk = g_zoom_pref * 1000 + g_win.w + g_win.h * 7;
    if (g_win_dirty || g_cur != g_scene_cur || zk != g_scene_zoom || g_scene_dirty)
    {
        render_scene();
        g_win_dirty = false; g_scene_dirty = false; g_scene_cur = g_cur; g_scene_zoom = zk;
    }
    memcpy(g_scr.px, g_scene.px, (size_t) SCR_W * SCR_H * 2);
}

//******************************************************************************
// cursor and mouse emulation
//------------------------------------------------------------------------------
static void centre_cursor()
{
    BoardCtrl* bc = board_ctrl();
    int n = bc->numCells();
    if (n <= 0) { g_cur = INVALID_CELL; return; }
    Point mid(g_win.w / 2, 0);
    Rect r;
    // the board's own middle: average of all cell centres
    long sx = 0, sy = 0; int cnt = 0;
    for (CellNr k = 0; k < n; k++) { Point p; if (bc->cellCentre(k, p)) { sx += p.x; sy += p.y; cnt++; } }
    if (!cnt) { g_cur = INVALID_CELL; return; }
    Point c((int) (sx / cnt), (int) (sy / cnt));
    long best = -1; CellNr bk = 0;
    for (CellNr k = 0; k < n; k++)
    {
        Point p;
        if (!bc->cellCentre(k, p)) continue;
        long d = (long) (p.x - c.x) * (p.x - c.x) + (long) (p.y - c.y) * (p.y - c.y);
        if (best < 0 || d < best) { best = d; bk = k; }
    }
    (void) mid; (void) r;
    g_cur = bk;
}

static void move_cursor(int dx, int dy)
{
    BoardCtrl* bc = board_ctrl();
    Point c;
    if (g_cur == INVALID_CELL || !bc->cellCentre(g_cur, c)) { centre_cursor(); return; }
    int n = bc->numCells();
    long best = -1; CellNr bk = INVALID_CELL;
    for (CellNr k = 0; k < n; k++)
    {
        if (k == g_cur) continue;
        Point p;
        if (!bc->cellCentre(k, p)) continue;
        int vx = p.x - c.x, vy = p.y - c.y;
        int fwd = vx * dx + vy * dy;                 // distance along the direction
        int perp = abs(vx * dy) + abs(vy * dx);      // sideways offset
        if (fwd <= 0 || perp > 2 * fwd) continue;    // within about 63 degrees of the direction
        long score = (long) fwd * fwd + 3L * perp * perp;
        if (best < 0 || score < best) { best = score; bk = k; }
    }
    if (bk == INVALID_CELL) return;
    g_cur = bk;
}

static void send_mouse(MouseEvent::Type t)
{
    Point p;
    if (g_cur == INVALID_CELL || !board_ctrl()->cellCentre(g_cur, p)) return;
    MouseEvent ev;
    ev.m_type = t;
    ev.m_pos = p;
    ev.m_prev_pos = g_prev_pos;
    ev.m_left_is_down = g_a_down;
    ev.m_right_is_down = g_b_down;
    g_prev_pos = p;
    g_game->onMouseEvent(ev);
    g_game->show();
}

static void game_show() { g_game->show(); }

//******************************************************************************
// menus
//------------------------------------------------------------------------------
enum
{
    M_RESUME = 1, M_NEW, M_LEVEL, M_MODE, M_BOARD, M_TOOLS, M_OPEN, M_SAVE, M_BEST, M_SETTINGS, M_HELP, M_QUIT,
    L_BEG = 100, L_INT, L_EXP, L_SELF,
    D_ORIG = 110, D_HINT, D_IMMUNE, D_LUCKY, D_STARTUP, D_MURPHY,
    T_HINT = 120, T_ONE, T_ALL1, T_ALL2, T_ALL3, T_AUTO0, T_AUTO1, T_AUTO2, T_AUTO3, T_MAX1, T_MAX2, T_MAX3, T_MINES, T_UNDO, T_REDO, T_UNDOALL,
    S_SOUND = 140, S_ZOOM,
    H_PAGES = 150, H_ABOUT,
    B_FIRST = 200
};

static bool menu_level()
{
    Options* o = g_opt;
    vector<MItem> it;
    it.push_back(MItem("Beginner", L_BEG, o->getLevelNr() == BEGINNER));
    it.push_back(MItem("Intermediate", L_INT, o->getLevelNr() == INTERMEDIATE));
    it.push_back(MItem("Expert", L_EXP, o->getLevelNr() == EXPERT));
    it.push_back(MItem("Selfdefined...", L_SELF, o->getLevelNr() == USER_DEFINED));
    int id = ui_menu("Level", it, o->getLevelNr());
    if (id < 0) return false;
    if (id == L_SELF)
    {
        Level lvl = o->getLevel();
        lvl.nr = USER_DEFINED;
        if (!ui_selfdefined(lvl)) return false;
        g_game->changeLevel(lvl);
    }
    else g_game->changeLevel(id == L_BEG ? BEGINNER : id == L_INT ? INTERMEDIATE : EXPERT);
    return true;
}

static bool menu_mode()
{
    int sel = 0;
    for (;;)
    {
        Options* o = g_opt;
        vector<MItem> it;
        it.push_back(MItem("Original", D_ORIG, o->getModus() == ORIGINAL));
        it.push_back(MItem("Hint", D_HINT, o->getModus() == HINTS));
        it.push_back(MItem("Immune", D_IMMUNE, o->getModus() == IMMUNE));
        it.push_back(MItem("Lucky", D_LUCKY, o->getModus() == LUCKY, o->getMaxStage() == MAX_STAGE));
        it.push_back(MItem("Startup", D_STARTUP, o->getModus() == STARTUP));
        it.push_back(MItem("Murphy's Law", D_MURPHY, o->getMurphysLaw()));
        int id = ui_menu("Mode", it, sel);
        if (id < 0) return false;
        if (id == D_MURPHY) { g_game->setMurphysLaw(!o->getMurphysLaw()); sel = 5; game_show(); continue; }
        g_game->changeModus(id == D_ORIG ? ORIGINAL : id == D_HINT ? HINTS : id == D_IMMUNE ? IMMUNE : id == D_LUCKY ? LUCKY : STARTUP);
        return true;
    }
}

static bool menu_board()
{
    vector<MItem> it;
    for (int i = 0; i < g_opt->getNumBoards(); i++)
        it.push_back(MItem(g_opt->getBoardName(i), B_FIRST + i, i == g_opt->getBoardNr()));
    int id = ui_menu("Board", it, g_opt->getBoardNr());
    if (id < 0) return false;
    g_game->changeBoard(id - B_FIRST);
    return true;
}

static bool menu_tools()
{
    int sel = 0;
    for (;;)
    {
        Options* o = g_opt;
        vector<MItem> it;
        it.push_back(MItem("Hint", T_HINT));
        it.push_back(MItem("Solve One", T_ONE));
        it.push_back(MItem("Solve All, to stage 1", T_ALL1));
        it.push_back(MItem("Solve All, to stage 2", T_ALL2));
        it.push_back(MItem("Solve All, to stage 3", T_ALL3));
        it.push_back(MItem("Solve Auto: off", T_AUTO0, o->getAutoStage() == 0));
        it.push_back(MItem("Solve Auto: stage 1", T_AUTO1, o->getAutoStage() == 1));
        it.push_back(MItem("Solve Auto: stage 2", T_AUTO2, o->getAutoStage() == 2));
        it.push_back(MItem("Solve Auto: stage 3", T_AUTO3, o->getAutoStage() == 3));
        bool ms = o->getModus() != LUCKY;
        it.push_back(MItem("Max. stage 1", T_MAX1, o->getMaxStage() == 1, ms));
        it.push_back(MItem("Max. stage 2", T_MAX2, o->getMaxStage() == 2, ms));
        it.push_back(MItem("Max. stage 3", T_MAX3, o->getMaxStage() == 3, ms));
        it.push_back(MItem("Show Mines", T_MINES, o->getShowMines()));
        it.push_back(MItem("Undo", T_UNDO));
        it.push_back(MItem("Redo", T_REDO));
        it.push_back(MItem("Undo All", T_UNDOALL));
        int id = ui_menu("Help tools", it, sel);
        if (id < 0) return false;
        switch (id)
        {
        case T_HINT: g_game->giveHint(); return true;
        case T_ONE: g_game->solveOne(); return true;
        case T_ALL1: g_game->solveAll(1); return true;
        case T_ALL2: g_game->solveAll(2); return true;
        case T_ALL3: g_game->solveAll(3); return true;
        case T_AUTO0: case T_AUTO1: case T_AUTO2: case T_AUTO3: g_game->changeSolveAuto(id - T_AUTO0); break;
        case T_MAX1: case T_MAX2: case T_MAX3: g_game->changeMaxStage(id - T_MAX1 + 1); break;
        case T_MINES: g_game->setShowMines(!o->getShowMines()); break;
        case T_UNDO: g_game->undo(false); return true;
        case T_REDO: g_game->redo(); return true;
        case T_UNDOALL: g_game->undo(true); return true;
        }
        sel = id == T_MINES ? 12 : (id >= T_AUTO0 && id <= T_AUTO3) ? 5 + (id - T_AUTO0) : 9 + (id - T_MAX1);
        game_show();
    }
}

static void menu_open()
{
    vector<string> names;
    FindFiles(names, "logs/*.log");
    std::sort(names.begin(), names.end());
    int i = ui_pick_file("Open game", names);
    if (i >= 0) g_game->load(string("logs/") + names[i]);
}

static void menu_save()
{
    string name = "game";
    if (!ui_text_entry("Save game", "File name:", name, 16, 0)) return;
    mkdir("logs", 0755);
    string path = "logs/" + name + ".log";
    if (FileExist(path) && !ui_confirm("Save game", (name + ".log exists. Replace it?").c_str())) return;
    g_game->save(path);
}

static void menu_settings()
{
    int sel = 0;
    for (;;)
    {
        vector<MItem> it;
        it.push_back(MItem("Sound", S_SOUND, g_sound ? 1 : 0));
        char b[48];
        if (g_zoom_pref == 0) snprintf(b, sizeof b, "Zoom: auto"); else if (g_zoom_pref == 5) snprintf(b, sizeof b, "Zoom: fit (smooth)"); else snprintf(b, sizeof b, "Zoom: %dx", g_zoom_pref);
        it.push_back(MItem(b, S_ZOOM));
        int id = ui_menu("Settings", it, sel);
        if (id < 0) return;
        if (id == S_SOUND) g_sound = !g_sound, sel = 0;
        if (id == S_ZOOM) g_zoom_pref = (g_zoom_pref + 1) % 6, sel = 1;
        cfg_save();
    }
}

static void menu_help()
{
    vector<MItem> it;
    it.push_back(MItem("How to play", H_PAGES));
    it.push_back(MItem("About", H_ABOUT));
    int id = ui_menu("Help", it, 0);
    if (id == H_PAGES) ui_help_pages(0);
    else if (id == H_ABOUT) ui_about();
}

static void menu_main()
{
    int sel = 0;
    for (;;)
    {
        vector<MItem> it;
        it.push_back(MItem("Resume", M_RESUME));
        it.push_back(MItem("New Game", M_NEW));
        it.push_back(MItem("Level", M_LEVEL, -1, true, true));
        it.push_back(MItem("Mode", M_MODE, -1, true, true));
        it.push_back(MItem("Board", M_BOARD, -1, true, true));
        it.push_back(MItem("Help tools", M_TOOLS, -1, true, true));
        it.push_back(MItem("Open game", M_OPEN));
        it.push_back(MItem("Save game", M_SAVE));
        it.push_back(MItem("Best Times", M_BEST));
        it.push_back(MItem("Settings", M_SETTINGS, -1, true, true));
        it.push_back(MItem("Help", M_HELP, -1, true, true));
        it.push_back(MItem("Quit", M_QUIT));
        int id = ui_menu("Mines-Perfect", it, sel);
        if (id < 0 || id == M_RESUME) return;
        sel = id - 1;
        bool close = false;
        switch (id)
        {
        case M_NEW: g_game->newGame(); close = true; break;
        case M_LEVEL: close = menu_level(); break;
        case M_MODE: close = menu_mode(); break;
        case M_BOARD: close = menu_board(); break;
        case M_TOOLS: close = menu_tools(); break;
        case M_OPEN: menu_open(); close = true; break;
        case M_SAVE: menu_save(); break;
        case M_BEST: ui_best_times(g_opt); break;
        case M_SETTINGS: menu_settings(); break;
        case M_HELP: menu_help(); break;
        case M_QUIT: if (ui_confirm("Quit", "Quit Mines-Perfect?")) app_quit(0); break;
        }
        if (g_resized) { g_game->setDirty(); }
        game_show();
        if (close) return;
    }
}

//******************************************************************************
// main
//------------------------------------------------------------------------------
static void run()
{
    g_opt = new Options();
    g_opt->setWithSound(true);
    InitApi();
    if (const char* sd = getenv("MP_SEED")) Glob::rand_int.setStart((uint32) atoi(sd));   // test hook: reproducible boards
    g_game = new GameCtrl(g_opt);
    if (getenv("MP_LISTBOARDS")) for (int i = 0; i < g_opt->getNumBoards(); i++) fprintf(stderr, "board %d: %s\n", i, g_opt->getBoardName(i).c_str());
    // test hook: MP_START="board,level[,height,width,mines]" (level 0..3)
    if (const char* st = getenv("MP_START"))
    {
        int b = 0, l = 0, h = 0, w = 0, m = 0;
        int n = sscanf(st, "%d,%d,%d,%d,%d", &b, &l, &h, &w, &m);
        if (n >= 1) g_game->changeBoard(b);
        if (n >= 5) { Level lv = g_opt->getLevel(); lv.nr = USER_DEFINED; lv.height = h; lv.width = w; lv.num_mines = m; g_game->changeLevel(lv); }
        else if (n >= 2) g_game->changeLevel((LevelNr) l);
    }
    g_game->setDirty();
    game_show();
    centre_cursor();

    if (const char* ui = getenv("MP_UI"))     // test hook: open one dialog straight away
    {
        if (!strcmp(ui, "newrecord")) ui_new_record(g_opt, 12345, true);
        else if (!strcmp(ui, "best")) ui_best_times(g_opt);
        else if (!strcmp(ui, "selfdef")) { Level lv = g_opt->getLevel(); ui_selfdefined(lv); }
        else if (!strcmp(ui, "help")) ui_help_pages(atoi(getenv("MP_UIARG") ? getenv("MP_UIARG") : "0"));
        else if (!strcmp(ui, "about")) ui_about();
        else if (!strcmp(ui, "save")) menu_save();
        else if (!strcmp(ui, "open")) menu_open();
        else if (!strcmp(ui, "settings")) menu_settings();
        else if (!strcmp(ui, "tools")) menu_tools();
        else if (!strcmp(ui, "mode")) menu_mode();
        game_show();
    }
    if (getenv("MP_BOT"))     // test hook: open the middle cell, let the solver finish the game, report
    {
        g_a_down = true; send_mouse(MouseEvent::LEFT_DOWN); g_a_down = false; send_mouse(MouseEvent::LEFT_UP);
        int moves = 0, clicks = 1;
        unsigned rs = (unsigned) (getenv("MP_SEED") ? atoi(getenv("MP_SEED")) : 1) * 2654435761u + 1;
        while (!g_game->getBoard()->gameOver() && moves < 3000)
        {
            Board* bd = g_game->getBoard();
            int before = bd->getNumOpen() + bd->getNumFlagged();
            g_game->solveAll(3); g_game->show(); moves++;
            if (bd->gameOver()) break;
            if (bd->getNumOpen() + bd->getNumFlagged() == before)    // stuck: guess (Lucky mode makes the guess safe)
            {
                for (int tries = 0; tries < 10000; tries++)
                {
                    rs = rs * 1664525u + 1013904223u;
                    CellNr k = (CellNr) ((rs >> 8) % (unsigned) bd->getNumCells());
                    if (bd->cellIsOpen(k) || bd->cellIsFlagged(k)) continue;
                    g_cur = k; g_a_down = true; send_mouse(MouseEvent::LEFT_DOWN); g_a_down = false; send_mouse(MouseEvent::LEFT_UP);
                    clicks++;
                    break;
                }
            }
        }
        Board* b = g_game->getBoard();
        fprintf(stderr, "BOT result=%s rounds=%d clicks=%d open=%d flagged=%d cells=%d mines=%d\n", b->gameIsWon() ? "WON" : b->gameIsLost() ? "LOST" : "UNFINISHED",
                moves, clicks, b->getNumOpen(), b->getNumFlagged(), b->getNumCells(), b->getNumMines());
        app_quit(0);
    }

    unsigned frame = 0;
    for (;;)
    {
        try
        {
            if (g_resized)
            {
                g_resized = false;
                g_game->setDirty();
                game_show();
                if (g_cur == INVALID_CELL || g_cur >= board_ctrl()->numCells()) centre_cursor();
                g_cam_x = g_cam_y = 0;
            }
            unsigned d = g_pad.down;
            if (d & B_UP) move_cursor(0, -1);
            if (d & B_DOWN) move_cursor(0, 1);
            if (d & B_LEFT) move_cursor(-1, 0);
            if (d & B_RIGHT) move_cursor(1, 0);
            if ((d & (B_UP | B_DOWN | B_LEFT | B_RIGHT)) && (g_a_down || g_b_down)) send_mouse(MouseEvent::MOVE);

            if (g_pad.down & B_A) { g_a_down = true; send_mouse(MouseEvent::LEFT_DOWN); }
            if (g_pad.up & B_A) { g_a_down = false; send_mouse(MouseEvent::LEFT_UP); }
            if (g_pad.down & B_B) { g_b_down = true; send_mouse(MouseEvent::RIGHT_DOWN); }
            if (g_pad.up & B_B) { g_b_down = false; send_mouse(MouseEvent::RIGHT_UP); }

            if (g_pad.down & B_X) { g_game->giveHint(); game_show(); }
            if (g_pad.down & B_Y) { g_game->newGame(); game_show(); }
            if (g_pad.down & B_L1) { g_game->undo(false); game_show(); }
            if (g_pad.down & B_R1) { g_game->redo(); game_show(); }
            if (g_pad.down & B_L2) { g_zoom_pref = std::max(1, zoom_int_now() - 1); cfg_save(); }
            if (g_pad.down & B_R2) { g_zoom_pref = std::min(4, zoom_int_now() + 1); cfg_save(); }
            if (g_pad.down & B_START) { g_a_down = g_b_down = false; menu_main(); }

            app_poll_timer();
            app_draw_scene();
            plat_present();
            pad_poll();
            frame++;
        }
        catch (const Exception& e)
        {
            ui_message("Error", e.getText().c_str());
        }
    }
}

int main(int argc, char** argv)
{
    (void) argc; (void) argv;
    if (plat_init() != 0) return 1;
    plat_set_fps(30);
    g_scr.wrap(plat_fb(), SCR_W, SCR_H);
    cfg_load();
    pad_poll();
    try { run(); }
    catch (const Exception& e) { fprintf(stderr, "mines-perfect: %s\n", e.getText().c_str()); ui_message("Error", e.getText().c_str()); }
    return 0;
}
