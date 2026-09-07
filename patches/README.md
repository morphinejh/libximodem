# ximodem patch series

The buildable Zimodem sketch is **not** stored in this tree. It is reconstructed
at build time by `scripts/patch_firmware.py`:

    external/zimodem/     pristine Zimodem, exactly as of external/COMMIT (never edited)
    patches/SERIES      this directory, applied in order
        |
        v
    build/vendor/firmware/   generated -- what gen_unity.py and the compiler read

Upstream pin: **`external/COMMIT`** (currently
`a24225cc1b6c219e6c0764a4e6b1a388356a1dd5`). Zimodem has no tagged release
carrying `INCLUDE_CMDRX16`; this is the commit the port was verified against.
Bump to the 4.0.3 tag once it exists (see "Updating upstream" below).

All firmware *behaviour* changes are guarded by `ZIMODEM_HOST` (defined in
`src/xi_prelude.h`, not patched in) so the patched tree is still a valid ESP32
build. Patches 0007-0008 are unguarded but behaviour-neutral on every toolchain
(a whitespace fix and an explicit cast that C++ would otherwise make implicitly).

## The patches

| # | File | Change | Why |
|---|------|--------|-----|
| 0001 | `zimodem.ino` | `#ifdef ZIMODEM_HOST` block after the `INCLUDE_*` defines: `#undef INCLUDE_SLIP`, `INCLUDE_PPP`, `INCLUDE_OTH_UPDATES` (and `INCLUDE_SSH` only under `XIMODEM_NO_SSH`) | No host lwIP netif for SLIP/PPP; OTA is meaningless for a library. SSH/TLS/PING are built by default. |
| 0001 | `zimodem.ino` | `class ZMode` gets inline empty `serialIncoming()`/`loop()` + a virtual dtor under `ZIMODEM_HOST` | Upstream only declares them; with no TU defining them GCC never emits `typeinfo for ZMode`. Every real mode overrides both, so inline bodies change nothing. |
| 0001 | `zimodem.ino` | `#include "xi_prototypes.h"` (guarded) after the mode-header includes | The Arduino IDE auto-generates free-function prototypes; the host unity build supplies an equivalent generated header (`scripts/gen_unity.py`). Placed after every firmware type is defined. |
| 0001 | `zimodem.ino` | `ZIMODEM_HOST` branch on the debug-console init line | Cosmetic, pairs with 0004. |
| 0002 | `pet2asc.h` | Add `#ifndef ZHEADER_PET2ASC_H` include guard | The file is `#include`d twice within one translation unit in the unity build. |
| 0003 | `proto_ping.ino` | Top guard `#if INCLUDE_PING` -> `#if INCLUDE_PING && !defined(ZIMODEM_HOST)` | The lwIP raw-ICMP `ping()` can't build on a host; `compat/ping.cpp` provides `ping()` over an unprivileged ICMP datagram socket (bounded TCP-connect fallback when the kernel disallows that). `INCLUDE_PING` stays defined so `AT+PING` and `proto_ping.h` are active. |
| 0004 | `zcommand.ino` | `showInitMessage()` `#if defined(ZIMODEM_HOST)`: `Zimodem v<ver> (ximodem host build)` / `sdk=ximodem-<ver>  cpu=host  heap=<n>k` instead of the ESP32 `chipid=`/`cpu@`/`tot=` fields | The ESP fields are fake on the host. `Zimodem` + version stay contiguous for client parsers; `ATI4` still returns bare `<ver>`. |
| 0004 | `zcommand.ino` | **Fixed, read-only network identity.** `loadConfig()` forces `wifiSSI=""`/`wifiPW=""` on the host; `showInitMessage()` and `ATI3`/`ATI9` report `WiFi.SSID()` (a constant `"ximodem-host"` from `compat/WiFi.cpp`) directly; `doWiFiCommand` scans as `ximodem-host (0)` and no-ops an SSID set (`ZOK`) | The host adapter *is* the connection and the user can't change it. Keeping `wifiSSI` empty means `connectWifi()` is never entered (from `loadConfig`, `checkReconnect`, or a persisted config) and **`AT&W` can't save a name that wedges the next boot** (`connectWifi`'s `while(WiFi.status()==WL_CONNECTED) WiFi.disconnect();` spin is otherwise infinite on the host). Belt: `compat/WiFi::disconnect()` now makes the next `status()` report `WL_DISCONNECTED` once, so that spin also terminates on its own. |
| 0005 | `wifisshclient.h` | `#include "src/libssh2/..."` becomes `#if defined(XIMODEM_SYSTEM_LIBSSH2)` -> `<libssh2.h>` else vendored | With a real libssh2 (MSYS2 UCRT64, a Linux `-dev`), use *its* header so `libssh2_socket_t` (= `SOCKET` on Windows) matches the linked lib's ABI. CMake sets the define. |
| 0006 | `wifisshclient.ino` | `WiFiSSHClient::connect(IPAddress,uint16_t)` -- add missing `return true;` | Upstream falls off the end (UB). On ESP32 the garbage return was truthy; on x86-64 it read `false`, so `ATDS"user:pass@host"` reported failure after a successful handshake+auth+shell. |
| 0006 | `wifisshclient.ino` | `close(sock)` -> `xi_closesocket(sock)`; `fd()` returns `(int)sock` | Windows: `closesocket()`, and `libssh2_socket_t` is 64-bit `SOCKET`. `xi_closesocket` comes from `compat/xi_platform.h`. |
| 0007 | `wificlientnode.ino` | `void WiFiClientNode:: setNoDelay` -> `WiFiClientNode::setNoDelay` (drop the stray space) | The space made `scripts/gen_unity.py` classify the out-of-line member definition as a free function and emit a file-scope prototype. Clang rejects that declaration ("out-of-line declaration of a member must be a definition"); GCC only warns under `-fpermissive`. The generator now also drops any `::`-qualified match -- this removes the trigger at the source. |
| 0008 | `proto_ftp.ino` | `char *end = strrchr(remotepath, '/')` -> `(char *)strrchr(...)` | `FTPHost::fixPath` takes `const char *remotepath`; in C++ `strrchr(const char*)` returns `const char*`. GCC drops the const with a warning under `-fpermissive`, Clang errors. Explicit cast matches the existing `strchr((char *)vbuf, ...)` style in this file; no behaviour change. |

Deliberately **not** carried from the old hand-patched tree: a
`zcommand.ino` change from `checkPhonebookEntry(colon+1)` to
`checkPhonebookEntry(colon)` in `doPhonebookCommand`. Origin unclear, not on any
path the tests exercise (direct `ATDS"user:pass@host:port"` goes through
`doSSHCommand`, not the phonebook), and it looked like an abandoned experiment.
Re-add it as its own patch if a phonebook-SSH regression turns up.

## Updating upstream

    # 1. drop in the new snapshot
    curl -sSL https://github.com/bozimmerman/zimodem/archive/<ref>.tar.gz | tar xz
    rm -rf external/zimodem && cp -r Zimodem-<ref>/zimodem external/zimodem
    printf '<full-sha>\n' > external/COMMIT

    # 2. re-vendor -- fails loudly on the first patch that no longer applies
    python3 scripts/patch_firmware.py

    # 3. for each rejected patch: hand-fix build/vendor/firmware/<file>, then
    diff -u external/zimodem/<file> build/vendor/firmware/<file> \
      | sed -e '1s|^--- external/zimodem/|--- a/|' -e '2s|^+++ build/vendor/firmware/|+++ b/|' \
      > patches/000N-....patch

    # 4. rebuild + run the standalone tools
    make clean && make && (cd build && ./ximodem-smoke && ./ximodem-dialtest)

`patch_firmware.py` normalises line endings to LF on copy (upstream ships CRLF), so
patch context matches regardless of how the snapshot was obtained.
