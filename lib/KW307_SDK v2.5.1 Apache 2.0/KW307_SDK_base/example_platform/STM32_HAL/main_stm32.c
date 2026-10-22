/******************************************************************************
 * KW307 SDK - STM32 (CubeMX HAL) Minimal Example
 *
 * Presence-triggered application skeleton:
 *   1. on_radar() callback latches the radar's human_flag into a volatile flag.
 *   2. main loop reads the flag and runs the user-supplied TODO branches.
 *
 * Keep the callback SHORT (it runs in the SDK's UART RX path). Put your
 * reaction code (LED / relay / timer / message send / ...) inside the
 * `if (g_human_flag)` branch in the main loop, NOT inside on_radar().
 *
 * CubeMX prerequisites:
 *   - USARTx enabled (baud 115200, 8-N-1, RX IT enabled)
 *   - MX_USARTx_UART_Init() called by the generated main()
 *   - stm32xxxx_it.c USARTx_IRQHandler() calls HAL_UART_IRQHandler(&huartx)
 *
 * Build:
 *   KW307_SDK/core/KW307_io.c / KW307_api.c / KW307_cmd.c / KW307_protocol.c / KW307_uart.c
 *   KW307_SDK/example_platform/STM32_HAL/port_uart.c
 *
 * Customise:
 *   KW307_SDK/example_platform/STM32_HAL/user_config.h  - point
 *     KW307_STM32_UART_NAME at your huartX instance
 *
 * To integrate into your CubeMX-generated main.c, copy only the blocks
 * marked USER CODE below into the matching USER CODE sections.
 *****************************************************************************/

#include "main.h"            /* CubeMX-generated */
#include "KW307_api.h"


/* Scene preset selector — change DEMO_SCENE to retarget the curated
 * configuration applied before the main loop. */
#define DEMO_SCENE_OUTDOOR     0   /* FOV 120, 12 m, hold  5 s              */
#define DEMO_SCENE_OFFICE      1   /* FOV 120, 10 m, hold 11 s              */
#define DEMO_SCENE_DESK        2   /* FOV 120,  2 m, hold 11 s              */
#define DEMO_SCENE_URINAL      3   /* FOV  60, 1.5 m, hold 1 s              */
#define DEMO_SCENE_HAND_DRYER  4   /* FOV  80,  1 m, fast response (byp_spd) */
#define DEMO_SCENE             DEMO_SCENE_OFFICE


/* ===== USER CODE BEGIN PV ===== */
/* Application-level state — set from the callback, read in main loop.
 * `volatile` because the callback runs from the SDK's RX path while main reads it. */
static volatile uint8_t  g_human_flag      = 0;
static volatile uint16_t g_target_dist_cm  = 0;   /* nearest moving-target distance, cm */
/* ===== USER CODE END PV ===== */


/* ===== USER CODE BEGIN 0 ===== */
/* Fires on every radar frame. Keep short — heavy work belongs in main loop. */
/* A moving slot is Valid at 1 in the simple reporting form — how the module boots.
   Switch the module with kw307_set_tracker_state_advanced_mode(true) and a moving
   slot is Valid at 3 instead, so change this to 3 as well. The stationary slot is
   Valid at 1 in
   both forms. Never test state != 0, and never decide on mag either: mag is signal
   strength, not a presence flag. */
#define MOVING_VALID  1u

static void on_radar(const KW307Frame_t* f)
{
    /* The frame carries the whole result: f->human_flag, both moving
       targets (f->moving[0], f->moving[1]) and the stationary target
       (f->stationary[0]). This demo uses only the nearest moving target;
       read the others the same way — a slot counts only when it is Valid
       (MOVING_VALID above). */

    g_human_flag     = f->human_flag ? 1u : 0u;
    g_target_dist_cm = (f->moving[0].state == MOVING_VALID)
                     ? f->moving[0].distance_cm
                     : 0u;                             /* 0 when no target */

    /* Beyond plain presence — your own rule goes here, where the whole frame is
       available. A person counts when a slot is Valid AND inside the area you care
       about, so latch that instead of (or alongside) the flag:

           g_in_zone = (f->moving[0].state      == MOVING_VALID &&
                        f->moving[0].distance_cm <= 300 &&        -- within 3 m
                        f->moving[0].angle_tenth >= -300 &&       -- -30 .. +30 deg
                        f->moving[0].angle_tenth <=  300) ? 1u : 0u;

       then drive the LED from g_in_zone in the main loop. Copy out whatever fields
       you need here: f is only valid inside this callback. f->human_flag is the
       module's own decision with the hold time already applied
       (kw307_set_flag_hold_time), which is enough when the position does not matter. */
}
/* ===== USER CODE END 0 ===== */


