# `pc_win32/` — Windows PC port

Reference port that targets Windows hosts (PC / IPC / embedded x86). Talks
to the KW307 module over a standard COM port (USB-Serial bridge / direct
UART pin header / USB CDC). No special driver needed — standard Win32 COM API.

Works with any Windows C/C++ compiler: **MSVC**, **MinGW**, **Clang/LLVM**,
**Embarcadero C++ Builder (RAD Studio)**.

## Files in this folder

| File | What it is | Edit? |
|---|---|---|
| `port_win32.c` | The 5 HAL functions, implemented against Win32 `CreateFile` / `ReadFile` / `WriteFile` | No — works as-is |
| `user_config.h` | COM port name + baud rate defaults | **Yes — set your COM port** |
| `main_demo.c` | Minimal console demo (presence-print on edge transitions) | Use as starting point for your app |
| `README.md` | This file | — |

## Quick start

### 1. Edit `user_config.h`

```c
#define KW307_DEFAULT_PORT_NAME   "COM3"      /* change to your COM port */
#define KW307_DEFAULT_BAUD_RATE   115200
```

Find your COM port in Device Manager → Ports (COM & LPT).

### 2. Build the demo

Add these C files to your build target:

| Path | Source layer |
|---|---|
| `KW307_SDK/core/KW307_io.c` | Runtime / IO |
| `KW307_SDK/core/KW307_api.c` | Feature / config |
| `KW307_SDK/core/KW307_cmd.c` | CMD encode |
| `KW307_SDK/core/KW307_protocol.c` | Wire decode |
| `KW307_SDK/core/KW307_uart.c` | Framing + CRC-16 |
| `KW307_SDK/example_platform/pc_win32/port_win32.c` | Win32 HAL (this port) |
| `KW307_SDK/example_platform/pc_win32/main_demo.c` | The demo entry point |

Include path (compiler `-I` flag, or RAD Studio Project Options →
C++ Compiler → Paths and Defines → Include path):

```
KW307_SDK/core
KW307_SDK/example_platform/pc_win32
```

Link against `Kernel32.lib` (standard Win32; usually automatic).

### 3. Run

```
> KW307_demo.exe
KW307 SDK PC demo (Win32) — opening COM port from user_config.h
    FW version matches SDK pairing (v2.3.0).
    Streaming radar frames — press Ctrl+C to exit.

[  0.4s] PRESENT  dist=  47 cm  angle= +1.2 deg  mag= 832   (fc=8)
[  3.1s] EMPTY                                              (fc=62)
[  5.7s] PRESENT  dist=  52 cm  angle= -0.5 deg  mag= 910   (fc=114)
```

## What `main_demo.c` does

```
kw307_init()                  → open the COM port set in user_config.h
kw307_check_fw_version(300)   → verify MCU FW version matches SDK pairing
see example_platform/<port>/main.c switch{}
                              → apply indoor preset (FOV 120°, range 10 m,
                                 11 s hold). Swap for OUTDOOR / DESK /
                                 URINAL / HAND_DRYER as needed.
kw307_on_frame(on_radar)      → register per-frame callback
while (1) kw307_update();     → drain RX, fire callback
```

The callback latches state into volatile flags; the main loop reads
them and prints presence transitions. **Heavy work belongs in the
main loop, not the callback** — same pattern as the MCU example ports.

## Compiler-specific notes

### Embarcadero C++ Builder (RAD Studio)

1. File → New → Console Application (C / C++).
2. Project → Add to Project → add the 7 `.c` files listed above.
3. Project → Options → C++ Compiler → Paths and Defines → Include
   path: add the two paths above.
4. Build (F9) and run. Run with the radar module plugged in via USB-
   Serial bridge or COM header.

### MSVC (Visual Studio / cl.exe)

