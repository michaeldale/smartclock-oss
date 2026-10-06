# PlatformIO post-script: drop link flags that force code we never use.
#
# The Arduino ESP8266 build passes `-u _printf_float -u _scanf_float`, which links
# newlib's float printf/scanf (and dtoa/strtod/mprec behind them), about 13 KB,
# whether or not anything formats a float. Nothing in this firmware uses %f/%g/%e
# with printf or scanf (ArduinoJson and TFT_eSPI format floats themselves), and the
# OTA budget needs the space. tests/run_safety_tests.py checks the sources for %f.
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)

flags = list(env["LINKFLAGS"])
for sym in ("_printf_float", "_scanf_float"):
    for i in range(len(flags) - 1):
        if flags[i] == "-u" and flags[i + 1] == sym:
            del flags[i:i + 2]
            break
env.Replace(LINKFLAGS=flags)
