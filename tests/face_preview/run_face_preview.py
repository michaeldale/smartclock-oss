"""Render the clock faces on the host and write PNGs plus a contact sheet.

    python tests/face_preview/run_face_preview.py

Builds src/faces.cpp against the mocks in tests/face_preview/mock with Zig, using the
real TFT_eSPI fonts and tjpgd from .pio/libdeps (run `pio run` once first). Output:
.pio/face-preview/*.png and sheet.png. Exits non-zero if an incremental redraw
leaves pixels that a full redraw would not.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
out = root / ".pio" / "face-preview"
build = out / "build"
shutil.rmtree(out, ignore_errors=True)
build.mkdir(parents=True)
libs = root / ".pio" / "libdeps" / "esp12e"
tft, tjpg = libs / "TFT_eSPI", libs / "TJpg_Decoder" / "src"
if not tft.is_dir():
    raise SystemExit("Run `pio run` first so .pio/libdeps holds TFT_eSPI and TJpg_Decoder")

# faces.cpp includes config.h, whose load()/save() need ArduinoJson and LittleFS.
# The preview only needs the fields, so it gets a copy with those methods removed.
config = (root / "src" / "config.h").read_text(encoding="utf-8")
config = re.sub(r"\n  bool load\(\) \{.*?\n  \}\n", "\n", config, flags=re.S)
config = re.sub(r"\n  bool save\(\) \{.*?\n  \}\n", "\n", config, flags=re.S)
config = config.replace("#include <ArduinoJson.h>\n", "").replace("#include <LittleFS.h>\n", "")
(build / "config.h").write_text(config, encoding="utf-8")
for name in ("faces.cpp", "faces.h", "weather.h"):
    shutil.copy(root / "src" / name, build / name)
# tjpgd.h hand-rolls stdint types under _WIN32; Zig's headers already define them.
header = (tjpg / "tjpgd.h").read_text(encoding="utf-8").replace("#if defined(_WIN32)", "#if 0")
(build / "tjpgd.h").write_text(header, encoding="utf-8")
shutil.copy(tjpg / "tjpgdcnf.h", build / "tjpgdcnf.h")

# A sample photo for the Photo face.
photos = build / "photo"
photos.mkdir()
img = Image.new("RGB", (240, 240))
px = img.load()
for y in range(240):
    for x in range(240):
        px[x, y] = (40 + y // 2, 90 + x // 4, 160 - y // 3)
ImageDraw.Draw(img).ellipse((60, 40, 180, 160), fill=(250, 200, 80))
img.save(photos / "sample.jpg", quality=88)

zig = shutil.which("zig") or str(root / ".pio-tools" / "ziglang" / "zig.exe")
cache = root / ".pio" / "face-preview-cache"   # outside `out`, which is wiped each run
env = dict(os.environ, ZIG_LOCAL_CACHE_DIR=str(cache / "local"), ZIG_GLOBAL_CACHE_DIR=str(cache / "global"))
inc = [f"-I{build}", f"-I{here / 'mock'}", f"-I{tft}", f"-I{tjpg}"]
obj = build / "tjpgd.o"
subprocess.run([zig, "cc", "-c", "-O2", f"-I{tjpg}", str(tjpg / "tjpgd.c"), "-o", str(obj)], check=True, env=env)
exe = build / "preview.exe"
subprocess.run([zig, "c++", "-std=c++17", "-O2", "-Wall", "-Wno-nullability-completeness", *inc, str(here / "preview.cpp"), str(build / "faces.cpp"),
                str(here / "mock" / "TFT_eSPI.cpp"), str(obj), "-o", str(exe)], check=True, env=env)
# FACE_PREVIEW_PHOTOS=<dir> renders your own JPEGs (e.g. ones the settings page produced).
photo_dir = os.environ.get("FACE_PREVIEW_PHOTOS") or str(photos)
result = subprocess.run([str(exe), str(out)], env=dict(env, PHOTO_DIR=photo_dir))

frames = sorted(out.glob("*.raw"))
tiles = []
for raw in frames:
    data = raw.read_bytes()
    im = Image.new("RGB", (240, 240))
    im.putdata([((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                for v in (int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data), 2))])
    im.save(raw.with_suffix(".png"))
    raw.unlink()
    tiles.append((raw.stem, im))

cols, scale, label = 4, 1, 18
sheet = Image.new("RGB", (cols * 250, ((len(tiles) + cols - 1) // cols) * (240 + label + 10)), (60, 60, 60))
draw = ImageDraw.Draw(sheet)
for i, (name, im) in enumerate(tiles):
    x, y = (i % cols) * 250 + 5, (i // cols) * (240 + label + 10) + 5
    draw.text((x, y), name, fill=(230, 230, 230))
    sheet.paste(im, (x, y + label))
sheet.save(out / "sheet.png")
print(f"{len(tiles)} frames -> {out / 'sheet.png'}")
sys.exit(result.returncode)
