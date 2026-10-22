# KW307 SDK

The **KW307** is a 24 GHz human-presence radar module, driven by a pure-C SDK
over UART — one include (`KW307_api.h`), multi-target tracking, and configurable
range, field of view, gain, sensitivity and hold time.

## Package layout

| Path | What it is |
|------|------------|
| `LICENSE.txt` | Licence terms — read before use |
| `README.md` | This file |
| `core/` | Portable C SDK source — add `core/*.c` to your build |
| `doc/` | User Guide (PDF) — protocol, API reference, application presets |
| `example_platform/` | Reference ports (STM32, Arduino ESP32-S3, Windows) + a porting skeleton |

## Quick start

You only need **one include** — `#include "KW307_api.h"`.

```c
#include "KW307_api.h"

static void on_radar(const KW307Frame_t* f) {
    bool present = f->human_flag;                 // 1 = someone present
    /* f carries the whole frame: 2 moving + 1 stationary target */
}

int main(void) {
    kw307_init();                // open the device once at startup
    kw307_on_frame(on_radar);    // register your frame callback
    for (;;) kw307_update();     // pump the receiver in your loop (>= 20 Hz)
}
```

Pick the port that matches your host from `example_platform/` (see
`example_platform/README.md`), copy it, and edit its `user_config.h`.

## Documentation

The full **User Guide** — UART protocol, output-frame format, complete API
reference, signal pipeline, and ready-made application presets — is in
[`doc/`](doc/) as a PDF.

**Using an AI coding assistant?** Point it at [`doc/AGENTS.md`](doc/AGENTS.md) first, or paste
the file into the conversation. It collects the behaviour that is not visible in the headers —
loop cadence, what a setter's return value does and does not promise, why `kw307_radar_init()`
is not "apply my settings", the mixed angle units — the things a model otherwise guesses at.

## Licence

Proprietary and confidential. See [`LICENSE.txt`](LICENSE.txt). This SDK is
licensed solely for developing products that incorporate KeywaveTech KW307
hardware.

© 2026 Keywave Technology Ltd. All rights reserved · <https://keywavetech.co.uk/>
