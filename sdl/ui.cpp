// ui.cpp - menus and dialogs for the SF3500 port (replaces the original's wxWidgets menu bar and dialogs).
// Windows-9x look to match the original. Not part of the original Mines-Perfect. GPL-2.0-or-later.
#include "app.h"
#include "../core/utils.h"
#include "../core/perfana.h"
namespace MinesPerfect { namespace Glob { extern int VERSION; } }
#include <stdio.h>
#include <string.h>
#include <time.h>

using namespace MinesPerfect;
using std::string;
using std::vector;

static const uint32_t FACE = 0xC0C0C0, NAVY = 0x000080, WHITE = 0xFFFFFF, GREY = 0x808080, BLACK = 0, RED = 0xA00000;

//******************************************************************************
// helpers
//------------------------------------------------------------------------------
static void scene() { app_draw_scene(); g_dim(g_scr); }

static void panel(int x, int y, int w, int h, const char* title)
{
    g_bevel(g_scr, x, y, w, h, true, FACE);
    g_fill(g_scr, x + 4, y + 4, w - 8, 26, NAVY);
    g_text(g_scr, font_bold(), x + 10, y + 6, WHITE, title);
}

static void wrap_text(const char* text, FontP f, int maxw, vector<string>& lines)
{
    string cur, word;
    for (const char* p = text;; p++)
    {
        char c = *p;
        if (c == ' ' || c == '\n' || c == 0)
        {
            if (!word.empty())
            {
                string t = cur.empty() ? word : cur + " " + word;
                if (g_text_w(f, t.c_str()) <= maxw || cur.empty()) cur = t;
                else { lines.push_back(cur); cur = word; }
                word.clear();
            }
            if (c == '\n' || c == 0) { lines.push_back(cur); cur.clear(); }
            if (c == 0) break;
        }
        else word += c;
    }
    while (lines.size() > 1 && lines.back().empty()) lines.pop_back();
}

//******************************************************************************
// message / confirm
//------------------------------------------------------------------------------
static bool message_box(const char* title, const char* text, bool ask)
{
    vector<string> lines;
    wrap_text(text, font_ui(), 440, lines);
    int lh = g_font_h(font_ui()) + 2;
    int w = 480, h = 34 + 20 + (int) lines.size() * lh + 40;
    if (h > 460) h = 460;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    for (;;)
    {
        scene();
        panel(x, y, w, h, title);
        for (size_t i = 0; i < lines.size(); i++)
            if (34 + 14 + (int) (i + 1) * lh < h - 30) g_text(g_scr, font_ui(), x + 20, y + 34 + 14 + (int) i * lh, BLACK, lines[i].c_str());
        g_text_c(g_scr, font_small(), x + w / 2, y + h - 26, 0x404040, ask ? "A: yes      B: no" : "A: OK");
        ui_frame();
        if (g_pad.down & B_A) return true;
        if (g_pad.down & B_B) return !ask ? true : false;
    }
}

void ui_message(const char* title, const char* text) { message_box(title, text, false); }
bool ui_confirm(const char* title, const char* text) { return message_box(title, text, true); }

