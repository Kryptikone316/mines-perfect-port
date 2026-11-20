#!/bin/bash
# Assembles the device package: package/MinesPerfect.sh + package/minesperfect/{run.sh,minesperfect.mipsel,README.txt,NOTICE.md,boards/,sound/,licenses/}
# Run ./build_sf3500.sh first; sounds: tools/make_sounds.py (assets/sound); box art: package-src/boxart.png (500x500).
set -e
cd "$(dirname "$0")"
OUT=package
ORIG=${ORIG:-$HOME/handhelds/Development/Datafrog/SF3500/ports/mines-perfect/Installer}
rm -rf "$OUT"; mkdir -p "$OUT/minesperfect/licenses" "$OUT/minesperfect/boards" "$OUT/minesperfect/sound"
cp package-src/MinesPerfect.sh "$OUT/"
cp package-src/{run.sh,README.txt,NOTICE.md} "$OUT/minesperfect/"
cp build-sf3500/minesperfect.stripped.mipsel "$OUT/minesperfect/minesperfect.mipsel"
[ -f package-src/boxart.png ] && cp package-src/boxart.png "$OUT/minesperfect/"
cp licenses/*.txt "$OUT/minesperfect/licenses/"
cp assets/sound/*.wav "$OUT/minesperfect/sound/"
# the extra boards of the original's binary distribution (data files, GPL like the rest)
cp "$ORIG"/mines140/boards/* "$OUT/minesperfect/boards/"
[ -f "$OUT/minesperfect/boards/PENTAGON.BMP" ] && mv "$OUT/minesperfect/boards/PENTAGON.BMP" "$OUT/minesperfect/boards/Pentagon.bmp"
# GPL: the corresponding source travels with the binary
tar -C . --exclude='./build-*' --exclude='./package' --exclude='./work' --exclude='./.git' \
    --transform 's,^\./,minesperfect-sf3500/,' -cJf "$OUT/minesperfect/licenses/minesperfect-sf3500-source.tar.xz" .
[ -f "$ORIG/mines140_src.zip" ] && cp "$ORIG/mines140_src.zip" "$OUT/minesperfect/licenses/mines-perfect-1.4.0-original-source.zip"
chmod +x "$OUT/MinesPerfect.sh" "$OUT/minesperfect/run.sh" "$OUT/minesperfect/minesperfect.mipsel"
(cd "$OUT" && find . -type f -print0 | sort -z | xargs -0 md5sum)