```
cl /nologo /W3 /I KW307_SDK\core /I KW307_SDK\example_platform\pc_win32 ^
   KW307_SDK\core\KW307_io.c ^
   KW307_SDK\core\KW307_api.c ^
   KW307_SDK\core\KW307_cmd.c ^
   KW307_SDK\core\KW307_protocol.c ^
   KW307_SDK\core\KW307_uart.c ^
   KW307_SDK\example_platform\pc_win32\port_win32.c ^
   KW307_SDK\example_platform\pc_win32\main_demo.c ^
   /Fe:KW307_demo.exe
```

### MinGW-w64 / GCC

```
gcc -Wall -O2 ^
    -IKW307_SDK/core -IKW307_SDK/example_platform/pc_win32 ^
    KW307_SDK/core/KW307_io.c ^
    KW307_SDK/core/KW307_api.c ^
    KW307_SDK/core/KW307_cmd.c ^
    KW307_SDK/core/KW307_protocol.c ^
    KW307_SDK/core/KW307_uart.c ^
    KW307_SDK/example_platform/pc_win32/port_win32.c ^
    KW307_SDK/example_platform/pc_win32/main_demo.c ^
    -o KW307_demo.exe
```

## How the port works

The 5 functions in `core/KW307_port.h` are the only thing the SDK calls
into the platform — every other layer is portable C. This port maps
them onto Win32 COM port APIs:

| `KW307_port.h` function | Win32 implementation |
|---|---|
| `kw307_port_open(name, baud)` | `CreateFileW("\\\\.\\COMx")` → `GetCommState` / `SetCommState` (8N1, no flow) → `SetCommTimeouts` (non-blocking: `MAXDWORD / 0 / 0`) → `PurgeComm` |
| `kw307_port_close()`          | `PurgeComm` + `CloseHandle` |
| `kw307_port_send(buf, len)`   | `WriteFile` (blocking up to ~100 ms) |
| `kw307_port_recv(buf, max, ms)` | `ReadFile` drains up to `max` bytes (one syscall, instant return). When 0 bytes arrive and `ms > 0`, `Sleep(ms)` honours the blocking-read contract. Returns bytes received (>= 0), -1 on error. |
| `kw307_port_is_open()`        | Boolean check of the cached `HANDLE`. |

`COMMTIMEOUTS` are fully non-blocking (`ReadIntervalTimeout = MAXDWORD,
ReadTotalTimeoutMultiplier = ReadTotalTimeoutConstant = 0`), so the
read syscall never stalls — useful when the app wraps `kw307_update()`
in a thread that also holds a UI lock. Multi-byte `ReadFile` (up to
`max_len`) lets a single `kw307_update()` drain a whole frame, keeping
the pump above the MCU's 20 Hz × ~34 byte/frame ≈ 680 byte/s stream
rate. Demos and worker loops should sleep only when the pump returned
zero bytes — unconditional `Sleep(N)` would throttle RX below stream
rate and drop frames.

## Notes

- **One COM port per process.** The HAL keeps a single static `HANDLE`,
  so this port supports one KW307 device at a time. Open / close
  multiple times to switch ports — but not concurrently.
- **No threads.** The polling-driven `kw307_update()` model is
  single-threaded by default. If your app needs the GUI thread to stay
  responsive while RX runs, spawn a worker thread that calls
  `kw307_update()` in a loop and post results back to the GUI thread.
  (See `example_platform/Arduino_ESP32S3/` for a similar single-thread
  pattern.)
- **COM port enumeration** is the app's responsibility — Win32
  `EnumPorts` or scanning the registry under `HARDWARE\DEVICEMAP\
  SERIALCOMM` works; the SDK only opens the name you give it.
- **Baud rate** must match the KW307 MCU firmware (typically 115200).
  Wrong baud → `kw307_check_fw_version()` reports transport error.

## See also

- `example_platform/Nuvoton_M251/` — MCU bare-metal example for
  Nuvoton M251 (ISR-driven RX, single chip)
- `example_platform/STM32_HAL/` — STM32 via Cube/HAL UART
- `example_platform/Arduino_ESP32S3/` — Arduino HardwareSerial
- `example_platform/port_template/` — empty skeleton for new
  platforms not covered above
- `doc/QUICKSTART.md` — 5-minute SDK walk-through
