# Snowman2026
An animatronic Snowman puppet based on a Raspberry Pi Pico 2 W. It tracks people with a mmWave radar, turns its head to look at them, and reacts with nods, shakes, waves and dances. It sleeps when nobody is about.

Main code is in the firmware folder.

## Cloning and Building
This is a Pico SDK based project in C++. Where libraries are included they are normally included as submodules. When cloning please recurse submodules.

I have a blog that explains how to clone repos and build my projects on
[drjonea.co.uk](https://drjonea.co.uk/2025/12/15/building-my-projects-from-repo/).

The firmware target is `snowman`, built from the `firmware` folder.

## BOM

- Brain: Raspberry Pi Pico 2 W
- Sensor: [Keywave Technology KW307](https://store.keywavetech.co.uk/drjonea) mmWave radar, on UART
- Head motion: 2 x servo (pan and tilt) on a pan tilt bracket
- Arms: 2 x servo
- Star: 7 LED WS2812B disc. LED 0 is the centre and LEDs 1 to 6 are the outer ring

Pins are defined in `firmware/src/ServoHead.h`.

## Behaviour
Everything is polled and non-blocking. Animations are step based (`Anim`) and run in parallel, and a hierarchical state machine decides which one runs.

### Radar target
The radar callback reduces each frame to a single `RadarTarget_t`, which is shared with the rest of the firmware:
- A stationary target wins if there is one, otherwise the nearest moving target.
- The distance is filtered per frame: a median of the last 5 frames to reject spikes (servo noise), then a light average.
- The callback runs from the UART path, so the target is handed to the main loop with a sequence counter and never read half written.

### Head tracking
- A stationary target is looked at with a slow turn (2 s, `ease_sin`). Small angle changes are ignored.
- A moving target is followed with fast turns (50 ms, `ease_in_expo`).
- With no target the head stays where it is.
- Animations are written around centre. While facing a target, a pan bias makes them relative to the direction faced.

### State machine
`StateMachine` is a small hierarchical state machine. Unhandled events bubble up to the parent state.

- **Asleep**: sleep pose, slow blue heartbeat. Wakes when a target appears.
- **Awake**
  - **WakingUp**: gentle rise to full attention.
  - **Idle**: faces a target, otherwise looks around now and then. Sleeps after a few cycles with nobody present.
  - **Looking**: looks up and around the room.
  - **Greeting** and **Entertaining**: a person comes near, then dances.
  - **Attending**: a stationary target is in front. Nods, shakes and waves like joining a conversation. The wave uses the hand opposite the target.

In Attending:
- Head shakes become much more likely when the target is under 1 m.
- The gap between gestures is shortest under 1 m and stretches to about 15 s at 3 m or more.

### Animations
| Class | Description |
|---|---|
| `ArmWave`, `ArmHug` | Arm gestures |
| `HeadNod`, `HeadShake` | Head gestures |
| `HeadLook` | Looks up and around, pausing at a few points |
| `HeadSleep` | Droops into a sleep pose, arms to rest |
| `WakeUp` | Gentle wake up to full attention |
| `Dance` | Ballet style dance with head and arms |
| `StarChase` | Three evenly spaced ring LEDs that fade round. Colour and speed can be set |
| `StarHeartbeat` | Centre LED fading in and out. Colour and speed can be set |

The chase colour follows the distance to the target: green at 2 m or more, through blue at 2 m to 1 m, to pure red under 0.7 m.

## Tuning
- Radar angle sign: `RADAR_TO_PAN_SIGN` in `SnowmanBrain.cpp`.
- Distance filter: `DIST_MEDIAN_LEN` and `DIST_EMA_ALPHA` in `main.cpp`.
- Tracking speeds and gesture timing: the defines at the top of `SnowmanBrain.cpp`.

## Folders
- `firmware`: the Snowman firmware
- `exp`: experiments, built up in stages (see below)
- `lib`: libraries (submodules and the KW307 SDK)
- `SkullHead2026`: the earlier skull project this tracking is based on

## Experiments
The `exp` folder holds small stand-alone projects. Each one builds on its own and was used to prove one part of the hardware before it went into the main firmware. They are roughly in the order I built them.

### boardTest
First power-up of the board. It initialises all five servo outputs (head pan and tilt, two arms and a spare) at 90 degrees, then loops forever. Each pass fills the Star with the next of eight colours and moves the four servos together to the next of a set of positions (90, 45, 0, 45, 90, 135, 180, 135 degrees) with an `ease_in_expo` move.

Use it to check wiring, servo direction and range, and the LEDs.

### centre
Centring tool for assembly. It sets all five servos to 90 degrees and then holds them there, with the Star cycling colours so you can see the board is alive. Run it before fitting horns and arms so that 90 degrees is the true neutral position.

### arms
The first animation framework: a polled, non-blocking `Anim` base class and the first animations built on it.

- `Anim`: a list of timed steps that is advanced by calling `poll()`, so several animations can run at once.
- `Arm` and `Head`: servo wrappers that move in radians, with limits. `Kinematics` (Eigen) gives the position of the hand or head from the joint angles.
- `ArmWave` and `ArmHug`, and `HeadNod` and `HeadShake`.

The main loop runs each animation in turn and also has a test that prints arm positions as the arms sweep.

### demoMovement
**The base animation library.** It builds on `arms` and adds everything the Snowman can do. All of the animations in the firmware started here, and this is the project shown in the first of the three videos.

The Star (the 7 LED WS2812B disc, centre LED first and then the six ring LEDs) is driven by two animations that run in parallel with the movements:
- `StarChase`: three evenly spaced ring LEDs that chase round and fade between positions. Colour and speed can be set.
- `StarHeartbeat`: the centre LED fades in and out like a heartbeat, continuously. Colour and speed can be set.

The movement animations are:

| Animation | What it does |
|---|---|
| Wave left, Wave right | Waves the arm |
| Hug | Both arms come forward |
| Nod | Nods the head |
| Shake | Shakes the head |
| Look | Looks up and around the room, stopping on a few points before returning to centre |
| Dance | A slow ballet style dance with head and arms |
| Sleep | Dozes off: the head droops, jerks and droops again, then slumps into a sleep pose with the arms at rest |
| Wake up | A gentle rise to full attention |

`main.cpp` runs them as a demo. It steps through the list one at a time and changes the chase and heartbeat colours before each one. Because each animation writes only its own servos or LEDs, the lights keep running underneath.

The animation classes were then copied to `firmware`, where the state machine chooses between them instead of this fixed list.

### kw307Int
Radar bring-up, with no servos. It initialises the KW307 mmWave radar over UART, checks the firmware version, applies a scene preset (outdoor, office, desk, urinal or hand dryer) and prints every frame: the human flag, the stationary target and both moving targets, each with distance, angle, signal strength, state and counter. The radar settings used by the main firmware started here.
