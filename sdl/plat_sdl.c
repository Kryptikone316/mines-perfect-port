/* Platform layer on SDL2.
 *  - SF3500 build (-DSF3500): frames go to the device display driver (hwdisp), the pad is read from
 *    the shared joy_key memory (joykey), sound goes to the software mixer -> audio driver.
 *    SDL itself is only used for timing and screenshots there: this device has no usable SDL video
 *    or audio device (run it with SDL_VIDEODRIVER=dummy).
 *  - Desktop build: an SDL2 window, the keyboard, SDL audio.
 * Test hooks (both builds): MP_HEADLESS=1 (no window/driver), MP_SHOTS=<dir> + MP_SHOT_EVERY=<frames>,
 * MP_JOY="frame:mask,frame:mask" scripted pad (mask in hex or decimal), MP_MAXFRAMES=<n>, MP_SHOT_FRAMES="f1,f2,..." (save only those frames). */
#include "plat.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#ifdef SF3500
#include "hwdisp.hpp"
#include "joykey.hpp"
#include "sf3500_mixer.hpp"
#endif

#define NSND 16

static uint16_t* g_fb;
static int g_fps = 50, g_headless, g_shot_every, g_maxframes;
static unsigned g_frame, g_last_ticks;
static const char* g_shots;
static char g_script[2048];
static unsigned g_shot_list[64];
static int g_nshot_list;
static SDL_Surface* g_surf;

#ifdef SF3500
static Sf3500Voice* g_voice[NSND];
static int g_mixer_ok;
static Sf3500Voice* g_stream;
static PlatFill g_stream_fill;
static void* g_stream_ud;
static int stream_pull(void* ud, short* dst, int frames) { (void)ud; return g_stream_fill(g_stream_ud, dst, frames); }
#else
static SDL_Window* g_win;
static SDL_Renderer* g_ren;
static SDL_Texture* g_tex;
static SDL_AudioDeviceID g_adev;
typedef struct { const short* pcm; int frames; volatile int pos; volatile float gain; volatile int on; } HVoice;
static HVoice g_hv[NSND];
static unsigned g_keys_held;
static struct { PlatFill fill; void* ud; volatile int on; volatile float gain; } g_hstream;
#endif

uint16_t* plat_fb(void) { return g_fb; }
int plat_headless(void) { return g_headless; }
unsigned plat_frame(void) { return g_frame; }
void plat_set_fps(int fps) { g_fps = fps < 5 ? 5 : (fps > 120 ? 120 : fps); }
unsigned plat_ticks(void) { return SDL_GetTicks(); }

#ifndef SF3500
static void audio_cb(void* ud, Uint8* stream, int len) {
    short* out = (short*)stream;
    int n = len / 4, i, v;
    (void)ud;
    memset(stream, 0, (size_t)len);
    for (v = 0; v < NSND; v++) {
        HVoice* h = &g_hv[v];
        if (!h->on || !h->pcm) continue;
        for (i = 0; i < n && h->pos < h->frames; i++, h->pos++) {
            int l = out[i * 2] + (int)(h->pcm[h->pos * 2] * h->gain), r = out[i * 2 + 1] + (int)(h->pcm[h->pos * 2 + 1] * h->gain);
            out[i * 2] = (short)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
            out[i * 2 + 1] = (short)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
        }
        if (h->pos >= h->frames) h->on = 0;
    }
    if (g_hstream.on && g_hstream.fill) {            /* the music (48 kHz stereo here; the device mixer resamples other rates) */
        short tmp[2048 * 2];
        int done = 0;
        while (done < n) {
            int want = n - done > 2048 ? 2048 : n - done, got = g_hstream.fill(g_hstream.ud, tmp, want);
            for (i = 0; i < got * 2; i++) {
                int m = out[done * 2 + i] + (int)(tmp[i] * g_hstream.gain);
                out[done * 2 + i] = (short)(m > 32767 ? 32767 : (m < -32768 ? -32768 : m));
            }
            done += got;
            if (got < want) { g_hstream.on = 0; break; }
        }
    }
}
#endif

