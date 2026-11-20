// api_sdl.cpp - implements core/api.h (the original's platform interface, formerly wxWidgets) on the SF3500 platform layer.
// Not part of the original Mines-Perfect. GPL-2.0-or-later.
#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <time.h>
#include <strings.h>

#include "../core/api.h"
#include "../gui/gamectrl.h"
#include "gfx.h"
#include "plat.h"
#include "app.h"

#include "lcd.xpm"
#include "symbols.xpm"
#include "smileys.xpm"
#include "button.xpm"
#include "square.xpm"
#include "triangle.xpm"
#include "hexagon.xpm"
#include "grid3d.xpm"

using namespace MinesPerfect;

extern "C" long mp_clock_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long) ((ts.tv_sec % 1000000) * 1000 + ts.tv_nsec / 1000000);
}

//******************************************************************************
// pictures
//------------------------------------------------------------------------------
struct Image { Argb px; };

static Image g_lcd, g_symbols, g_smileys, g_button, g_square, g_triangle, g_hexagon, g_grid3d;

static bool parse_xpm(const char* const* xpm, Image& out, bool transparent)
{
    int w, h, nc, cpp;
    if (sscanf(xpm[0], "%d %d %d %d", &w, &h, &nc, &cpp) != 4 || cpp != 1) return false;
    uint32_t pal[256];
    for (int i = 0; i < 256; i++) pal[i] = 0;
    for (int i = 0; i < nc; i++)
    {
        const char* line = xpm[1 + i];
        unsigned char key = (unsigned char) line[0];
        const char* c = strstr(line + 1, "c ");
        if (!c) return false;
        c += 2;
        while (*c == ' ') c++;
        if (!strncasecmp(c, "None", 4)) pal[key] = 0x00000000;
        else if (*c == '#') pal[key] = 0xFF000000u | (uint32_t) strtoul(c + 1, 0, 16);
        else return false;
    }
    out.px.w = w; out.px.h = h;
    out.px.px.assign((size_t) w * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            out.px.px[(size_t) y * w + x] = pal[(unsigned char) xpm[1 + nc + y][x]];
    if (transparent)    // the original takes the bottom-left pixel as the mask colour
    {
        uint32_t key = out.px.px[(size_t) (h - 1) * w] & 0xFFFFFF;
        for (size_t i = 0; i < out.px.px.size(); i++)
            if ((out.px.px[i] & 0xFFFFFF) == key) out.px.px[i] = 0;
    }
    return true;
}

static bool load_bmp(const std::string& fname, Image& out, bool transparent)
{
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) return false;
    unsigned char hd[54];
    if (fread(hd, 1, 54, f) != 54 || hd[0] != 'B' || hd[1] != 'M') { fclose(f); return false; }
    int off = hd[10] | (hd[11] << 8) | (hd[12] << 16) | (hd[13] << 24);
    int w = (int) (hd[18] | (hd[19] << 8) | (hd[20] << 16) | (hd[21] << 24));
    int h = (int) (hd[22] | (hd[23] << 8) | (hd[24] << 16) | (hd[25] << 24));
    int bpp = hd[28] | (hd[29] << 8);
    int comp = hd[30] | (hd[31] << 8);
    bool flip = true;
    if (h < 0) { h = -h; flip = false; }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || comp != 0 || (bpp != 24 && bpp != 32 && bpp != 8)) { fclose(f); return false; }
    std::vector<uint32_t> pal(256, 0);
    if (bpp == 8)
    {
        int ncol = hd[46] | (hd[47] << 8); if (!ncol) ncol = 256;
        fseek(f, 14 + (hd[14] | (hd[15] << 8)), SEEK_SET);
        for (int i = 0; i < ncol; i++) { unsigned char q[4]; if (fread(q, 1, 4, f) != 4) { fclose(f); return false; } pal[i] = (q[2] << 16) | (q[1] << 8) | q[0]; }
    }
    fseek(f, off, SEEK_SET);
    int stride = ((w * bpp + 31) / 32) * 4;
    std::vector<unsigned char> row(stride);
    out.px.w = w; out.px.h = h; out.px.px.assign((size_t) w * h, 0);
    for (int j = 0; j < h; j++)
    {
        if (fread(&row[0], 1, stride, f) != (size_t) stride) { fclose(f); return false; }
        int y = flip ? h - 1 - j : j;
        for (int x = 0; x < w; x++)
        {
            uint32_t c;
            if (bpp == 8) c = pal[row[x]];
            else c = (row[x * (bpp / 8) + 2] << 16) | (row[x * (bpp / 8) + 1] << 8) | row[x * (bpp / 8)];
            out.px.px[(size_t) y * w + x] = 0xFF000000u | c;
        }
    }
    fclose(f);
    if (transparent)
    {
        uint32_t key = out.px.px[(size_t) (h - 1) * w] & 0xFFFFFF;
        for (size_t i = 0; i < out.px.px.size(); i++)
            if ((out.px.px[i] & 0xFFFFFF) == key) out.px.px[i] = 0;
    }
    return true;
}

