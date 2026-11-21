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
rm -f "$OUT/minesperfect/licenses/Backgrounds-LICENCE.txt"     # belongs to the optional art pack
cp assets/sound/*.wav "$OUT/minesperfect/sound/"
# the extra boards of the original's binary distribution (data files, GPL like the rest)
cp "$ORIG"/mines140/boards/* "$OUT/minesperfect/boards/"
[ -f "$OUT/minesperfect/boards/PENTAGON.BMP" ] && mv "$OUT/minesperfect/boards/PENTAGON.BMP" "$OUT/minesperfect/boards/Pentagon.bmp"
# GPL: the corresponding source travels with the binary
tar -C . --exclude='./build-*' --exclude='./package' --exclude='./work' --exclude='./assets/backgrounds' --exclude='./package-art' --exclude='./.git' \
    --transform 's,^\./,minesperfect-sf3500/,' -cJf "$OUT/minesperfect/licenses/minesperfect-sf3500-source.tar.xz" .
[ -f "$ORIG/mines140_src.zip" ] && cp "$ORIG/mines140_src.zip" "$OUT/minesperfect/licenses/mines-perfect-1.4.0-original-source.zip"
chmod +x "$OUT/MinesPerfect.sh" "$OUT/minesperfect/run.sh" "$OUT/minesperfect/minesperfect.mipsel"
(cd "$OUT" && find . -type f -print0 | sort -z | xargs -0 md5sum)

# Optional art pack (separate download): backgrounds/*.bg + LICENCE.txt + install note, as a zip that is unpacked INTO the port folder
ART=package-art
rm -rf "$ART"; mkdir -p "$ART/pack/backgrounds"
cp assets/backgrounds/*.bg "$ART/pack/backgrounds/"
cp licenses/Backgrounds-LICENCE.txt "$ART/pack/backgrounds/LICENCE.txt"
cp package-src/ARTPACK-README.txt "$ART/pack/ARTPACK-README.txt"
(cd "$ART/pack" && python3 -c "
import zipfile, os
z = zipfile.ZipFile('../MinesPerfectArtPack.zip', 'w', zipfile.ZIP_DEFLATED)
for root, _, files in sorted(os.walk('.')):
    for f in sorted(files): z.write(os.path.join(root, f), os.path.join(root, f)[2:])
z.close()")
(cd "$ART" && md5sum MinesPerfectArtPack.zip && ls -l MinesPerfectArtPack.zip)
