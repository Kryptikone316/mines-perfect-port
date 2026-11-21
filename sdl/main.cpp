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
static bool g_solver = true;            // solver assistance on (modes, hint, tools); off = classic Minesweeper
static int g_saved_mode = LUCKY;        // what to restore when it is switched on again
static bool g_saved_murphy = true;
static Surf g_bg;                       // background picture (640x480), if one is chosen
static string g_bg_name;                // file name in backgrounds/, empty = classic plain colour
static bool g_bg_on = false;            // a picture is loaded in g_bg
static uint32_t g_bg_rgb = 0x006060;    // flat colour when no picture
static bool g_bg_shadow = false;        // anything but Classic gets a drop shadow under the window
static bool g_scene_dirty = true;
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
        else if (!strncmp(line, "background=", 11)) { g_bg_name = line + 11; while (!g_bg_name.empty() && (g_bg_name[g_bg_name.size() - 1] == '\n' || g_bg_name[g_bg_name.size() - 1] == '\r')) g_bg_name.erase(g_bg_name.size() - 1); }
        else if (sscanf(line, "solver=%d", &v) == 1) g_solver = v != 0;
        else if (sscanf(line, "savedmode=%d", &v) == 1 && v >= 0 && v <= 4) g_saved_mode = v;
        else if (sscanf(line, "savedmurphy=%d", &v) == 1) g_saved_murphy = v != 0;
    }
    fclose(f);
}

static void cfg_save()
{
    FILE* f = fopen(CFG, "w");
    if (!f) return;
    fprintf(f, "sound=%d\nzoom=%d\nsolver=%d\nsavedmode=%d\nsavedmurphy=%d\nbackground=%s\n", g_sound ? 1 : 0, g_zoom_pref, g_solver ? 1 : 0, g_saved_mode, g_saved_murphy ? 1 : 0, g_bg_name.c_str());
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
// backgrounds: backgrounds/*.bg (our 640x480 RGB565 files) and *.bmp (your own pictures, any size, cropped to 4:3)
//------------------------------------------------------------------------------
static bool ends_with_ci(const string& s, const char* suf)
{
    size_t n = strlen(suf);
    return s.size() >= n && !strcasecmp(s.c_str() + s.size() - n, suf);
}

static vector<string> bg_files()
{
    vector<string> v;
    FindFiles(v, "backgrounds/*");
    vector<string> r;
    for (size_t i = 0; i < v.size(); i++) if (ends_with_ci(v[i], ".bg") || ends_with_ci(v[i], ".bmp")) r.push_back(v[i]);
    std::sort(r.begin(), r.end());
    return r;
}

static string bg_title(const string& file)     // "03-Sunset_Valley.bg" -> "Sunset Valley"
{
    string t = file.substr(0, file.rfind('.'));
    if (t.size() > 3 && isdigit((unsigned char) t[0]) && isdigit((unsigned char) t[1]) && t[2] == '-') t = t.substr(3);
    for (size_t i = 0; i < t.size(); i++) if (t[i] == '_') t[i] = ' ';
    return t;
}

static bool bg_load(const string& file)         // false: unreadable, the caller keeps the classic colour
{
    string path = "backgrounds/" + file;
    Surf src;
    if (ends_with_ci(file, ".bg"))
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        unsigned char hd[8];
        if (fread(hd, 1, 8, f) != 8 || memcmp(hd, "MPBG", 4)) { fclose(f); return false; }
        int w = hd[4] | (hd[5] << 8), h = hd[6] | (hd[7] << 8);
        if (w < 16 || h < 16 || w > 4096 || h > 4096) { fclose(f); return false; }
        src.make(w, h);
        bool ok = fread(src.px, 2, (size_t) w * h, f) == (size_t) w * h;     // files are little-endian, so is the device
        fclose(f);
        if (!ok) return false;
    }
    else
    {
        Argb a;
        if (!app_load_bmp(path, a)) return false;
        src.make(a.w, a.h);
        for (size_t i = 0; i < a.px.size(); i++) src.px[i] = rgb565(a.px[i]);
    }
    if (g_bg.w != SCR_W) g_bg.make(SCR_W, SCR_H);
    if (src.w == SCR_W && src.h == SCR_H) { memcpy(g_bg.px, src.px, (size_t) SCR_W * SCR_H * 2); return true; }
    int cx = 0, cy = 0, cw = src.w, ch = src.h;   // cover: crop the middle 4:3 part, scale to the screen
    if (src.w * 3 > src.h * 4) { cw = src.h * 4 / 3; cx = (src.w - cw) / 2; } else { ch = src.w * 3 / 4; cy = (src.h - ch) / 2; }
    g_scale_smooth(g_bg, src, 0, 0, SCR_W, SCR_H, cx, cy, cw, ch);
    return true;
}

