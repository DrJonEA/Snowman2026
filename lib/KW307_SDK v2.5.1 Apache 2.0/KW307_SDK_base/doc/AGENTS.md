# AGENTS.md — notes for AI coding assistants

Instructions for an AI assistant (Claude Code, Cursor, Copilot, …) writing an application
against the **KW307** radar SDK. Everything here is behaviour you cannot infer from the
headers. Read it before writing code, and follow it over any guess.

The API itself is documented in `core/KW307_api.h` — read that too.

## The one rule to get right: test for Valid

**Use a target slot only when its state is Valid.** Which number that is depends on the
reporting form, which `kw307_set_tracker_state_advanced_mode()` selects:

| Reporting form | `moving[]` Valid | `stationary[]` Valid |
|---|---|---|
| **simple** — the default, how a module boots | `1` | `1` |
| **advanced** — for evaluation | `3` | `1` |

Everything else is a candidate or a suppressed slot and must not be displayed.

- **Never test `state != 0`.** It happens to equal Valid in the simple form only, so the bug
  stays invisible until someone switches the form.
- **Never test `mag != 0`** — `mag` is signal strength, not presence.
- **Never test `distance_cm != 0`** — in the advanced form an unconfirmed slot carries a real
  distance.

This table is repeated here because this file is often the only thing an assistant is given.
The full state model — what the in-between values mean, and why — is in `core/KW307_api.h` and
`core/KW307_protocol.h`. **If they ever disagree with this file, the headers are correct.**

---

## Build and wire-up

Compile these, all pure C99, no OS:

```
core/KW307_io.c  core/KW307_api.c  core/KW307_cmd.c  core/KW307_protocol.c  core/KW307_uart.c
example_platform/<your port>/port_uart.c     (+ user_isr.c if the port is ISR-driven)
```

Include paths: `core/` and `example_platform/<your port>/`.

- **The port and baud rate come from `user_config.h`**, not from a function argument. There is
  no `kw307_open("COM5")`. Copy the port folder closest to your hardware and edit that file.
- **Do not modify anything under `core/`.** It is regenerated for each release, so your edits
  are lost on the next drop. Put your code in your own files.
- **Do not write your own UART framing.** `kw307_uart.c` already adds the 5 × `0x00` wake-up
  preamble, the header and the CRC-16 to every frame. Sending your own wake bytes corrupts
  the frame.

## The main loop

`kw307_update()` is non-blocking; it drains the UART and fires your callback. Call it
**at least 20 times a second** — the reference GUI uses a 30 ms timer.

The port's RX buffer is 256 bytes (`KW307_UART_RX_BUFSIZE`) and the module streams a 27-byte
frame ~20×/s (≈540 B/s), so the buffer holds roughly nine frames: fall behind by ~450 ms and
frames are lost. Never `sleep()` in the loop for longer than that.

## Callback rules

Your `kw307_on_frame()` callback runs **inside** `kw307_update()`, on the same thread.

- **Do not call any `kw307_*` function from inside the callback.** The blocking calls
  (`kw307_read_blocking` and the setters that use it) would re-enter the receive path.
- Copy out what you need — the `KW307Frame_t*` is only valid for the duration of the call.
- Latch into a variable, act on it in the main loop. Keep the callback to assignments.
- The SDK is **single-threaded** by design. If your platform has threads, keep every
  `kw307_*` call — `kw307_update()` included — on one thread.

## What a setter's return value means

`KW307_OK` means **the command was transmitted**, not that the module accepted it. Arguments
are validated locally (`KW307_ERR_PARAM`), but nothing waits for the module's ACK.

```c
if (kw307_set_gain(5) == KW307_OK) { /* sent — NOT confirmed */ }
```

If the firmware rejects a value it answers a NACK, which you read afterwards with
`kw307_last_nack_status()` (`0x01` invalid value, `0x02` unknown CMD, `0x03` CRC, `0x04`
length). For a hard confirmation, read the setting back. `kw307_check_fw_version()` is the
exception — it is a blocking read and does return a real verdict.

## `kw307_radar_init()` is not "apply my settings"

This is the easiest thing to get backwards.