int plat_init(void) {
    const char* s;
    g_headless = getenv("MP_HEADLESS") && atoi(getenv("MP_HEADLESS"));
    g_shots = getenv("MP_SHOTS");
    g_shot_every = getenv("MP_SHOT_EVERY") ? atoi(getenv("MP_SHOT_EVERY")) : 0;
    g_maxframes = getenv("MP_MAXFRAMES") ? atoi(getenv("MP_MAXFRAMES")) : 0;
    if ((s = getenv("MP_SHOT_FRAMES"))) {      /* "120,400,900": only those frames are saved */
        while (*s && g_nshot_list < 64) { char* e; g_shot_list[g_nshot_list++] = (unsigned)strtoul(s, &e, 10); if (e == s) break; s = *e ? e + 1 : e; }
    }
    if ((s = getenv("MP_JOY"))) snprintf(g_script, sizeof g_script, "%s", s);
    g_fb = (uint16_t*)calloc((size_t)SCR_W * SCR_H, sizeof(uint16_t));
    if (!g_fb) return -1;
    g_surf = SDL_CreateRGBSurfaceWithFormatFrom(g_fb, SCR_W, SCR_H, 16, SCR_W * 2, SDL_PIXELFORMAT_RGB565);
#ifdef SF3500
    if (SDL_Init(SDL_INIT_TIMER) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return -1; }
    setpriority(PRIO_PROCESS, 0, -10);      /* the card's background scripts wake up often; frames stay even */
    if (!g_headless) {
        if (hwdisp_init() != 0) { fprintf(stderr, "gweled: hwdisp_init failed\n"); return -1; }
        joykey_init();
        g_mixer_ok = sf3500_mixer_init();
        if (!g_mixer_ok) fprintf(stderr, "gweled: mixer/hwaudio unavailable, sound off\n");
    }
#else
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS | (g_headless ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO))) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return -1; }
    if (!g_headless) {
        SDL_AudioSpec want, have;
        g_win = SDL_CreateWindow("Gweled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCR_W * 3 / 2, SCR_H * 3 / 2, SDL_WINDOW_RESIZABLE);
        g_ren = SDL_CreateRenderer(g_win, -1, 0);
        SDL_RenderSetLogicalSize(g_ren, SCR_W, SCR_H);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SCR_W, SCR_H);
        memset(&want, 0, sizeof want);
        want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
        g_adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (g_adev) SDL_PauseAudioDevice(g_adev, 0);
    }
#endif
    g_last_ticks = SDL_GetTicks();
    return 0;
}

static void save_shot(void) {
    char path[512];
    snprintf(path, sizeof path, "%s/frame%06u.bmp", g_shots, g_frame);
    SDL_SaveBMP(g_surf, path);
}

void plat_present(void) {
    unsigned now, target;
#ifdef SF3500
    if (!g_headless) hwdisp_present(g_fb, SCR_W, SCR_H, SCR_W * 2);
#else
    if (!g_headless) {
        SDL_UpdateTexture(g_tex, NULL, g_fb, SCR_W * 2);
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
    }
#endif
    g_frame++;
    if (g_shots && g_shot_every > 0 && g_frame % (unsigned)g_shot_every == 0) save_shot();
    if (g_shots && g_nshot_list) { int k; for (k = 0; k < g_nshot_list; k++) if (g_shot_list[k] == g_frame) save_shot(); }
    if (g_maxframes && g_frame >= (unsigned)g_maxframes) plat_quit(0);
    now = SDL_GetTicks();
    target = g_last_ticks + 1000u / (unsigned)g_fps;
    if (!g_headless && now < target) SDL_Delay(target - now);
    g_last_ticks = SDL_GetTicks();
}

/* "frame:mask,frame:mask": from that frame on the pad reads as mask until the next entry. */
static unsigned scripted(void) {
    static unsigned cur;
    static const char* p;
    static int init;
    if (!init) { init = 1; p = g_script; }
    while (p && *p) {
        char* end;
        unsigned long fr = strtoul(p, &end, 10);
        if (*end != ':' || fr > g_frame) break;
        cur = (unsigned)strtoul(end + 1, &end, 0);
        p = (*end == ',') ? end + 1 : end;
    }
    return cur;
}

/* test hook MP_FUZZ=<seed>: a random but human-ish pad (d-pad taps, A, B; never START/SELECT) */
static unsigned fuzz(void) {
    static unsigned cur, rs, next;
    if (!rs) rs = (unsigned)atoi(getenv("MP_FUZZ")) * 2654435761u + 1;
    if (g_frame >= next) {
        static const unsigned k[8] = { 0, 0, B_LEFT, B_RIGHT, B_UP, B_DOWN, B_A, B_B };
        rs = rs * 1664525u + 1013904223u;
        cur = (cur == 0) ? k[(rs >> 20) & 7] : 0;           /* press, release, press, ... */
        if ((rs >> 12) % 9 == 0) cur = B_A;
        next = g_frame + 2 + ((rs >> 8) % 5);
    }
    return cur;
}

