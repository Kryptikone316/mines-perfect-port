// compat.h - force-included (-include core/compat.h) when the 2003 sources are built with a current compiler.
// Not part of the original Mines-Perfect; added for the SF3500 port (GPL-2.0-or-later like the rest).
#ifndef MP_COMPAT_H
#define MP_COMPAT_H

#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <strings.h>
#include <iostream>
#include <algorithm>

#ifndef _VARIANT_
#define _VARIANT_ 5            // own value for this port (1 Borland, 2 Dev-C++, 3 Visual C++ = the released 1.4.0 exe, 4 m42a's Linux port); only written into saved games, never read back
#endif

// Wall clock in milliseconds, so every clock() in the old code is wall time (clock() on Linux counts CPU time of all threads).
extern "C" long mp_clock_ms(void);
#define CLK_TCK 1000
#define clock() ((clock_t) mp_clock_ms())

#define stricmp strcasecmp
static inline char* itoa(int v, char* buf, int radix) { (void) radix; sprintf(buf, "%d", v); return buf; }

typedef unsigned long ulong;
#endif
#endif