class BitmapImpl : public MinesPerfect::Bitmap
{
public:
    Argb img;
    bool trans;

    BitmapImpl(const string& kind, const string& spec);
    BitmapImpl(const MinesPerfect::Bitmap* from, const MinesPerfect::Rect& rect);

    Point getSize() const { return Point(img.w, img.h); }
    bool isTransparent() const { return trans; }
    bool pointIsTransparent(const Point& p) const
    {
        if (p.x < 0 || p.y < 0 || p.x >= img.w || p.y >= img.h) return true;
        return (img.px[(size_t) p.y * img.w + p.x] >> 24) < 128;
    }
};

static void sub_image(const Argb& src, int x0, int y0, int w, int h, Argb& out)
{
    out.w = w; out.h = h; out.px.assign((size_t) w * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int sx = x0 + x, sy = y0 + y;
            if (sx >= 0 && sy >= 0 && sx < src.w && sy < src.h) out.px[(size_t) y * w + x] = src.px[(size_t) sy * src.w + sx];
        }
}

BitmapImpl::BitmapImpl(const string& kind, const string& spec) : trans(true)
{
    int nr = -1, num_cols = -1, num_rows = -1;
    const Image* image = 0;

    if (kind == "board")
    {
        static Image ext;
        string fname = string("./boards/") + spec + ".bmp";
        if (load_bmp(fname, ext, true)) { img = ext.px; return; }
        if (spec == "Square") image = &g_square;
        else if (spec == "Triangle") image = &g_triangle;
        else if (spec == "Hexagon") image = &g_hexagon;
        else if (spec == "3d-Grid") image = &g_grid3d;
        else ASSERT(false);
        img = image->px;
        return;
    }

    if (kind == "smiley")
    {
        if (spec == "happy") nr = 0; else if (spec == "1eye") nr = 1; else if (spec == "2eyes") nr = 2;
        else if (spec == "3eyes") nr = 3; else if (spec == "4eyes") nr = 4; else if (spec == "norm") nr = 5;
        else if (spec == "ooh") nr = 6; else if (spec == "worry") nr = 7; else if (spec == "sunglass") nr = 8;
        else if (spec == "tmp") nr = 9;
        num_cols = 5; num_rows = 2; image = &g_smileys;
    }
    else if (kind == "lcd")
    {
        nr = atoi(spec.c_str());
        if (nr <= 0) { if (spec == "0") nr = 0; else if (spec == "-") nr = 10; else nr = -1; }
        num_cols = 11; num_rows = 1; image = &g_lcd; trans = false;
    }
    else if (kind == "symbol")
    {
        nr = atoi(spec.c_str());
        if (nr > 0) nr += 4;
        else
        {
            if (spec == "flag") nr = 0; else if (spec == "quest") nr = 1; else if (spec == "mine") nr = 2;
            else if (spec == "cross") nr = 3; else if (spec == "0") nr = 4; else nr = -1;
        }
        num_cols = 4; num_rows = 8; image = &g_symbols;
    }
    else if (kind == "button")
    {
        if (spec == "up") nr = 0; else if (spec == "down") nr = 1;
        num_cols = 2; num_rows = 1; image = &g_button; trans = false;
    }
    else ASSERT(false);

    ASSERT(nr >= 0 && nr < num_rows * num_cols);
    int i = nr / num_cols, j = nr % num_cols;
    int w = image->px.w / num_cols, h = image->px.h / num_rows;
    sub_image(image->px, j * w, i * h, w, h, img);
}

BitmapImpl::BitmapImpl(const MinesPerfect::Bitmap* from, const MinesPerfect::Rect& rect) : trans(true)
{
    const BitmapImpl* f = dynamic_cast<const BitmapImpl*>(from);
    ASSERT(f != 0);
    trans = f->trans;
    sub_image(f->img, rect.getLeft(), rect.getTop(), rect.getWidth(), rect.getHeight(), img);
}