// Stock backgrounds: flat matte neutral colours built into the game (no files). "Classic" is the original plain teal.
struct StockBg { const char* name; uint32_t rgb; };
static const StockBg g_stock[] =
{
    { "Classic",   0x006060 },
    { "Charcoal",  0x2E3033 },
    { "Slate",     0x4A5560 },
    { "Dusty Blue", 0x56697F },
    { "Sage",      0x667A64 },
    { "Stone",     0x77746E },
    { "Sand",      0x9C9279 },
};
static const int NSTOCK = (int) (sizeof g_stock / sizeof *g_stock);

// name kept in minesperfect.cfg: "" = Classic, "@Slate" = a stock colour, otherwise a file in backgrounds/ (optional art pack or your own)
static void bg_apply(string file)      // by value: callers pass g_bg_name itself
{
    g_bg_on = false; g_bg_shadow = false; g_bg_rgb = g_stock[0].rgb;
    g_bg_name.clear();
    if (!file.empty() && file[0] == '@')
    {
        for (int i = 1; i < NSTOCK; i++)
            if (file == string("@") + g_stock[i].name) { g_bg_rgb = g_stock[i].rgb; g_bg_shadow = true; g_bg_name = file; }
    }
    else if (!file.empty() && bg_load(file)) { g_bg_on = true; g_bg_shadow = true; g_bg_name = file; }
    g_scene_dirty = true;
}

