// app.h - glue between the original controls (api_sdl.cpp), the screen (main.cpp) and the menus/dialogs (ui.cpp).
// Not part of the original Mines-Perfect. GPL-2.0-or-later.
#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include "gfx.h"
#include "plat.h"
#include "../core/options.h"

extern Surf g_win;            // the original's window, drawn by its controls
extern bool g_win_dirty;
extern Surf g_scr;            // the 640x480 screen

// api_sdl.cpp / main.cpp
void app_window_resized();
bool app_sound_on();
void app_show_time(int secs);
void app_poll_timer();
void app_draw_scene();                  // game view on g_scr (no present)
void app_quit(int code) __attribute__((noreturn));

bool app_load_bmp(const std::string& fname, Argb& out);   // api_sdl.cpp

// input (main.cpp). pad_poll() once per frame; `down` = pressed this frame (d-pad also auto-repeats), `held` = still down
struct PadState { unsigned held, down, up; };
extern PadState g_pad;
void pad_poll();
bool ui_frame();                        // present the frame, wait for the next one, poll the pad; false never (quit exits)

// ui.cpp
struct MItem
{
    std::string text;
    int id;                             // returned when chosen
    int check;                          // -1 none, 0 radio/check off, 1 on
    bool enabled;
    bool sub;                           // shows an arrow
    std::string help;                   // one or two plain-language lines shown at the bottom while the item is selected
    MItem(const std::string& t, int i, int c = -1, bool en = true, bool s = false, const char* h = "") : text(t), id(i), check(c), enabled(en), sub(s), help(h) {}
};
int  ui_menu(const char* title, const std::vector<MItem>& items, int sel = 0);                      // id, or -1 for B
void ui_message(const char* title, const char* text);
bool ui_confirm(const char* title, const char* text);
bool ui_text_entry(const char* title, const char* prompt, std::string& text, unsigned maxlen, const std::vector<std::string>* quick);
bool ui_selfdefined(MinesPerfect::Level& lvl);
void ui_best_times(MinesPerfect::Options* options);
void ui_new_record(MinesPerfect::Options* options, int msecs, bool certified);
void ui_help_pages(int first);          // first = page number (see g_pages in ui.cpp: PAGE_SOLVER)
enum { PAGE_SOLVER = 2 };
void ui_about();
int  ui_pick_file(const char* title, const std::vector<std::string>& names);
void g_dim(Surf& s);

#endif
