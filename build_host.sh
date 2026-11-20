#!/bin/bash
# Desktop build for development and tests: ./build_host.sh  ->  build-host/minesperfect
set -e
cd "$(dirname "$0")"
mkdir -p build-host
FLAGS="-std=gnu++11 -O2 -g -fpermissive -w -Wno-write-strings -include core/compat.h -Icore -Igui -Isdl $(sdl2-config --cflags)"
pids=()
for f in core/*.cpp gui/*.cpp sdl/api_sdl.cpp sdl/gfx.cpp sdl/ui.cpp sdl/main.cpp; do
  g++ $FLAGS -c "$f" -o "build-host/$(echo "${f%.cpp}" | tr / _).o" & pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done
gcc -O2 -g -Isdl $(sdl2-config --cflags) -c sdl/plat_sdl.c -o build-host/plat_sdl.o
g++ build-host/*.o -o build-host/minesperfect $(sdl2-config --libs) -lpthread -lm
echo built build-host/minesperfect
