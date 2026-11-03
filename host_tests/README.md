# host_tests

Unit tests for the parts of this project with no hardware/network
dependency: `framebuffer` (OLED pixel/text drawing), `web_content`
(the web server's string-building logic -- URL encode/decode, filename
safety, file-size formatting, HTML fragment rendering, firmware image
header checks) and `dir_scan`
(listing and counting directory entries, tested against a scratch
directory under the system temp dir). They build as a native executable
via ESP-IDF's `linux` target and run directly on this machine, no board
or flashing involved.

This is a separate `idf.py` project (its own `CMakeLists.txt`/`main`/
`sdkconfig`), not a subdirectory of the ESP32 app -- that keeps
`web_server`'s `espressif/mdns` dependency (which doesn't support the
`linux` target) out of this build entirely; `EXTRA_COMPONENT_DIRS` only
points at `framebuffer`, `web_content` and `dir_scan`, not the whole `components/`
directory.

## One-time setup

```bash
sudo apt-get install -y libbsd-dev   # ESP-IDF's linux-target port needs it
```

## Build and run

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
export PATH="/usr/bin:$PATH"   # put the host gcc/ld ahead of the Xtensa/ULP
                                # cross toolchain activate_idf puts on PATH --
                                # otherwise CMake picks up the wrong `ld`
cd host_tests
idf.py build
./build/ludo001_host_tests.elf
```

Exits non-zero (the Unity failure count) if any test fails.

## Adding a test

Pure logic only -- no `esp_*`/driver headers, no hardware. Add the test
function to the matching `test_*.cpp` (or a new one, registered in
`main/CMakeLists.txt`), forward-declare it and add a `RUN_TEST(...)` call
in `main/test_main.cpp`.
