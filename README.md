# ximodem

The [Zimodem](https://github.com/bozimmerman/Zimodem) ESP32 WiFi-modem firmware
(Commander X16 `INCLUDE_CMDRX16` build) compiled as a **host shared library**,
`libximodem`, so the X16 emulator can talk to a virtual Zimodem instead of a
physical ESP32 on a serial cable.

## Layout

| Path | What |
|------|------|
| `external/zimodem/` | Verbatim Zimodem source, pinned at `external/COMMIT` (never hand-edited) |
| `patches/` | Ordered patch series (`patches/SERIES`) applied on top of the snapshot at build time |
| `compat/` | Arduino / ESP32-core compatibility shim (Print/Stream/String/HardwareSerial/WiFi*/FS/…) |
| `src/xi_prelude.h` | Platform identity, included first in the unity TU |
| `src/generated/` | `unity.cpp` + `xi_prototypes.h`, produced by `scripts/gen_unity.py` (build output, not tracked) |
| `src/ximodem.cpp` | Public C-API glue: DTE byte rings, GPIO↔signal bridge, `setup()`/`loop()` pump |
| `include/ximodem.h` | The public C API |
| `scripts/` | `patch_firmware.py` (snapshot + patches → sketch) and `gen_unity.py` — run by CMake |

Test harnesses (`tools/`) and their fixtures (`ximodem-*-data/`) are local-only and
not tracked; see `.gitignore`. A code-only checkout builds just `libximodem`.

## Build

```sh
make                # release build -> build/libximodem.so
make debug          # debug build
make clean          # clean objects
make distclean      # remove build/ and src/generated/
```

Options: `make NO_SSH=1` (drop the SSH client), `make NO_TLS=1` (plaintext
`WiFiClientSecure`), `JOBS=N`. Or drive CMake directly:
`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`.

Requires cmake, a C++17 compiler, and python3. Optional: OpenSSL for real TLS
and libssh2 for the SSH client; both degrade gracefully if absent (plaintext
TLS / no SSH).

The standalone test harnesses (`ximodem-smoke`, `-dialtest`, `-pty`, …) live in
`tools/`, which is not part of this repo; `make test` is a no-op without it.

## Public API sketch

```c
ximodem_t *m = ximodem_create(&(ximodem_config_t){ .data_dir="./data", .verbose=1 });
for (;;) {
    ximodem_poll(m);                          // pump the firmware loop
    ximodem_write(m, from_x16, n);            // X16 -> modem
    int got = ximodem_read(m, to_x16, cap);   // modem -> X16
    int dcd = ximodem_get_signal(m, XIMODEM_SIG_DCD);
    ximodem_set_signal(m, XIMODEM_SIG_DTR, dtr_from_x16);
}
```

## Emulator integration

`emulator/x16-emulator-uart` builds libximodem as a subdirectory and selects it
with `-uart1 ximodem` (or `-uart1 ximodem:/path/to/datadir`). The TL16C2550
model now talks to an `IUartBackend` — `SerialLibBackend` for a real port,
`XimodemBackend` for the virtual modem — so a device path still works unchanged.

## Status

Working and verified:
- boot, AT command set, NTP clock, outbound TCP
- `ATDT"host:port"` — quote the host
- `AT&G` file download
- **TLS** (`ATDTs"host:443"`) and **SSH** (`ATDTs"user@host:22"`)
- inbound listeners `ATA`, DTR-drop hangup, RTS/CTS + buffer flow control
- modem-control lines, PTY bridge
