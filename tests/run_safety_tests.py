"""Run the real OTA handler against a mock updater (Zig C++ or Windows MSVC).

Build firmware first with `pio run`, then run `python tests/run_safety_tests.py`.
No device connection or flash operations are performed.
"""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / ".pio" / "safety-tests"
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/main.cpp").read_text(encoding="utf-8")
handler = source[source.index("FirmwareValidation firmware;"):source.index("void setupRoutes()")]
cycles = source[source.index("constexpr int CYCLE_ADDR"):source.index("// Every deliberate reboot")]
harness = (root / "tests/ota_harness.cpp").read_text(encoding="utf-8")
harness = harness.replace("// OTA_IMPLEMENTATION", handler).replace("// CYCLE_IMPLEMENTATION", cycles)
(out / "ota.cpp").write_text(harness, encoding="utf-8")
vs = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio/2022"
scripts = list(vs.glob("*/VC/Auxiliary/Build/vcvars64.bat"))
zig = shutil.which("zig") or root / ".pio-tools/ziglang/zig.exe"
env = dict(os.environ)
if Path(zig).is_file():
    env["ZIG_LOCAL_CACHE_DIR"] = str(out / "zig-cache")
    env["ZIG_GLOBAL_CACHE_DIR"] = str(out / "zig-global-cache")
    command = [str(zig), "c++", "-std=c++17", "-Wall", "-Wextra",
               f"-I{root / 'include'}", str(out / "ota.cpp"), "-o", str(out / "ota.exe")]
else:
    if not scripts:
        raise SystemExit("Install Zig or MSVC C++ build tools to run these tests")
    (out / "env.cmd").write_text(f'@call "{scripts[0]}" >nul\n@set\n', encoding="utf-8")
    result = subprocess.run(["cmd", "/d", "/c", str(out / "env.cmd")],
                            capture_output=True, text=True, check=True)
    for line in result.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    compiler = shutil.which("cl", path=env.get("Path", env.get("PATH", "")))
    if not compiler:
        raise SystemExit("MSVC environment could not locate cl.exe: " + result.stderr)
    command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/W4",
               f"/I{root / 'include'}", str(out / "ota.cpp"),
               f"/Fe:{out / 'ota.exe'}", f"/Fo:{out / 'ota.obj'}"]
with (out / "compiler.log").open("w", encoding="utf-8") as log:
    result = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    print((out / "compiler.log").read_text(encoding="utf-8", errors="replace")[-6000:])
    raise SystemExit(result.returncode)
subprocess.run([str(out / "ota.exe"), str(root / ".pio/build/esp12e/firmware.bin")], check=True)

# Storage and recovery invariants that do not require flash hardware.
import re
setup = source[source.index("void setup()"):source.index("void loop()")]
ini = (root / "platformio.ini").read_text(encoding="utf-8")
assert "board_build.ldscript = eagle.flash.4m3m.ld" in ini, "layout must match stock (4m3m)"
assert "pre:scripts/build_options.py" in ini, "OTA size guard must run"
assert "LittleFS.begin" not in setup and "displayBegin" not in setup
assert setup.index("server.begin()") >= setup.index("startAP()")
assert setup.index("countPowerCycle()") < setup.index("startAP()")
assert "LittleFS.setConfig(LittleFSConfig(false))" in source
assert "WIFI_AP_STA" in source and "WIFI_STA)" not in source
assert "digitalRead(PIN_BUTTON)" not in source, "GPIO4 must not select recovery (no button fitted)"
assert "WiFi.setAutoReconnect(false)" in source and "STA_TRY_MS" in source
assert "U_FS" not in handler
# Every reboot except the one inside safeRestart() must go through safeRestart().
restarts = [m.start() for m in re.finditer(r"ESP\.restart\(\)", source)]
safe = source[source.index("void safeRestart()"):]
safe = safe[:safe.index("}")]
assert len(restarts) == 1 and "ESP.restart()" in safe, "use safeRestart() for deliberate reboots"
# The firmware image must fit the OTA budget enforced by build_options.py.
image = root / ".pio/build/esp12e/firmware.bin"
assert image.stat().st_size <= 0x7E000, "image exceeds OTA budget"
data = image.read_bytes()
fs = data.find((0x405FA000).to_bytes(4, "little"))
assert fs > 0 and (0x40300000).to_bytes(4, "little") in data[fs - 8:fs], "FS layout differs from stock"
# Float printf/scanf are dropped from the link (scripts/link_options.py), so no source
# may format or parse floats with them: %f/%e/%g would print nothing on the device.
assert "post:scripts/link_options.py" in ini
for f in list((root / "src").glob("*.cpp")) + list((root / "src").glob("*.h")):
    code = f.read_text(encoding="utf-8")
    assert not re.search(r'"[^"\n]*%[-+ #0-9.]*[eEfgG][^"\n]*"', code), f"float printf format in {f.name}"
    assert "scanf" not in code and "toFloat" not in code, f"float parsing in {f.name}"
# Every handler that changes the clock checks the sign-in first.
for name in ("handleApiSet", "handleConnect", "handleFormatStorage", "handlePhotoDelete", "handleWeatherSync",
             "handlePasswordChange", "handleSettingsExport", "handleSettingsImport", "handleSettingsReset",
             "handleWifiForget", "handleScanWifi"):
    body = source[source.index("void " + name + "()"):]
    body = body[:body.index("\n}\n")]
    assert "requireAuth()" in body, f"{name} does not require sign-in"
assert "if (!requestAuthorized()) { failOta(" in handler, "OTA upload must require sign-in"
assert "if (!requestAuthorized()) { photoError" in source, "photo upload must require sign-in"
print("PASS: stock-matching 4m3m layout, OTA size budget, no float printf, sign-in guards, non-formatting mount,")
print("      early recovery server, power-cycle/crash recovery (no button), safe restarts, STA backoff, persistent AP")