static string bg_title_of(const string& name)
{
    if (name.empty()) return "Classic";
    if (name[0] == '@') return name.substr(1);
    return bg_title(name);
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
static CellNr g_scene_cur = -2;
static int g_scene_zoom = -1, g_scene_camx = -1, g_scene_camy = -1;

static void render_scene()
{
    Surf& d = g_scene;
    if (g_bg_on) memcpy(d.px, g_bg.px, (size_t) SCR_W * SCR_H * 2); else g_fill(d, 0, 0, SCR_W, SCR_H, g_bg_rgb);
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
    if (g_bg_shadow) g_shade(d, ox + 7, oy + 7, v.dw, v.dh);      // drop shadow so the window lifts off the picture
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
    {
        const char* hint = g_solver ? "A open   B flag   X hint   Y new   L1 undo   R1 redo   START menu" : "A open   B flag   Y new   L1 undo   R1 redo   START menu";
        if (g_bg_on) g_shade(d, 0, SCR_H - 28, SCR_W, 28);      // calm strip under the hint on busy pictures
        if (g_bg_shadow) g_text_c(d, font_small(), SCR_W / 2 + 1, SCR_H - 21, 0x000000, hint);
        g_text_c(d, font_small(), SCR_W / 2, SCR_H - 22, 0xE8F4F4, hint);
    }
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
    M_RESUME = 1, M_NEW, M_RESTART, M_LEVEL, M_BOARD, M_BEST, M_CONFIG, M_SETTINGS, M_HELP, M_QUIT,
    L_BEG = 100, L_INT, L_EXP, L_SELF,
    D_ORIG = 110, D_HINT, D_IMMUNE, D_LUCKY, D_STARTUP, D_MURPHY,
    T_HINT = 120, T_ONE, T_ALL1, T_ALL2, T_ALL3, T_AUTO0, T_AUTO1, T_AUTO2, T_AUTO3, T_MAX1, T_MAX2, T_MAX3, T_MINES,
    C_SOLVER = 140, C_MODE, C_TOOLS, C_SAVE, C_OPEN, C_SOUND, C_ZOOM, C_BG,
    H_PAGES = 150, H_SOLVER, H_ABOUT,
    B_FIRST = 200
};

static const char* mode_name(Modus m)
{
    return m == ORIGINAL ? "Original" : m == HINTS ? "Hint" : m == IMMUNE ? "Immune" : m == LUCKY ? "Lucky" : "Startup";
}

// "Solver: off" = classic Minesweeper: mode Original, no tools, no Murphy's Law. Turning it on again restores what you had.
static void solver_off_state()
{
    g_game->changeSolveAuto(0);
    g_game->changeMaxStage(MAX_STAGE);
    if (g_opt->getShowMines()) g_game->setShowMines(false);
    g_game->setMurphysLaw(false);
    g_game->changeModus(ORIGINAL);
}

static void solver_set(bool on)
{
    if (on == g_solver) return;
    if (!on)
    {
        g_saved_mode = (int) g_opt->getModus();
        g_saved_murphy = g_opt->getMurphysLaw();
        g_solver = false;
        solver_off_state();
    }
    else
    {
        g_solver = true;
        g_game->changeModus((Modus) g_saved_mode);
        g_game->setMurphysLaw(g_saved_murphy);
    }
    cfg_save();
    g_scene_dirty = true;
    game_show();
}

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

static void menu_mode()
{
    int sel = (int) g_opt->getModus() == ORIGINAL ? 0 : g_opt->getModus() == HINTS ? 1 : g_opt->getModus() == IMMUNE ? 2 : g_opt->getModus() == LUCKY ? 3 : 4;
    for (;;)
    {
        Options* o = g_opt;
        vector<MItem> it;
        it.push_back(MItem("Original", D_ORIG, o->getModus() == ORIGINAL, true, false,
            "Classic rules. When you are stuck you must guess, and a wrong guess ends the game."));
        it.push_back(MItem("Hint", D_HINT, o->getModus() == HINTS, true, false,
            "When you are stuck, the solver opens or flags one certain cell for you."));
        it.push_back(MItem("Immune", D_IMMUNE, o->getModus() == IMMUNE, true, false,
            "Your next move is protected: if you open a mine while guessing, the game goes on."));
        it.push_back(MItem("Lucky", D_LUCKY, o->getModus() == LUCKY, o->getMaxStage() == MAX_STAGE, false,
            "When you are stuck, a guess is always safe: the solver moves the mines to fit. (Needs max. stage 3.) This is the default."));
        it.push_back(MItem("Startup", D_STARTUP, o->getModus() == STARTUP, true, false,
            "A new game starts with enough cells opened and flagged that you never need to guess."));
        it.push_back(MItem("Murphy's Law", D_MURPHY, o->getMurphysLaw(), true, false,
            "The opposite of Lucky: any cell that could hold a mine does. For people who want a harder game."));
        int id = ui_menu("Mode: what happens when you must guess", it, sel);
        if (id < 0) return;
        if (id == D_MURPHY) { g_game->setMurphysLaw(!o->getMurphysLaw()); sel = 5; game_show(); continue; }
        g_game->changeModus(id == D_ORIG ? ORIGINAL : id == D_HINT ? HINTS : id == D_IMMUNE ? IMMUNE : id == D_LUCKY ? LUCKY : STARTUP);
        game_show();
        return;
    }
}

static bool menu_tools()
{
    int sel = 0;
    for (;;)
    {
        Options* o = g_opt;
        vector<MItem> it;
        it.push_back(MItem("Hint", T_HINT, -1, true, false,
            "Marks cells to look at: either open numbers that prove a covered cell, or covered cells you can open or flag."));
        it.push_back(MItem("Solve one move", T_ONE, -1, true, false, "The solver makes one certain move for you."));
        it.push_back(MItem("Solve all, up to stage 1", T_ALL1, -1, true, false,
            "Plays every certain move that needs only a single open cell (stage 1: the easiest)."));
        it.push_back(MItem("Solve all, up to stage 2", T_ALL2, -1, true, false,
            "Plays every certain move that needs one or two open cells together."));
        it.push_back(MItem("Solve all, up to stage 3", T_ALL3, -1, true, false,
            "Plays every certain move there is, however hard. The board is solved as far as it can be without guessing."));
        it.push_back(MItem("Auto-solve: off", T_AUTO0, o->getAutoStage() == 0, true, false, "You make all the moves yourself."));
        it.push_back(MItem("Auto-solve: stage 1", T_AUTO1, o->getAutoStage() == 1, true, false,
            "After each of your moves, the easiest certain moves are played for you."));
        it.push_back(MItem("Auto-solve: stage 2", T_AUTO2, o->getAutoStage() == 2, true, false,
            "After each of your moves, certain moves up to stage 2 are played for you."));
        it.push_back(MItem("Auto-solve: stage 3", T_AUTO3, o->getAutoStage() == 3, true, false,
            "After each of your moves, every certain move is played: the game nearly plays itself."));
        bool ms = o->getModus() != LUCKY;
        it.push_back(MItem("Max. stage 1", T_MAX1, o->getMaxStage() == 1, ms, false,
            "Treats anything harder than stage 1 as a dead end, so help (modes Hint/Immune/Startup) comes sooner. Not with Lucky."));
        it.push_back(MItem("Max. stage 2", T_MAX2, o->getMaxStage() == 2, ms, false, "Same, for anything harder than stage 2. Not with Lucky."));
        it.push_back(MItem("Max. stage 3", T_MAX3, o->getMaxStage() == 3, ms, false, "Normal: nothing is treated as too hard."));
        it.push_back(MItem("Show mines", T_MINES, o->getShowMines(), true, false, "Reveals where all the mines are."));
        int id = ui_menu("Solver tools (stop the clock: no record)", it, sel);
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

// Game Configuration: the solver (on/off, mode, tools) and saved games. Looks and sound are under Settings.
// Returns true when the menus should close (a tool made a move).
static bool menu_config()
{
    int sel = 0;
    for (;;)
    {
        vector<MItem> it;
        it.push_back(MItem(string("Solver: ") + (g_solver ? "On" : "Off"), C_SOLVER, g_solver ? 1 : 0, true, false,
            g_solver ? "Mines-Perfect's solver can protect you from guesses and help with hints. Switch it off to play classic Minesweeper, with no help at all."
                     : "Classic Minesweeper: no protection, no hints, no tools. Switch it on for the solver's modes and tools (it remembers your settings)."));
        if (g_solver)
        {
            it.push_back(MItem(string("Mode: ") + mode_name(g_opt->getModus()) + (g_opt->getMurphysLaw() ? " + Murphy" : ""), C_MODE, -1, true, true,
                "What the solver does when only a guess is left: protect you, help, or leave you alone."));
            it.push_back(MItem("Solver tools", C_TOOLS, -1, true, true,
                "Hint, solve a move or the whole board, auto-solve, show mines. Using them stops the clock: no record is made."));
        }
        it.push_back(MItem("Save game", C_SAVE, -1, true, false, "Saves this game (every move) to logs/<name>.log on the card."));
        it.push_back(MItem("Open game", C_OPEN, -1, true, false, "Replays a saved game and carries on from where it stopped."));
        int id = ui_menu("Game Configuration", it, sel);
        if (id < 0) return false;
        switch (id)
        {
        case C_SOLVER: solver_set(!g_solver); break;
        case C_MODE: menu_mode(); break;
        case C_TOOLS: if (menu_tools()) return true; break;
        case C_SAVE: menu_save(); break;
        case C_OPEN: menu_open(); return true;
        }
        // keep the cursor on the same item (the solver rows come and go)
        sel = id == C_SOLVER ? 0 : id == C_MODE ? 1 : id == C_TOOLS ? 2 : id == C_SAVE ? (g_solver ? 3 : 1) : (g_solver ? 4 : 2);
        game_show();
    }
}

// Background gallery: full-screen preview with the real game window on top, Left/Right to browse, A to choose.
static void menu_background()
{
    vector<string> files = bg_files();              // optional art pack and your own pictures
    int n = NSTOCK + (int) files.size();
    string orig = g_bg_name;
    auto entry = [&](int i) -> string { return i == 0 ? string() : i < NSTOCK ? string("@") + g_stock[i].name : files[i - NSTOCK]; };
    int cur = 0;
    for (int i = 0; i < n; i++) if (entry(i) == g_bg_name) cur = i;
    int shown = -1;
    for (;;)
    {
        if (shown != cur) { bg_apply(entry(cur)); shown = cur; }
        app_draw_scene();
        string name = bg_title_of(entry(cur));
        if (cur >= NSTOCK && !g_bg_on) name += " (cannot be read)";
        char head[48]; snprintf(head, sizeof head, "%d / %d", cur + 1, n);
        int w = 520, h = 50, x = (SCR_W - w) / 2, y = SCR_H - h - 6;
        g_bevel(g_scr, x, y, w, h, true, 0xC0C0C0);
        string line = string(head) + "   -   " + name;
        g_text_c(g_scr, font_bold(), x + w / 2, y + 4, 0x000000, line.c_str());
        g_text(g_scr, font_ui(), x + 12, y + 4, 0x000080, "<");
        g_text(g_scr, font_ui(), x + w - 12 - g_text_w(font_ui(), ">"), y + 4, 0x000080, ">");
        g_text_c(g_scr, font_small(), x + w / 2, y + 28, 0x404040, "Left/Right: browse     A: choose     B: cancel");
        ui_frame();
        if (g_pad.down & B_LEFT) cur = (cur + n - 1) % n;
        if (g_pad.down & B_RIGHT) cur = (cur + 1) % n;
        if (g_pad.down & B_A) { cfg_save(); return; }
        if (g_pad.down & B_B) { bg_apply(orig); return; }
    }
}

static void menu_settings()
{
    int sel = 0;
    for (;;)
    {
        vector<MItem> it;
        string bgname = bg_title_of(g_bg_name);
        it.push_back(MItem("Background: " + bgname, C_BG, -1, true, true,
            "The colour or picture behind the mine field, with a live preview. Classic is the plain original look. Pictures from the optional art pack, or your own .bmp files, appear after the colours when they are in the backgrounds folder."));
        char b[48];
        if (g_zoom_pref == 0) snprintf(b, sizeof b, "Zoom: auto"); else if (g_zoom_pref == 5) snprintf(b, sizeof b, "Zoom: fit (smooth)"); else snprintf(b, sizeof b, "Zoom: %dx", g_zoom_pref);
        it.push_back(MItem(b, C_ZOOM, -1, true, false,
            "Auto: the whole board, in whole steps (sharp) or smoothed when that leaves much unused. L2/R2 zoom out/in during play."));
        it.push_back(MItem(string("Sound: ") + (g_sound ? "On" : "Off"), C_SOUND, g_sound ? 1 : 0, true, false, "Game sound effects."));
        int id = ui_menu("Settings", it, sel);
        if (id < 0) return;
        if (id == C_BG) menu_background();
        else if (id == C_ZOOM) { g_zoom_pref = (g_zoom_pref + 1) % 6; cfg_save(); }
        else if (id == C_SOUND) { g_sound = !g_sound; cfg_save(); }
        sel = id == C_BG ? 0 : id == C_ZOOM ? 1 : 2;
        game_show();
    }
}

static void menu_help()
{
    vector<MItem> it;
    it.push_back(MItem("How to play", H_PAGES));
    it.push_back(MItem("What is the solver?", H_SOLVER));
    it.push_back(MItem("About", H_ABOUT));
    int id = ui_menu("Help", it, 0);
    if (id == H_PAGES) ui_help_pages(0);
    else if (id == H_SOLVER) ui_help_pages(PAGE_SOLVER);
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
        it.push_back(MItem("Restart This Board", M_RESTART, -1, true, false, "Plays the same board again from the start (undoes every move)."));
        it.push_back(MItem("Level", M_LEVEL, -1, true, true));
        it.push_back(MItem("Board", M_BOARD, -1, true, true, "The shape of the cells: squares, hexagons, triangles and more."));
        it.push_back(MItem("Best Times", M_BEST));
        it.push_back(MItem("Game Configuration", M_CONFIG, -1, true, true, "Solver on/off, mode and tools, save and open games."));
        it.push_back(MItem("Settings", M_SETTINGS, -1, true, true, "Background picture, zoom and sound."));
        it.push_back(MItem("Help", M_HELP, -1, true, true));
        it.push_back(MItem("Quit", M_QUIT));
        int id = ui_menu("Mines-Perfect", it, sel);
        if (id < 0 || id == M_RESUME) return;
        sel = id - 1;
        bool close = false;
        switch (id)
        {
        case M_NEW: g_game->newGame(); close = true; break;
        case M_RESTART: g_game->undo(true); close = true; break;
        case M_LEVEL: close = menu_level(); break;
        case M_BOARD: close = menu_board(); break;
        case M_BEST: ui_best_times(g_opt); break;
        case M_CONFIG: close = menu_config(); break;
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
    if (!g_solver) solver_off_state();       // classic mode stays classic across restarts
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
        else if (!strcmp(ui, "config")) menu_config();
        else if (!strcmp(ui, "settings")) menu_settings();
        else if (!strcmp(ui, "bg")) menu_background();
        else if (!strcmp(ui, "tools")) menu_tools();
        else if (!strcmp(ui, "mode")) menu_mode();
        else if (!strcmp(ui, "main")) menu_main();
        else if (!strcmp(ui, "helpmenu")) menu_help();
        game_show();
    }
    if (getenv("MP_UNDOREDO"))   // test hook: play to a loss (Original mode), undo to the start step by step, redo step by step
    {
        unsigned rs = (unsigned) (getenv("MP_SEED") ? atoi(getenv("MP_SEED")) : 1) * 2654435761u + 7;
        int maxclicks = getenv("MP_UR_CLICKS") ? atoi(getenv("MP_UR_CLICKS")) : 400;
        if (!getenv("MP_UR_KEEPMODE")) g_game->changeModus(ORIGINAL);
        Board* bd = g_game->getBoard();
        int clicks = 0, flags = 0;
        while (!bd->gameOver() && clicks < maxclicks)
        {
            rs = rs * 1664525u + 1013904223u;
            CellNr k = (CellNr) ((rs >> 8) % (unsigned) bd->getNumCells());
            if (bd->cellIsOpen(k)) continue;
            g_cur = k;
            if ((rs >> 4) % 5 == 0) { g_b_down = true; send_mouse(MouseEvent::RIGHT_DOWN); g_b_down = false; send_mouse(MouseEvent::RIGHT_UP); flags++; }
            else { g_a_down = true; send_mouse(MouseEvent::LEFT_DOWN); g_a_down = false; send_mouse(MouseEvent::LEFT_UP); }
            clicks++;
        }
        if (getenv("MP_UR_SOLVE"))    // a long, loss-free game made of solver steps and flags, then partial undo/redo
        {
            int steps = atoi(getenv("MP_UR_SOLVE"));
            for (int i = 0; i < steps && !bd->gameOver(); i++) { if (i % 4 == 3) { g_game->solveAll(1); } else g_game->solveOne(); }
            int bo = bd->getNumOpen(), bf = bd->getNumFlagged(), bl = bd->gameIsLost(), bw = bd->gameIsWon();
            int k = 3, bad = 0;
            for (int i = 0; i < k; i++) g_game->undo(false);
            for (int i = 0; i < k; i++) g_game->redo();
            Board* b3 = g_game->getBoard();
            if (b3->getNumOpen() != bo || b3->getNumFlagged() != bf || (int) b3->gameIsLost() != bl || (int) b3->gameIsWon() != bw) bad = 1;
            fprintf(stderr, "UR partial undo/redo x%d: open %d->%d flagged %d->%d => %s\n", k, bo, b3->getNumOpen(), bf, b3->getNumFlagged(), bad ? "DIFFERENT" : "SAME");
        }
        fprintf(stderr, "UR played: clicks=%d flags=%d state=%s open=%d\n", clicks, flags, bd->gameIsLost() ? "LOST" : bd->gameIsWon() ? "WON" : "?", bd->getNumOpen());
        int b_open = bd->getNumOpen(), b_flag = bd->getNumFlagged(), b_lost = bd->gameIsLost(), b_won = bd->gameIsWon();
        int ups = 0, downs = 0;
        try { for (; ups < 200; ups++) { int before = g_game->getBoard()->getNumOpen() + g_game->getBoard()->getNumFlagged(); g_game->undo(false); if (g_game->getBoard()->getNumOpen() + g_game->getBoard()->getNumFlagged() == before && ups > 0 && before == 0) break; } }
        catch (const Exception& e) { fprintf(stderr, "UR undo error after %d: %s\n", ups, e.getText().c_str()); }
        fprintf(stderr, "UR after undo x%d: open=%d flagged=%d\n", ups, g_game->getBoard()->getNumOpen(), g_game->getBoard()->getNumFlagged());
        try { for (; downs < 200; downs++) g_game->redo(); }
        catch (const Exception& e) { fprintf(stderr, "UR redo ERROR after %d redos: %s\n", downs, e.getText().c_str()); app_quit(3); }
        { Board* b2 = g_game->getBoard(); bool same = b2->getNumOpen() == b_open && b2->getNumFlagged() == b_flag && (int) b2->gameIsLost() == b_lost && (int) b2->gameIsWon() == b_won;
          fprintf(stderr, "UR redo x%d: open=%d flagged=%d lost=%d won=%d => %s\n", downs, b2->getNumOpen(), b2->getNumFlagged(), (int) b2->gameIsLost(), (int) b2->gameIsWon(), same ? "SAME AS BEFORE" : "DIFFERENT"); }
        app_quit(0);
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

            if ((g_pad.down & B_X) && g_solver) { g_game->giveHint(); game_show(); }
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
    if (!g_bg_name.empty()) bg_apply(g_bg_name);
    pad_poll();
    try { run(); }
    catch (const Exception& e) { fprintf(stderr, "mines-perfect: %s\n", e.getText().c_str()); ui_message("Error", e.getText().c_str()); }
    return 0;
}
