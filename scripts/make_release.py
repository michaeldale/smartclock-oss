"""Build the public release image: no WiFi details, stock-updater-friendly name.

    python scripts/make_release.py

Builds with SMARTCLOCK_RELEASE=1, which makes build_options.py ignore secrets.ini,
then refuses to finish if the image contains the SSID or password from a local
secrets.ini. Writes release/SDPro_SmartClockOSS_v<version>.bin (the stock update
page only accepts names starting with "SDP") and release/SHA256SUMS.txt.
"""
from pathlib import Path
import configparser
import hashlib
import os
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
version = re.search(r'#define FW_VERSION "([^"]+)"', (root / "src/main.cpp").read_text(encoding="utf-8")).group(1)
if version.endswith("-dev"):
    sys.exit(f"FW_VERSION is {version}; set a release version before building a release")

env = dict(os.environ, SMARTCLOCK_RELEASE="1")
pio = [sys.executable, "-m", "platformio"]
if (root / ".pio-tools" / "platformio").is_dir():
    env["PYTHONPATH"] = str(root / ".pio-tools") + os.pathsep + env.get("PYTHONPATH", "")
subprocess.run(pio + ["run", "-t", "clean"], cwd=root, env=env, check=True, stdout=subprocess.DEVNULL)
subprocess.run(pio + ["run"], cwd=root, env=env, check=True)

image = root / ".pio" / "build" / "esp12e" / "firmware.bin"
data = image.read_bytes()
header = (root / ".pio" / "build" / "esp12e" / "generated" / "wifi_secrets.h").read_text(encoding="utf-8")
if "DEFAULT_WIFI" in header:
    sys.exit("Build picked up WiFi defaults; release images must not contain them")
secrets = root / "secrets.ini"
if secrets.is_file():   # belt and braces: the local values must not be in the image
    cfg = configparser.ConfigParser(interpolation=None)
    cfg.read(secrets, encoding="utf-8")
    for key in ("ssid", "password"):
        value = cfg.get("wifi", key, fallback="").strip()
        if len(value) >= 3 and value.encode("utf-8") in data:
            sys.exit(f"Image contains the local WiFi {key}; not releasing")

out = root / "release"
out.mkdir(exist_ok=True)
name = f"SDPro_SmartClockOSS_v{version}.bin"
shutil.copy(image, out / name)
digest = hashlib.sha256(data).hexdigest()
(out / "SHA256SUMS.txt").write_bytes(f"{digest}  {name}\n".encode())   # LF, or `sha256sum -c` fails
print(f"{out / name}\n{len(data)} bytes, sha256 {digest}\nNo WiFi details in the image.")