unsigned plat_buttons(void) {
    if (g_script[0]) return scripted();
    if (g_headless && getenv("MP_FUZZ")) return fuzz();
#ifdef SF3500
    return g_headless ? 0 : joykey_read();     /* joykey's order is the B_* order */
#else
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) plat_quit(0);
            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                unsigned bit = 0;
                switch (e.key.keysym.sym) {
                    case SDLK_LEFT: bit = B_LEFT; break;
                    case SDLK_RIGHT: bit = B_RIGHT; break;
                    case SDLK_UP: bit = B_UP; break;
                    case SDLK_DOWN: bit = B_DOWN; break;
                    case SDLK_z: bit = B_A; break;
                    case SDLK_x: bit = B_B; break;
                    case SDLK_c: bit = B_X; break;
                    case SDLK_v: bit = B_Y; break;
                    case SDLK_a: bit = B_L1; break;
                    case SDLK_s: bit = B_R1; break;
                    case SDLK_q: bit = B_L2; break;
                    case SDLK_w: bit = B_R2; break;
                    case SDLK_RETURN: bit = B_START; break;
                    case SDLK_RSHIFT: case SDLK_BACKSPACE: bit = B_SELECT; break;
                    case SDLK_ESCAPE: if (e.type == SDL_KEYDOWN) plat_quit(0); break;
                    default: break;
                }
                if (e.type == SDL_KEYDOWN) g_keys_held |= bit; else g_keys_held &= ~bit;
            }
        }
        return g_keys_held;
    }
#endif
}

void plat_snd_set(int id, const short* pcm, int frames) {
    if (id < 0 || id >= NSND) return;
#ifdef SF3500
    if (!g_mixer_ok) return;
    if (g_voice[id]) sf3500_voice_destroy(g_voice[id]);
    g_voice[id] = sf3500_voice_create_static(pcm, frames);
    if (g_voice[id]) sf3500_voice_set_relative(g_voice[id], true);
#else
    g_hv[id].pcm = pcm; g_hv[id].frames = frames; g_hv[id].on = 0;
#endif
}

void plat_snd_play(int id, float gain) {
    if (id < 0 || id >= NSND) return;
#ifdef SF3500
    if (!g_mixer_ok || !g_voice[id]) return;
    sf3500_voice_set_gain(g_voice[id], gain);
    sf3500_voice_play(g_voice[id]);
#else
    g_hv[id].gain = gain; g_hv[id].pos = 0; g_hv[id].on = 1;
#endif
}

int plat_stream_ok(void) {
#ifdef SF3500
    return g_mixer_ok;
#else
    return g_adev != 0;
#endif
}

int plat_stream_start(PlatFill fill, void* ud, int rate, float gain) {
    plat_stream_stop();
#ifdef SF3500
    if (!g_mixer_ok) return 0;
    g_stream_fill = fill; g_stream_ud = ud;
    g_stream = sf3500_voice_create_stream(stream_pull, NULL, rate, 2);
    if (!g_stream) return 0;
    sf3500_voice_set_relative(g_stream, true);
    sf3500_voice_set_looping(g_stream, false);
    sf3500_voice_set_gain(g_stream, gain);
    sf3500_voice_play(g_stream);
    return 1;
#else
    (void)rate;
    if (!g_adev) return 0;
    SDL_LockAudioDevice(g_adev);
    g_hstream.fill = fill; g_hstream.ud = ud; g_hstream.gain = gain; g_hstream.on = 1;
    SDL_UnlockAudioDevice(g_adev);
    return 1;
#endif
}

void plat_stream_gain(float gain) {
#ifdef SF3500
    if (g_stream) sf3500_voice_set_gain(g_stream, gain);
#else
    g_hstream.gain = gain;
#endif
}

int plat_stream_playing(void) {
#ifdef SF3500
    return g_stream && sf3500_voice_playing(g_stream);
#else
    return g_hstream.on;
#endif
}

void plat_stream_stop(void) {
#ifdef SF3500
    if (g_stream) { sf3500_voice_destroy(g_stream); g_stream = NULL; }
#else
    if (g_adev) SDL_LockAudioDevice(g_adev);
    g_hstream.on = 0; g_hstream.fill = NULL;
    if (g_adev) SDL_UnlockAudioDevice(g_adev);
#endif
}

void plat_quit(int code) {
#ifdef SF3500
    if (g_mixer_ok) sf3500_mixer_deinit();
    if (!g_headless) hwdisp_deinit();
#endif
    SDL_Quit();
    exit(code);
}
