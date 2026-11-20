#!/bin/bash
# Cross build for the Datafrog SF3500 (MIPS32r2, little endian, hard float). Output: build-sf3500/minesperfect.stripped.mipsel
# Needs the toolchain from ~/handhelds/sf3000-work/buildroot (Codescape mips-mti-linux-gnu, glibc 2.20 sysroot with SDL2).
# Override with SF3500_TC=<buildroot/output/host>.
set -e
cd "$(dirname "$0")"
TC=${SF3500_TC:-$HOME/handhelds/sf3000-work/buildroot/output/host}
SYSROOT=$TC/mipsel-buildroot-linux-gnu/sysroot
CC=$TC/bin/mips-mti-linux-gnu-gcc
CXX=$TC/bin/mips-mti-linux-gnu-g++
ARCH="-mips32r2 -mtune=74kc -mhard-float -EL --sysroot=$SYSROOT"
# -O2 on purpose; no fast-math, no dspr2
CXXFLAGS="$ARCH -O2 -std=gnu++11 -fpermissive -w -Wno-write-strings -include core/compat.h -Icore -Igui -Isdl -Iplat -DSF3500"
CFLAGS="$ARCH -O2 -fno-strict-aliasing -Wall -Wno-unused-result -DSF3500 -Isdl -Iplat -I$SYSROOT/usr/include/SDL2"
OUT=${OUT:-build-sf3500}
mkdir -p "$OUT"
pids=()
for f in core/*.cpp gui/*.cpp sdl/api_sdl.cpp sdl/gfx.cpp sdl/ui.cpp sdl/main.cpp; do
  $CXX $CXXFLAGS -c "$f" -o "$OUT/$(echo "${f%.cpp}" | tr / _).o" & pids+=($!)
done
for f in hwdisp joykey hwaudio sf3500_mixer; do
  $CXX $ARCH -O2 -w -fno-exceptions -fno-rtti -Iplat -c plat/$f.cpp -o "$OUT/plat_$f.o" & pids+=($!)
done
$CC $CFLAGS -c sdl/plat_sdl.c -o "$OUT/plat_sdl.o" & pids+=($!)
for p in "${pids[@]}"; do wait "$p"; done
$CXX $ARCH "$OUT"/*.o -o "$OUT/minesperfect.mipsel" -lSDL2 -lpthread -ldl -lm
$TC/bin/mips-mti-linux-gnu-strip -o "$OUT/minesperfect.stripped.mipsel" "$OUT/minesperfect.mipsel"
ls -l "$OUT"/minesperfect*.mipsel