//******************************************************************************
// menu
//------------------------------------------------------------------------------
int ui_menu(const char* title, const vector<MItem>& items, int sel)
{
    const int rowh = 27, maxrows = 14;
    int n = (int) items.size();
    if (n == 0) return -1;
    if (sel < 0 || sel >= n) sel = 0;
    bool any_help = false;
    for (int i = 0; i < n; i++) if (!items[i].help.empty()) any_help = true;
    int w = g_text_w(font_bold(), title) + 40;
    for (int i = 0; i < n; i++)
    {
        int tw = g_text_w(font_ui(), items[i].text.c_str()) + 70;
        if (tw > w) w = tw;
    }
    if (w < 300) w = 300;
    if (any_help && w < 520) w = 520;
    if (w > 600) w = 600;
    int helph = any_help ? 3 * (g_font_h(font_small()) + 1) + 14 : 0;     // room for three lines of description
    int rows = n < maxrows ? n : maxrows;
    if (any_help && rows > 11) rows = 11;
    int h = 34 + rows * rowh + 14 + helph;
    int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    int top = 0;
    for (;;)
    {
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        scene();
        panel(x, y, w, h, title);
        for (int r = 0; r < rows; r++)
        {
            int i = top + r;
            const MItem& it = items[i];
            int ry = y + 34 + r * rowh;
            bool on = i == sel;
            if (on) g_fill(g_scr, x + 6, ry, w - 12, rowh - 1, NAVY);
            uint32_t col = !it.enabled ? GREY : on ? WHITE : BLACK;
            if (it.check == 1)
            {   // check mark
                int cx = x + 14, cy = ry + 9;
                g_line(g_scr, cx, cy + 4, cx + 3, cy + 7, col); g_line(g_scr, cx + 1, cy + 4, cx + 4, cy + 7, col);
                g_line(g_scr, cx + 4, cy + 7, cx + 10, cy, col); g_line(g_scr, cx + 5, cy + 7, cx + 11, cy, col);
            }
            g_text(g_scr, font_ui(), x + 34, ry + 3, col, it.text.c_str());
            if (it.sub)
            {
                int ax = x + w - 24, ay = ry + 8;
                for (int k = 0; k < 6; k++) g_vline(g_scr, ax + k, ay + k, ay + 11 - k, col);
            }
        }
        if (n > rows)
        {   // scroll hint
            if (top > 0) g_text_c(g_scr, font_small(), x + w - 20, y + 8, WHITE, "^");
            if (top + rows < n) g_text_c(g_scr, font_small(), x + w - 20, y + 34 + rows * rowh, 0x404040, "v");
        }
        if (any_help)
        {
            int hy = y + 34 + rows * rowh + 6;
            g_hline(g_scr, x + 8, x + w - 9, hy, GREY);
            g_hline(g_scr, x + 8, x + w - 9, hy + 1, WHITE);
            vector<string> lines;
            wrap_text(items[sel].help.c_str(), font_small(), w - 28, lines);
            for (size_t i = 0; i < lines.size() && i < 3; i++)
                g_text(g_scr, font_small(), x + 14, hy + 5 + (int) i * (g_font_h(font_small()) + 1), 0x202020, lines[i].c_str());
        }
        ui_frame();
        if (g_pad.down & B_UP) sel = (sel + n - 1) % n;
        if (g_pad.down & B_DOWN) sel = (sel + 1) % n;
        if (g_pad.down & B_LEFT) { sel -= rows; if (sel < 0) sel = 0; }
        if (g_pad.down & B_RIGHT) { sel += rows; if (sel >= n) sel = n - 1; }
        if ((g_pad.down & B_A) && items[sel].enabled) return items[sel].id;
        if (g_pad.down & B_B) return -1;
    }
}

