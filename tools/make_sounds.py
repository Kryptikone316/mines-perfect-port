#!/usr/bin/env python3
"""Synthesises the seven sound effects of the SF3500 port (sine/noise, no samples from anywhere): assets/sound/<name>.wav
(48 kHz mono 16-bit). The game looks for exactly these names in ./sound (new, open, flag, error, deadend, won, lost)."""
import math, random, struct, wave, os
SR = 48000
random.seed(5)
out = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sound')
os.makedirs(out, exist_ok=True)

def env(n, a=0.004, r=None):
    """attack/exp-release envelope of n samples"""
    r = r or n / SR
    return [min(1.0, i / (a * SR)) * math.exp(-5.0 * i / (r * SR)) for i in range(n)]

def tone(freq, dur, gain=1.0, wave_='sin', f2=None, decay=None):
    n = int(dur * SR); e = env(n, r=decay or dur); s = []
    ph = 0.0
    for i in range(n):
        f = freq if f2 is None else freq + (f2 - freq) * i / n
        ph += 2 * math.pi * f / SR
        v = math.sin(ph) if wave_ == 'sin' else (1.0 if math.sin(ph) > 0 else -1.0) * 0.5 + 0.5 * math.sin(ph)
        s.append(v * e[i] * gain)
    return s

def noise(dur, gain=1.0, decay=None):
    n = int(dur * SR); e = env(n, a=0.001, r=decay or dur)
    return [(random.random() * 2 - 1) * e[i] * gain for i in range(n)]

def mix(*parts):
    n = max(len(p) for p in parts); o = [0.0] * n
    for p in parts:
        for i, v in enumerate(p): o[i] += v
    return o

def seq(*parts, gap=0.0):
    o = []
    for p in parts: o += p + [0.0] * int(gap * SR)
    return o

def save(name, s, peak=0.6):
    m = max(1e-9, max(abs(v) for v in s)); k = peak / m
    fade = int(0.004 * SR)
    for i in range(min(fade, len(s))): s[-1 - i] *= i / fade
    w = wave.open(os.path.join(out, name + '.wav'), 'wb')
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, v * k)) * 32767)) for v in s)); w.close()

# open: a small dry tick
save('open', mix(tone(900, 0.07, 0.8, decay=0.03), noise(0.02, 0.35, decay=0.01)))
# flag: a soft two-step "plip"
save('flag', seq(tone(520, 0.045, 0.8, decay=0.03), tone(780, 0.07, 0.8, decay=0.04)))
# error: low double buzz
save('error', seq(tone(120, 0.11, 0.9, 'sq', decay=0.12), tone(105, 0.14, 0.9, 'sq', decay=0.14), gap=0.03))
# new game: three rising blips
save('new', seq(tone(440, 0.07, 0.8, decay=0.05), tone(554, 0.07, 0.8, decay=0.05), tone(659, 0.14, 0.8, decay=0.1), gap=0.01))
# dead end: two mid beeps, distinct from the others
save('deadend', seq(tone(660, 0.09, 0.8, decay=0.07), tone(495, 0.14, 0.8, decay=0.1), gap=0.04))
# won: rising arpeggio ending on a held chord
c = [tone(f, 0.16, 0.7, decay=0.12) for f in (523.25, 659.25, 783.99)]
chord = mix(tone(523.25, 0.7, 0.55, decay=0.45), tone(659.25, 0.7, 0.55, decay=0.45), tone(783.99, 0.7, 0.55, decay=0.45), tone(1046.5, 0.7, 0.45, decay=0.45))
save('won', seq(*c, chord))
# lost: falling thud with a noise burst
save('lost', mix(tone(260, 0.8, 1.0, f2=45, decay=0.45), noise(0.25, 0.7, decay=0.12)))
print('sounds written to', os.path.abspath(out))