MinesPerfect::Bitmap* MinesPerfect::CreateBitmap(const string& kind, const string& spec) { return new BitmapImpl(kind, spec); }
MinesPerfect::Bitmap* MinesPerfect::CreateBitmap(const MinesPerfect::Bitmap* from, const MinesPerfect::Rect& rect) { return new BitmapImpl(from, rect); }
MinesPerfect::Bitmap* MinesPerfect::CreateBitmap(const vector<MinesPerfect::Color>&, int) { ASSERT(false); return 0; }   // not implemented in the original either

void MinesPerfect::InitApi()
{
    parse_xpm(lcd_xpm, g_lcd, false);
    parse_xpm(symbols_xpm, g_symbols, true);
    parse_xpm(smileys_xpm, g_smileys, true);
    parse_xpm(button_xpm, g_button, false);
    parse_xpm(square_xpm, g_square, true);
    parse_xpm(triangle_xpm, g_triangle, true);
    parse_xpm(hexagon_xpm, g_hexagon, true);
    parse_xpm(grid3d_xpm, g_grid3d, true);
}

//******************************************************************************
// window canvas
//------------------------------------------------------------------------------
Surf g_win;                   // the "window": head (LCDs, smiley) and board, drawn by the original controls
bool g_win_dirty = true;
const uint32_t WIN_BG = 0xC0C0C0;

void MinesPerfect::WinSetSize(const Point& sz)
{
    if (sz.x != g_win.w || sz.y != g_win.h) g_win.make(sz.x, sz.y);
    g_fill(g_win, 0, 0, g_win.w, g_win.h, WIN_BG);
    g_win_dirty = true;
    app_window_resized();
}

void MinesPerfect::WinDrawBitmap(const MinesPerfect::Bitmap* bmp, const Point& p)
{
    if (!bmp) return;
    const BitmapImpl* b = dynamic_cast<const BitmapImpl*>(bmp);
    ASSERT(b != 0);
    if (!b->trans)   // opaque pictures (LCD digits, buttons): copy everything
    {
        for (int j = 0; j < b->img.h; j++)
            for (int i = 0; i < b->img.w; i++)
                g_pixel(g_win, p.x + i, p.y + j, b->img.px[(size_t) j * b->img.w + i]);
    }
    else g_blit_key(g_win, b->img, p.x, p.y);
    g_win_dirty = true;
}

void MinesPerfect::WinDrawBevel(const MinesPerfect::Rect& rect, int thickness, bool raised)
{
    if (rect.getWidth() < 2 * thickness || rect.getHeight() < 2 * thickness) return;
    const int l = rect.getLeft(), t = rect.getTop();
    const int r = rect.getLeft() + rect.getWidth() - 1, b = rect.getTop() + rect.getHeight() - 1;
    const uint32_t GREY = 0x808080, WHITE = 0xFFFFFF;
    uint32_t c1 = raised ? GREY : WHITE, c2 = raised ? WHITE : GREY;
    int i;
    for (i = 0; i < thickness; i++)
    {
        g_line(g_win, r - i, t + i, r - i, b - i, c1);          // right
        g_line(g_win, r - i, b - i, l + i - 1, b - i, c1);      // bottom
    }
    for (i = 0; i < thickness; i++)
    {
        g_line(g_win, l + i, b - i - 1, l + i, t + i, c2);      // left
        g_line(g_win, l + i, t + i, r - i, t + i, c2);          // top
    }
    g_win_dirty = true;
}

//******************************************************************************
// sound
//------------------------------------------------------------------------------
class SoundImpl : public MinesPerfect::Sound
{
public:
    SoundImpl(const string& name);
    bool isOk() { return ok; }
    void play() { if (ok && app_sound_on()) plat_snd_play(id, 1.0f); }
private:
    bool ok;
    int id;
    std::vector<short> pcm;
    static int next_id;
    bool load(const string& fname);
};
int SoundImpl::next_id = 0;

static unsigned rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned) p[3] << 24); }