//******************************************************************************
// text entry (names, file names): alphanumeric plus - and _
//------------------------------------------------------------------------------
bool ui_text_entry(const char* title, const char* prompt, string& text, unsigned maxlen, const vector<string>* quick)
{
    static const char* rowsets[] = { "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz", "0123456789-_" };
    const int NR = 5, cell = 40;
    int cx = 0, cy = 0, qi = -1;
    string err;
    User::NameChecker checker;
    for (;;)
    {
        int w = 13 * cell + 28, h = 34 + 30 + 30 + NR * cell + 52 + 40;
        int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
        scene();
        panel(x, y, w, h, title);
        g_text(g_scr, font_ui(), x + 14, y + 38, BLACK, prompt);
        g_bevel(g_scr, x + 14, y + 62, w - 28, 32, false, WHITE);
        string shown = text + ((clock() / 400) % 2 ? "_" : " ");
        g_text(g_scr, font_ui(), x + 22, y + 66, BLACK, shown.c_str());
        int gy = y + 102;
        for (int r = 0; r < NR; r++)
        {
            for (int c = 0; rowsets[r][c]; c++)
            {
                char s[2] = { rowsets[r][c], 0 };
                bool on = (cx == c && cy == r);
                g_bevel(g_scr, x + 14 + c * cell, gy + r * cell, cell - 2, cell - 2, !on, on ? NAVY : FACE);
                g_text_c(g_scr, font_ui(), x + 14 + c * cell + cell / 2 - 1, gy + r * cell + 8, on ? WHITE : BLACK, s);
            }
        }
        const char* acts[3] = { "Delete", "OK", "Cancel" };
        int ay = gy + NR * cell + 2;
        for (int i = 0; i < 3; i++)
        {
            bool on = (cy == NR && cx / 5 == i) || (cy == NR && cx == i * 5);
            bool sel = cy == NR && (cx < 5 ? i == 0 : cx < 10 ? i == 1 : i == 2);
            (void) on;
            g_bevel(g_scr, x + 14 + i * 4 * cell + (i ? 0 : 0), ay, 4 * cell - 8, 36, !sel, sel ? NAVY : FACE);
            g_text_c(g_scr, font_ui(), x + 14 + i * 4 * cell + (4 * cell - 8) / 2, ay + 7, sel ? WHITE : BLACK, acts[i]);
        }
        if (!err.empty()) g_text(g_scr, font_small(), x + 14, y + h - 30, RED, err.c_str());
        else g_text(g_scr, font_small(), x + 14, y + h - 30, 0x404040, quick && !quick->empty() ? "A: key   B: delete   L1/R1: earlier names   START: OK" : "A: key   B: delete   START: OK");
        ui_frame();
        if (g_pad.down & B_UP) cy = (cy + NR) % (NR + 1);
        if (g_pad.down & B_DOWN) cy = (cy + 1) % (NR + 1);
        if (g_pad.down & B_LEFT) { cx = (cx + (cy == NR ? 15 : 13) - 1) % (cy == NR ? 15 : 13); }
        if (g_pad.down & B_RIGHT) { cx = (cx + 1) % (cy == NR ? 15 : 13); }
        if (cy < NR && cx >= (int) strlen(rowsets[cy])) cx = (int) strlen(rowsets[cy]) - 1;
        if ((g_pad.down & (B_L1 | B_R1)) && quick && !quick->empty())
        {
            int n = (int) quick->size();
            qi = (g_pad.down & B_R1) ? (qi + 1) % n : (qi + n - 1 + (qi < 0)) % n;
            text = (*quick)[qi].substr(0, maxlen);
            err.clear();
        }
        bool ok = false;
        if (g_pad.down & B_A)
        {
            if (cy < NR) { if (text.size() < maxlen) text += rowsets[cy][cx]; err.clear(); }
            else if (cx < 5) { if (!text.empty()) text.erase(text.size() - 1); }
            else if (cx < 10) ok = true;
            else return false;
        }
        if (g_pad.down & B_B) { if (!text.empty()) text.erase(text.size() - 1); err.clear(); }
        if (g_pad.down & B_START) ok = true;
        if (g_pad.down & B_Y) text.clear();
        if (ok)
        {
            string e;
            if (checker.isValidString(text, &e)) return true;
            err = "'" + text + "' " + e;
        }
    }
}