- **No setter needs it.** Settings take effect from the next frame. The reference GUI applies
  its entire panel — gain, range, FOV, both sensitivities, hold time, thresholds, tracker
  counters — with no init call anywhere.
- **It reloads the module's saved record, so it DISCARDS anything you set but did not save.**
  Calling it after configuring therefore reverts your configuration.

Treat it as a recovery action (~130 ms, blocking). To drop stale targets without touching
settings, use `kw307_clear_state()` instead — it is much cheaper and keeps your configuration.

## Persistence

`kw307_save_settings()` writes flash. **Do not call it in a loop or once per settings change**
— flash has a limited number of write cycles. Configure, then save once.

`kw307_set_gpio_human_flag()` is deliberately *not* persisted. And see above: a later
`kw307_radar_init()` reloads the saved record, so unsaved changes disappear.

## Units — they are not uniform

| Value | Unit |
|---|---|
| `distance_cm` (frame) | cm (the wire carries mm; the SDK divides by 10, truncating) |
| `angle_tenth` (frame) | **0.1°** — `-350` is −35.0° |
| `kw307_set_detect_range()` | cm |
| `kw307_set_detect_fov()` | **whole degrees** — pass `30`, not `300` |
| `kw307_set_force_stationary()` | cm and **whole degrees** |
| hold time / stationary time | seconds |
| tracker counters | frames (20 fps → 32 frames ≈ 1.6 s) |

The frame reports tenths of a degree while every setter takes whole degrees. Mixing these up
does not fail — it silently configures a 10×-wrong angle.

## Reading a frame

```c
KW307Frame_t: frame_index, human_flag, moving[2], stationary[1]
```

- `stationary` has **exactly one** element. `stationary[1]` is out of bounds — the symmetry
  with `moving[2]` is misleading.
- For plain presence, `human_flag` alone is enough: it is already debounced by the module
  (`kw307_set_flag_hold_time()`). Read the slots only when you need positions.
- In the simple reporting form an Empty slot has every field zeroed. In the advanced form a
  not-yet-confirmed slot carries a **real** distance, so `distance_cm != 0` is not a presence
  test either. Test Valid.
- `counter` runs 0..`cnt_max`, which you set with `kw307_set_moving_tracker_cnt()` /
  `kw307_set_stationary_tracker_cnt()` (defaults 32 moving, 64 stationary) — not 0..32 fixed.
  Scale any fade or confidence display against the value you configured.
- **`angle_tenth`: `0` is straight ahead, and the magnitude is the off-axis angle — but do not
  assume which side is positive.** KW307 ships in two chip packages whose antenna geometry is
  mirrored, so the same person at the same spot reports `+250` on one and `-250` on the other.
  The module does not tell you which one it is. So:
  1. Put a target clearly off to one side, note the sign, and write it down for that hardware.
  2. Keep **one** constant in *your* code — `#define MY_ANGLE_SIGN (+1)` or `(-1)` — and apply
     it in exactly one place, where you draw or make a left/right decision.
  3. Store and log the raw `angle_tenth`, not the flipped value, so captured data stays
     comparable across builds. (The Keywave evaluation GUI does the same: it negates the angle
     at draw time only.)

  Anything that only needs distance, presence, or "how far off-axis" is unaffected — this
  matters solely when left and right must be told apart.
- Writing your own transport instead of using `kw307_on_frame()`? `kw307_dec_frame()` requires
  `len == 27` exactly and returns `-1` otherwise.

## Power down

`kw307_set_power_down(true)` stops the stream and powers the RF down. Waking is done by a
UART byte from the host — and **that byte is consumed by the wake-up**, so do not assume the
first command you send afterwards was acted on. Send it, then confirm (read something back)
before relying on the module being configured.

## Firmware pairing

`kw307_check_fw_version(timeout_ms)` returns `0` compatible, `1` firmware older than this
SDK's paired baseline, `<0` link error. It is worth calling once at startup and **acting on
the result** — the bundled examples ignore it for brevity; do not copy that habit.

A module answering CMD `0x01` also proves the link works, so a failure here means wiring,
power or baud rate, not a bad setting.