bool SoundImpl::load(const string& fname)
{
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 44 || n > 8 * 1024 * 1024) { fclose(f); return false; }
    std::vector<unsigned char> d(n);
    if (fread(&d[0], 1, n, f) != (size_t) n) { fclose(f); return false; }
    fclose(f);
    if (memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "WAVE", 4)) return false;
    int fmt = 0, ch = 0, rate = 0, bits = 0; const unsigned char* data = 0; long dlen = 0;
    for (long p = 12; p + 8 <= n;)
    {
        unsigned long len = rd32(&d[p + 4]);
        if (!memcmp(&d[p], "fmt ", 4) && p + 8 + 16 <= n) { fmt = rd16(&d[p + 8]); ch = rd16(&d[p + 10]); rate = rd32(&d[p + 12]); bits = rd16(&d[p + 22]); }
        else if (!memcmp(&d[p], "data", 4)) { data = &d[p + 8]; dlen = (long) len; if (p + 8 + dlen > n) dlen = n - p - 8; break; }
        p += 8 + len + (len & 1);
    }
    if (fmt != 1 || !data || (ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate < 4000) return false;
    long frames = dlen / (ch * bits / 8);
    if (frames < 1) return false;
    // to 48 kHz stereo S16 (linear interpolation)
    long outn = (long) ((double) frames * 48000.0 / rate);
    pcm.assign((size_t) outn * 2, 0);
    for (long i = 0; i < outn; i++)
    {
        double pos = (double) i * rate / 48000.0;
        long i0 = (long) pos; double fr = pos - i0; long i1 = i0 + 1 < frames ? i0 + 1 : i0;
        for (int c = 0; c < 2; c++)
        {
            int cc = c < ch ? c : 0;
            double v[2];
            long idx[2] = { i0, i1 };
            for (int k = 0; k < 2; k++)
            {
                if (bits == 8) v[k] = ((int) data[idx[k] * ch + cc] - 128) * 256.0;
                else v[k] = (short) rd16(data + (idx[k] * ch + cc) * 2);
            }
            pcm[(size_t) i * 2 + c] = (short) (v[0] + (v[1] - v[0]) * fr);
        }
    }
    return true;
}

SoundImpl::SoundImpl(const string& name) : ok(false), id(-1)
{
    string fname = "./sound/" + name + ".wav";
    if (!MinesPerfect::FileExist(fname))
    {   // the original shipped upper/lower case mixes (FLAG.WAV, Won.wav): look case-insensitively
        DIR* d = opendir("./sound");
        if (d)
        {
            struct dirent* e;
            while ((e = readdir(d)))
            {
                string n = e->d_name;
                if (!strcasecmp(n.c_str(), (name + ".wav").c_str())) { fname = "./sound/" + n; break; }
            }
            closedir(d);
        }
    }
    if (!load(fname)) return;
    id = next_id++;
    plat_snd_set(id, &pcm[0], (int) (pcm.size() / 2));
    ok = true;
}

MinesPerfect::Sound* MinesPerfect::CreateSound(const string& name) { return new SoundImpl(name); }

//******************************************************************************
// timer (polled by the main loop instead of a wx timer)
//------------------------------------------------------------------------------
class TimerImpl : public MinesPerfect::Timer
{
public:
    TimerImpl() : running(false), t0(0), t1(0), notified(-1) {}
    bool isRunning() const { return running; }
    int getMSecs() const { return (int) ((running ? mp_clock_ms() : t1) - t0); }
    clock_t start() { reset(); t0 = mp_clock_ms(); running = true; return (clock_t) t0; }
    void stop() { if (running) { t1 = mp_clock_ms(); running = false; } }
    void reset() { stop(); notified = -1; t1 = t0 = mp_clock_ms(); }
    void Notify()
    {
        if (!running) return;
        int s = getMSecs() / 1000;
        if (s != notified) { notified = s; app_show_time(s); }
    }
private:
    bool running; long t0, t1; int notified;
};
static TimerImpl* g_timer = 0;
MinesPerfect::Timer* MinesPerfect::CreateTimer() { g_timer = new TimerImpl(); return g_timer; }
void app_poll_timer() { if (g_timer) g_timer->Notify(); }

//******************************************************************************
// files and dialogs
//------------------------------------------------------------------------------
void MinesPerfect::FindFiles(vector<string>& files, const string& pattern)
{
    string dir = ".", pat = pattern;
    string::size_type i = pattern.rfind('/');
    if (i != string::npos) { dir = pattern.substr(0, i); pat = pattern.substr(i + 1); }
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)))
        if (e->d_name[0] != '.' && fnmatch(pat.c_str(), e->d_name, FNM_CASEFOLD) == 0) files.push_back(e->d_name);
    closedir(d);
}

bool MinesPerfect::FileExist(const string& fname)
{
    struct stat st;
    return stat(fname.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void MinesPerfect::ShowMessageDlg(const string& text, const string& title) { ui_message(title.c_str(), text.c_str()); }
bool MinesPerfect::StartBrowser(const string&, const string&) { return false; }
void MinesPerfect::DlgNewRecord(Options* options, int num_msecs, bool certified_board) { ui_new_record(options, num_msecs, certified_board); }