//******************************************************************************
// selfdefined board size
//------------------------------------------------------------------------------
bool ui_selfdefined(Level& lvl)
{
    enum { ROWS, COLS, PAGES, MINES, WHOLES, OK, CANCEL, NROW };
    int sel = ROWS;
    Level l = lvl;
    l.nr = USER_DEFINED;
    for (;;)
    {
        bool show[NROW] = { true, true, l.deep > 0, true, l.num_wholes > -1, true, true };
        const char* names[NROW] = { "Rows", "Columns", "Pages", "Mines", "Holes", "OK", "Cancel" };
        int vals[NROW] = { l.height, l.width, l.deep, l.num_mines, l.num_wholes, 0, 0 };
        int w = 420, h = 34 + 14;
        for (int i = 0; i < NROW; i++) if (show[i]) h += 32;
        h += 40;
        int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
        scene();
        panel(x, y, w, h, "Custom board");
        int ry = y + 40;
        for (int i = 0; i < NROW; i++)
        {
            if (!show[i]) continue;
            bool on = i == sel;
            if (on) g_fill(g_scr, x + 6, ry - 2, w - 12, 30, NAVY);
            uint32_t col = on ? WHITE : BLACK;
            g_text(g_scr, font_ui(), x + 20, ry + 2, col, names[i]);
            if (i < OK)
            {
                char b[32]; snprintf(b, sizeof b, "< %d >", vals[i]);
                g_text(g_scr, font_ui(), x + w - 20 - g_text_w(font_ui(), b), ry + 2, col, b);
            }
            ry += 32;
        }
        g_text_c(g_scr, font_small(), x + w / 2, y + h - 26, 0x404040, "Left/Right: change by 1     L1/R1: change by 10");
        ui_frame();
        int d = 0;
        if (g_pad.down & B_LEFT) d = -1;
        if (g_pad.down & B_RIGHT) d = 1;
        if (g_pad.down & B_L1) d = -10;
        if (g_pad.down & B_R1) d = 10;
        if (g_pad.down & B_UP) { do sel = (sel + NROW - 1) % NROW; while (!show[sel]); }
        if (g_pad.down & B_DOWN) { do sel = (sel + 1) % NROW; while (!show[sel]); }
        if (d && sel < OK)
        {
            int* p = sel == ROWS ? &l.height : sel == COLS ? &l.width : sel == PAGES ? &l.deep : sel == MINES ? &l.num_mines : &l.num_wholes;
            *p += d;
            if (l.height < 1) l.height = 1;
            if (l.height > MAX_ROWS) l.height = MAX_ROWS;
            if (l.width < 1) l.width = 1;
            if (l.width > MAX_COLS) l.width = MAX_COLS;
            if (l.deep < 1 && lvl.deep > 0) l.deep = 1;
            if (l.deep > 99) l.deep = 99;
            if (l.num_wholes < 0 && lvl.num_wholes > -1) l.num_wholes = 0;
            int maxm = l.height * l.width - 1;
            if (l.num_mines > maxm) l.num_mines = maxm;
            if (l.num_mines < 0) l.num_mines = 0;
        }
        if (g_pad.down & B_B) return false;
        if ((g_pad.down & B_A) && sel == CANCEL) return false;
        if (((g_pad.down & B_A) && sel == OK) || (g_pad.down & B_START)) { lvl = l; return true; }
    }
}

//******************************************************************************
// best times
//------------------------------------------------------------------------------
static string fmt_time(int ms)
{
    char b[32];
    snprintf(b, sizeof b, "%d,%03d s", ms / 1000, ms % 1000);
    return b;
}

void ui_best_times(Options* options)
{
    int board = options->getBoardNr(), lvl = 0, nb = options->getNumBoards();
    static const char* lvlnames[3] = { "Beginner", "Intermediate", "Expert" };
    for (;;)
    {
        BoardType* bt = options->getBoardType(board);
        int w = 560, h = 34 + 54 + 3 * 36 + 60;
        int x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
        scene();
        panel(x, y, w, h, "Best Times");
        string bn = "Board: " + bt->name;
        g_text(g_scr, font_bold(), x + 20, y + 40, BLACK, bn.c_str());
        if (nb > 1) g_text(g_scr, font_small(), x + w - 20 - g_text_w(font_small(), "L1/R1: board"), y + 44, 0x404040, "L1/R1: board");
        for (int i = 0; i < 3; i++)
        {
            const Record& r = bt->records[i];
            int ry = y + 34 + 54 + i * 36;
            bool on = i == lvl;
            if (on) g_fill(g_scr, x + 6, ry - 3, w - 12, 33, NAVY);
            uint32_t col = on ? WHITE : BLACK;
            g_text(g_scr, font_ui(), x + 20, ry + 2, col, lvlnames[i]);
            if (r.time < MAX_MSECS)
            {
                g_text(g_scr, font_ui(), x + 190, ry + 2, col, fmt_time(r.time).c_str());
                g_text(g_scr, font_ui(), x + 340, ry + 2, col, r.name.c_str());
            }
            else g_text(g_scr, font_ui(), x + 190, ry + 2, on ? 0xC0C0C0 : GREY, "--");
        }
        g_text_c(g_scr, font_small(), x + w / 2, y + h - 30, 0x404040, "X: delete this record    B: close");
        ui_frame();
        if (g_pad.down & B_UP) lvl = (lvl + 2) % 3;
        if (g_pad.down & B_DOWN) lvl = (lvl + 1) % 3;
        if (g_pad.down & B_L1) board = (board + nb - 1) % nb;
        if (g_pad.down & B_R1) board = (board + 1) % nb;
        if ((g_pad.down & B_X) && bt->records[lvl].time < MAX_MSECS)
            if (ui_confirm("Delete record", "Delete this record?")) { bt->deleteRecord(lvl); options->saveIni(); }
        if (g_pad.down & (B_B | B_A | B_START)) return;
    }
}

