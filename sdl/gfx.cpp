// gfx.cpp - see gfx.h
#include "gfx.h"
#include "font_data.h"
#include <string.h>
#include <stdlib.h>

FontP font_small() { return &bf_ui_small; }
FontP font_ui()    { return &bf_ui; }
FontP font_bold()  { return &bf_ui_bold; }
FontP font_big()   { return &bf_ui_big; }

void Surf::make(int w_, int h_)
{
    w = w_; h = h_;
    store.assign((size_t) w * h, 0);
    px = store.empty() ? 0 : &store[0];
}

void g_fill(Surf& s, int x, int y, int w, int h, uint32_t rgb)
{
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > s.w) x1 = s.w;
    if (y1 > s.h) y1 = s.h;
    if (x >= x1 || y >= y1) return;
    uint16_t c = rgb565(rgb);
    for (int j = y; j < y1; j++)
    {
        uint16_t* d = s.px + (size_t) j * s.w + x;
        for (int i = x; i < x1; i++) *d++ = c;
    }
}

void g_pixel(Surf& s, int x, int y, uint32_t rgb)
{
    if ((unsigned) x < (unsigned) s.w && (unsigned) y < (unsigned) s.h) s.px[(size_t) y * s.w + x] = rgb565(rgb);
}

void g_hline(Surf& s, int x0, int x1, int y, uint32_t rgb)
{
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    g_fill(s, x0, y, x1 - x0 + 1, 1, rgb);
}

void g_vline(Surf& s, int x, int y0, int y1, uint32_t rgb)
{
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    g_fill(s, x, y0, 1, y1 - y0 + 1, rgb);
}