int main(void)
{
    /* (a) CubeMX-generated init (DO NOT MODIFY). UART IRQ is enabled inside
     *     MX_USARTx_UART_Init() — so it's already armed before kw307_init(). */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();     /* or whichever USART you picked in CubeMX */

    /* ===== USER CODE BEGIN 2 ===== */
    /* (b) Open the SDK (reads UART settings from user_config.h). */
    if (kw307_init() != KW307_OK) {
        while (1) { /* init failed — halt for debugger */ }
    }

    /* (c) Optional but recommended: verify MCU FW version matches this SDK's
     *     pairing (300 ms = UART response timeout). 0=match, 1=mismatch, <0=link error. */
    kw307_check_fw_version(300);

    /* (d) Apply scene preset — switch{} below applies a curated radar configuration. Edit
     *     DEMO_SCENE at top of file to retarget.
     *     The raw kw307_set_parameter() writes are firmware-version specific. */
    kw307_set_output_default();
    switch (DEMO_SCENE) {
    case DEMO_SCENE_OUTDOOR:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (50, 1200);       /* 0.5~12 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (6, 0);
        kw307_set_stationary_sensitivity(3);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (5);           /* presence flag hold 5 s */
        kw307_set_max_stationary_time(1, 5);           /* still cap on, 5 s */
        kw307_set_force_stationary   (false, 150, 40); /* off */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    case DEMO_SCENE_OFFICE:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (50, 1000);       /* 0.5~10 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (2, 0);
        kw307_set_stationary_sensitivity(1);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (11);          /* presence flag hold 11 s */
        kw307_set_max_stationary_time(1, 20);          /* still cap on, 20 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(true,  300, 20);
        break;
    case DEMO_SCENE_DESK:
        kw307_set_gain               (3);              /* +6 dB */
        kw307_set_detect_range       (50, 200);        /* 0.5~2 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (0, 0);
        kw307_set_stationary_sensitivity(0);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (11);          /* presence flag hold 11 s */
        kw307_set_max_stationary_time(1, 120);         /* still cap on, 120 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(true,  300, 20);
        break;
    case DEMO_SCENE_URINAL:
        kw307_set_gain               (3);              /* +6 dB */
        kw307_set_detect_range       (10, 150);        /* 0.1~1.5 m */
        kw307_set_detect_fov         (60, 5);          /* 60 deg cone, -5 deg/step */
        kw307_set_motion_sensitivity    (1, 0);
        kw307_set_stationary_sensitivity(1);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (1);           /* presence flag hold 1 s */
        kw307_set_max_stationary_time(1, 60);          /* still cap on, 60 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    case DEMO_SCENE_HAND_DRYER:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (10, 100);        /* 0.1~1 m */
        kw307_set_detect_fov         (80, 5);          /* 80 deg cone, -5 deg/step */
        kw307_set_motion_sensitivity    (0, 1);   /* L0 + speed-gate bypass (react to any motion) */
        kw307_set_stationary_sensitivity(1);
        kw307_set_mag_threshold   (200, 5000);      /* floor: moving 200 / still 5000 (muted) */
        kw307_set_moving_tracker_cnt(20, 2);    /* life 20f, show 2 */
        kw307_set_stationary_tracker_cnt(64, 2);    /* life 64f, show 2 */
        kw307_set_flag_hold_time (1);           /* presence flag hold 1 s */
        kw307_set_max_stationary_time(1, 10);          /* still cap on, 10 s */
        kw307_set_force_stationary   (false, 150, 40); /* off */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    }

    /* (e) Register the frame callback. */
    kw307_on_frame(on_radar);
    /* ===== USER CODE END 2 ===== */

    /* ===== USER CODE BEGIN WHILE ===== */
    /* (f) Main loop — drain RX and run the application reaction. */
    while (1) {
        kw307_update();

        if (g_human_flag) {
            /* TODO: target detected — your reaction here.
             *   e.g. HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
             *   Nearest target distance is in g_target_dist_cm.
             *   For a range / angle limited rule, gate on Valid in on_radar()
             *   instead of using g_human_flag — see the note there. */
        } else {
            /* TODO: no target — idle / clear output.
             *   e.g. HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); */
        }
        /* ===== USER CODE END WHILE ===== */
        /* ===== USER CODE BEGIN 3 ===== */
    }
    /* ===== USER CODE END 3 ===== */
}