void ui_new_record(Options* options, int msecs, bool certified)
{
    static const char* lvlnames[3] = { "Beginner", "Intermediate", "Expert" };
    int rec_nr = options->getLevelNr();
    if (rec_nr < 0 || rec_nr > 2) return;
    string msg = string("You have a new record.\n\nBoard:  ") + options->getBoardName() + "\nLevel:  " + lvlnames[rec_nr] + "\nTime:  " + fmt_time(msecs);
    ui_message("Congratulations", msg.c_str());
    vector<string> users;
    options->getUserlist(users);
    string name = users.empty() ? "Player" : users[0];
    while (!ui_text_entry("Congratulations", "Your name:", name, NAME_LEN, &users)) {}
    options->setRecord(rec_nr, name, msecs, certified);
    options->saveIni();
    ui_best_times(options);
}

//******************************************************************************
// pick one of a list of names
//------------------------------------------------------------------------------
int ui_pick_file(const char* title, const vector<string>& names)
{
    vector<MItem> items;
    for (size_t i = 0; i < names.size(); i++) items.push_back(MItem(names[i], (int) i));
    if (items.empty()) { ui_message(title, "There are no saved games."); return -1; }
    return ui_menu(title, items, 0);
}

//******************************************************************************
// help pages
//------------------------------------------------------------------------------
struct HelpPage { const char* title; const char* text; };
static const HelpPage g_pages[] =
{
    { "How to play",
      "Mines are hidden under the grey cells. Open a cell with a mine and the game is over. Open every cell that has no mine and you win - the faster, the better.\n"
      "A number on an opened cell tells you how many of the cells touching it hide a mine. Use the numbers to work out where the mines are, and put a flag on each one.\n"
      "The little clock on the right counts seconds, the counter on the left shows how many mines are still unflagged." },
    { "Controls",
      "D-pad: move the yellow cursor.\n"
      "A: open the cell.     B: put a flag on it.\n"
      "X: hint (when the solver is on).     Y: new game.\n"
      "L1: undo a move.     R1: redo it.\n"
      "L2 / R2: zoom out / in, one step at a time. Both together: back to Auto, the whole board.\n"
      "START: the menu.     SELECT + START: quit." },
    { "Opening and flagging",
      "Press A on a covered cell to open it, B to flag it.\n"
      "Both buttons also work as shortcuts on a number that is already open:\n"
      "A opens all the covered cells around it in one go - use it when you have already flagged as many mines around the number as it says.\n"
      "B flags all the covered cells around it in one go - use it when the number says that every one of them is a mine.\n"
      "If the number does not fit, nothing happens." },
    { "What is the solver?",
      "Mines-Perfect has a built-in solver: a player that never gets it wrong. It reads the numbers and knows, for every covered cell, whether it is certainly safe, certainly a mine, or cannot be told.\n"
      "You can ignore it completely: START > Game Configuration > Solver: Off gives you plain classic Minesweeper.\n"
      "With the solver on, it can warn you (the smiley), help you (hints and tools) and look after you when you would otherwise have to guess (the Mode).\n"
      "Using a hint or a tool stops the clock, so no record is made." },
    { "The smiley",
      "The smiley shows how hard the easiest safe move on the board is right now:\n"
      "Eye patch: easy. One opened number is enough to find a safe move.\n"
      "Smile with grey corners: medium. Two numbers together tell you something.\n"
      "Monocle: hard. You need three or more numbers together.\n"
      "Plain smile during a game: no sure move is left, you would have to guess - but your Mode looks after you.\n"
      "Toothy grin: no sure move is left and nothing protects you.\n"
      "Sunglasses: you won.     Crossed eyes: you lost." },
    { "Modes",
      "A mode decides what happens when only a guess is left. Pick it under START > Game Configuration > Mode.\n"
      "Original: classic. A wrong guess ends the game.\n"
      "Hint: the game opens or flags one safe cell for you.\n"
      "Immune: your next move is protected; open a mine and the game simply carries on.\n"
      "Lucky (the default): a guess is always safe. The game quietly moves the mines to fit what you can see.\n"
      "Startup: the game starts with enough cells open and flagged that you never have to guess.\n"
      "Murphy's Law: the opposite of Lucky - if a cell could hide a mine, it does." },
    { "Solver tools",
      "START > Game Configuration > Solver tools (only when the solver is on):\n"
      "Hint shows cells worth a look. Solve one move makes one sure move for you. Solve all plays every sure move up to a difficulty. Auto-solve does that after each move you make.\n"
      "Max. stage limits how hard a move you are expected to work out yourself, so the Mode helps you sooner. Show mines reveals every mine.\n"
      "Any tool stops the clock and no record is made. Undo, redo and Restart This Board are always available." },
    { "Sound",
      "The game plays a sound for: a new game, opening, flagging, a dead end, an error, winning and losing. Switch sound off under START > Settings > Sound.\n"
      "You can swap in your own .wav files: put files named new.wav, open.wav, flag.wav, error.wav, deadend.wav, won.wav and lost.wav into the sound folder of the game on the card." },
};
static const int NPAGES = (int) (sizeof g_pages / sizeof *g_pages);