void g_line(Surf& s, int x0, int y0, int x1, int y1, uint32_t rgb)
{
    if (x0 == x1) { g_vline(s, x0, y0, y1, rgb); return; }
    if (y0 == y1) { g_hline(s, x0, x1, y0, rgb); return; }
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        g_pixel(s, x0, y0, rgb);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void g_blit_key(Surf& s, const Argb& a, int x, int y)
{
    for (int j = 0; j < a.h; j++)
    {
        int yy = y + j;
        if ((unsigned) yy >= (unsigned) s.h) continue;
        const uint32_t* sp = &a.px[(size_t) j * a.w];
        uint16_t* dp = s.px + (size_t) yy * s.w;
        for (int i = 0; i < a.w; i++)
        {
            int xx = x + i;
            if ((unsigned) xx >= (unsigned) s.w) continue;
            if ((sp[i] >> 24) >= 128) dp[xx] = rgb565(sp[i]);
        }
    }
}

void g_scale_blit(Surf& dst, const Surf& src, int zoom, int ox, int oy, int sx, int sy, int sw, int sh)
{
    // copy src rect (sx,sy,sw,sh) to dst at (ox,oy) enlarged `zoom` times, clipped to dst
    for (int j = 0; j < sh; j++)
    {
        int dy0 = oy + j * zoom;
        if (dy0 >= dst.h) break;
        const uint16_t* sp = src.px + (size_t) (sy + j) * src.w + sx;
        for (int k = 0; k < zoom; k++)
        {
            int dy = dy0 + k;
            if ((unsigned) dy >= (unsigned) dst.h) continue;
            uint16_t* dp = dst.px + (size_t) dy * dst.w;
            if (zoom == 1)
            {
                int x0 = ox, n = sw, skip = 0;
                if (x0 < 0) { skip = -x0; n -= skip; x0 = 0; }
                if (x0 + n > dst.w) n = dst.w - x0;
                if (n > 0) memcpy(dp + x0, sp + skip, (size_t) n * 2);
            }
            else
            {
                for (int i = 0; i < sw; i++)
                {
                    int dx = ox + i * zoom;
                    uint16_t c = sp[i];
                    for (int m = 0; m < zoom; m++)
                        if ((unsigned) (dx + m) < (unsigned) dst.w) dp[dx + m] = c;
                }
            }
        }
    }
}

void g_bevel(Surf& s, int x, int y, int w, int h, bool raised, uint32_t face)
{
    // Windows 9x button/panel look: outer light/dark, inner light-grey/grey
    const uint32_t white = 0xFFFFFF, lgray = 0xDFDFDF, gray = 0x808080, dark = 0x000000;
    uint32_t tl_o = raised ? white : gray, tl_i = raised ? lgray : dark;
    uint32_t br_o = raised ? dark : white, br_i = raised ? gray : lgray;
    if (face != 0xFFFFFFFFu) g_fill(s, x, y, w, h, face);
    g_hline(s, x, x + w - 1, y, tl_o);              g_vline(s, x, y, y + h - 1, tl_o);
    g_hline(s, x + 1, x + w - 2, y + 1, tl_i);      g_vline(s, x + 1, y + 1, y + h - 2, tl_i);
    g_hline(s, x, x + w - 1, y + h - 1, br_o);      g_vline(s, x + w - 1, y, y + h - 1, br_o);
    g_hline(s, x + 1, x + w - 2, y + h - 2, br_i);  g_vline(s, x + w - 2, y + 1, y + h - 2, br_i);
}

int g_font_h(FontP f) { return f->height; }

int g_text_w(FontP f, const char* str)
{
    int w = 0;
    for (const unsigned char* p = (const unsigned char*) str; *p; p++)
        if (*p >= 32) w += f->glyphs[*p - 32].adv;
    return w;
}

void g_text(Surf& s, FontP f, int x, int y, uint32_t rgb, const char* str)
{
    int cr = (rgb >> 16) & 255, cg = (rgb >> 8) & 255, cb = rgb & 255;
    for (const unsigned char* p = (const unsigned char*) str; *p; p++)
    {
        if (*p < 32) continue;
        const BakedGlyph& g = f->glyphs[*p - 32];
        const uint8_t* a = f->alpha + g.off;
        for (int j = 0; j < g.h; j++)
        {
            int yy = y + g.yoff + j;
            if ((unsigned) yy >= (unsigned) s.h) continue;
            for (int i = 0; i < g.w; i++)
            {
                int xx = x + g.xoff + i;
                if ((unsigned) xx >= (unsigned) s.w) continue;
                int al = a[j * g.w + i];
                if (!al) continue;
                uint16_t* d = &s.px[(size_t) yy * s.w + xx];
                if (al >= 250) { *d = rgb565(rgb); continue; }
                uint32_t bg = unpack565(*d);
                int r = (((bg >> 16) & 255) * (255 - al) + cr * al) / 255;
                int gg = (((bg >> 8) & 255) * (255 - al) + cg * al) / 255;
                int b = ((bg & 255) * (255 - al) + cb * al) / 255;
                *d = rgb565((r << 16) | (gg << 8) | b);
            }
        }
        x += g.adv;
    }
}

void g_text_c(Surf& s, FontP f, int cx, int y, uint32_t rgb, const char* str)
{
    g_text(s, f, cx - g_text_w(f, str) / 2, y, rgb, str);
}

void g_dim(Surf& s)
{
    size_t n = (size_t) s.w * s.h;
    for (size_t i = 0; i < n; i++) s.px[i] = (s.px[i] >> 1) & 0x7BEF;    // half brightness
}

void g_scale_smooth(Surf& dst, const Surf& src, int ox, int oy, int dw, int dh, int sx, int sy, int sw, int sh)
{
    // maps the source rectangle onto a dw x dh rectangle at (ox,oy); 16.16 fixed point
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    std::vector<int> xi(dw), xf(dw);
    for (int x = 0; x < dw; x++)
    {
        long u = (((long) (2 * x + 1) * sw) << 15) / dw - (1 << 15);       // ((x+0.5)*sw/dw - 0.5) in 16.16
        if (u < 0) u = 0;
        long max_u = (long) (sw - 1) << 16;
        if (u > max_u) u = max_u;
        xi[x] = (int) (u >> 16); xf[x] = (int) ((u & 0xFFFF) >> 8);          // weight 0..255
    }
    for (int y = 0; y < dh; y++)
    {
        int dy = oy + y;
        if ((unsigned) dy >= (unsigned) dst.h) continue;
        long v = (((long) (2 * y + 1) * sh) << 15) / dh - (1 << 15);
        if (v < 0) v = 0;
        long max_v = (long) (sh - 1) << 16;
        if (v > max_v) v = max_v;
        int y0 = (int) (v >> 16), yf = (int) ((v & 0xFFFF) >> 8), y1 = y0 + 1 < sh ? y0 + 1 : y0;
        const uint16_t* r0 = src.px + (size_t) (sy + y0) * src.w + sx;
        const uint16_t* r1 = src.px + (size_t) (sy + y1) * src.w + sx;
        uint16_t* dp = dst.px + (size_t) dy * dst.w;
        for (int x = 0; x < dw; x++)
        {
            int dx = ox + x;
            if ((unsigned) dx >= (unsigned) dst.w) continue;
            int i0 = xi[x], i1 = i0 + 1 < sw ? i0 + 1 : i0, fx = xf[x];
            uint32_t a = r0[i0], b = r0[i1], c = r1[i0], d = r1[i1];
            uint32_t out = 0;
            // blend each of R(5) G(6) B(5) separately
            static const int shifts[3] = { 11, 5, 0 }, masks[3] = { 31, 63, 31 };
            for (int ch = 0; ch < 3; ch++)
            {
                int sh_ = shifts[ch], m = masks[ch];
                int pa = (a >> sh_) & m, pb = (b >> sh_) & m, pc = (c >> sh_) & m, pd = (d >> sh_) & m;
                int top = pa * (256 - fx) + pb * fx, bot = pc * (256 - fx) + pd * fx;
                int val = (top * (256 - yf) + bot * yf + (1 << 15)) >> 16;
                out |= (uint32_t) val << sh_;
            }
            dp[dx] = (uint16_t) out;
        }
    }
}
