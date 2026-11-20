// gfx.h - tiny software renderer for the SF3500 Mines-Perfect port (RGB565 surfaces, baked DejaVu text).
// Not part of the original Mines-Perfect. GPL-2.0-or-later.
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <vector>
#include <string>

struct Surf
{
    int w, h;
    uint16_t* px;                  // rows are w pixels apart
    std::vector<uint16_t> store;   // owns px unless wrapping the screen
    Surf() : w(0), h(0), px(0) {}
    void make(int w_, int h_);
    void wrap(uint16_t* p, int w_, int h_) { px = p; w = w_; h = h_; store.clear(); }
};

struct Argb                        // straight-alpha picture, 0xAARRGGBB
{
    int w, h;
    std::vector<uint32_t> px;
    Argb() : w(0), h(0) {}
};

struct BakedFont;
typedef const BakedFont* FontP;
FontP font_small();
FontP font_ui();
FontP font_bold();
FontP font_big();

static inline uint16_t rgb565(uint32_t c) { return (uint16_t) (((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }
static inline uint32_t unpack565(uint16_t p) { return ((uint32_t) (p & 0xF800) << 8) | ((uint32_t) (p & 0x07E0) << 5) | ((uint32_t) (p & 0x001F) << 3); }

void g_fill(Surf& s, int x, int y, int w, int h, uint32_t rgb);
void g_pixel(Surf& s, int x, int y, uint32_t rgb);
void g_hline(Surf& s, int x0, int x1, int y, uint32_t rgb);
void g_vline(Surf& s, int x, int y0, int y1, uint32_t rgb);
void g_line(Surf& s, int x0, int y0, int x1, int y1, uint32_t rgb);
void g_blit_key(Surf& s, const Argb& a, int x, int y);                       // alpha >= 128 is drawn
void g_scale_blit(Surf& dst, const Surf& src, int zoom, int ox, int oy, int sx, int sy, int sw, int sh);   // nearest, integer zoom
void g_scale_smooth(Surf& dst, const Surf& src, int ox, int oy, int dw, int dh, int sx, int sy, int sw, int sh);   // bilinear, any size
void g_bevel(Surf& s, int x, int y, int w, int h, bool raised, uint32_t face);   // Windows-style 2 px bevel, filled with face (0xFFFFFFFF = no fill)
void g_text(Surf& s, FontP f, int x, int y, uint32_t rgb, const char* str);
void g_text_c(Surf& s, FontP f, int cx, int y, uint32_t rgb, const char* str);
int  g_text_w(FontP f, const char* str);
int  g_font_h(FontP f);

#endif