void ui_help_pages(int first)
{
    int pg = first;
    for (;;)
    {
        // one paragraph per line of the text, a little gap between paragraphs
        vector<string> paras;
        { string cur; for (const char* q = g_pages[pg].text;; q++) { if (*q == '\n' || *q == 0) { paras.push_back(cur); cur.clear(); if (!*q) break; } else cur += *q; } }
        scene();
        int w = 600, h = 440, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
        panel(x, y, w, h, g_pages[pg].title);
        int lh = g_font_h(font_ui()) + 1, ty = y + 42;
        for (size_t pi = 0; pi < paras.size(); pi++)
        {
            vector<string> lines;
            wrap_text(paras[pi].c_str(), font_ui(), 556, lines);
            for (size_t i = 0; i < lines.size(); i++, ty += lh)
                if (ty + lh < y + h - 34) g_text(g_scr, font_ui(), x + 22, ty, BLACK, lines[i].c_str());
            ty += 7;
        }
        char foot[64]; snprintf(foot, sizeof foot, "Left/Right: page %d of %d      B: close", pg + 1, NPAGES);
        g_text_c(g_scr, font_small(), x + w / 2, y + h - 26, 0x404040, foot);
        ui_frame();
        if (g_pad.down & B_LEFT) pg = (pg + NPAGES - 1) % NPAGES;
        if (g_pad.down & B_RIGHT) pg = (pg + 1) % NPAGES;
        if (g_pad.down & (B_B | B_START)) return;
    }
}

void ui_about()
{
    char ver[16];
    snprintf(ver, sizeof ver, "%d.%d.%d", Glob::VERSION / 100 % 10, Glob::VERSION / 10 % 10, Glob::VERSION % 10);
    string t = string("Mines-Perfect, version ") + ver + "\n"
        "A Minesweeper with a solver, by Christian Czepluch (c) 1995-2003.\n"
        "Free software under the GNU General Public License, version 2 or later.\n\n"
        "This handheld version keeps the original game and adds new controls, menus and settings.\n\n"
        "www.czeppi.de\n"
        "sourceforge.net/projects/mines-perfect";
    ui_message("About", t.c_str());
}
