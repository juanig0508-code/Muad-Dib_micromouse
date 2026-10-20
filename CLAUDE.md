# CLAUDE.md

Firmware for **Muad-Dib**, a maze robot for the LNR (Liga Nacional de Robótica, Argentina) "Laberinto" category. It is a fork of OPRobots' ZoroBot3 micromouse.

- Team repo: https://github.com/juanig0508-code/Muad-Dib_micromouse (remote `upstream`). Personal fork: remote `origin`.
- Original robot: https://github.com/OPRobots/ZoroBot3. The fork point in this repo is commit `0235b39`, so `git diff 0235b39 HEAD -- source_code/src` shows everything the team changed.
- Rules: https://sistema.lnr-argentina.com.ar/reglamentos/Laberinto. The page renders client-side; the Markdown is embedded in the `data-page` JSON. A summary is in `docs/FSD.md` §2. Facts from the team that override the page:
  - The next tournament uses **25 cm cells only**.
  - `CELL_DIMENSION` is the **measured** pitch of the maze in use (practice maze: 260 mm).
  - There are **2 passes per configuration**; "5 pasadas" in §5.1.2 is an erratum.
  - The IR remote is a **test tool only**; an on-board start button is planned.
- Write `docs/ISSUES.md` and team-facing docs in **Spanish**.
- Functional spec: `docs/FSD.md`. Known issues: `docs/ISSUES.md`. Team change log and status (Spanish): `docs/Cambios_ZoroBot3_vs_MuadDib.docx`.

## Scope for reviews and changes

- Firmware lives in `source_code/src` and `source_code/include`.
- Ignore `3d_model/` and `images/`. `pcb_files/` is hardware only.
- `source_code/simulator/` is not used. Read it only if `src/` calls into it. Code under `#ifdef MMSIM_ENABLED` is the simulator path and is never compiled for the robot.
- `source_code/lib/mmsim_api` is simulator-only. `lib/lsm6dsr_reg` is the old IMU library and is unused (the robot has an MPU6500).

## Build and flash

PlatformIO, framework libopencm3, env `custom_STM32F405RGT6`. Run from `source_code/`:

```
pio run                      # build
pio run -t upload            # flash via ST-Link
pio device monitor -b 115200 # UART3 printf (monitor_port in platformio.ini is COM5, so override it on Linux)
```

There are no unit tests and no hardware-in-the-loop. The only checks are "it compiles" and bench tests on the robot. The numeric remote buttons 1–5 run isolated motion self-tests (see `main.c`). Setting `DEBUG_ZONE 1` in `main.c` builds a telemetry-only firmware.

## Hardware facts the code depends on

- STM32F405RGT6 at 168 MHz from a **24 MHz** crystal (`setup.c`, PLLM=24). The original board used 8 MHz.
- **MPU6500** gyro on SPI3, CS on PA15. Config writes need SPI ≤1 MHz, so the code switches SPI speed. Z-axis only.
- Encoders on TIM3 and TIM4 in quadrature mode. Motor PWM on TIM8 (MP6551). Fan on TIM1, unused (fan_speed = 0).
- 4 IR wall sensors: front-left, front-right, side-left, side-right. ADC2 injected channels are driven by an emitter on/off state machine in the TIM2 ISR.
- An IR receiver on EXTI15_10 calls `rc5_register()`. It is a generic frame-hash learner, not a real RC5 decoder.
- The RGB LED is **not populated** on this board. Menu feedback uses the 10 info LEDs and the status LED.
- Both motor directions are inverted in `motors.c` (`*_MOTOR_DIRECTION = -1`).

## Runtime architecture

| Context | Rate | What runs |
|---|---|---|
| TIM2 ISR (prio 0) | ~32 kHz | `sm_emitter_adc()`: sensor emitter on/off + ADC |
| SysTick (prio 1) | 1 kHz | clock, encoders, `update_sensors_magics()`, battery, LEDs, buttons + IR + Play/Pause, `mpu6500_update()` (SPI read) |
| TIM5 ISR (prio 3) | ~1 kHz (period 997 µs) | `control_loop()`: linear/angular PID, heading lock, wall-follow, front-wall PIDs, saturation cut-off |
| `main()` loop | — | menu / start logic, then the **blocking** navigation loop (`handwall_loop()` → `move()` → busy-wait `while (is_race_started() && …)`) |

Rules that follow from this:
- Motion primitives in `move.c` set targets (`set_target_linear_speed`, `set_ideal_angular_speed`) and **busy-wait**. The ISR does the control. Any new wait loop must check `is_race_started()` and `!is_motor_saturated()` so that Play/Pause and the crash cut-off can abort it.
- `set_race_started(false)` can be called from ISRs (Play/Pause in SysTick, saturation in TIM5).
- Angular sign convention: **positive = turn right**. `get_measured_angular_speed()` is `-gyro_z`. Heading targets: right `+PI/2`, left `-PI/2`.
- Position along the maze is relative. `current_cell_start_mm` plus encoder distance is reset by `enter_next_cell()` and resynced on side-wall loss (`WALL_LOSS_TO_SENSING_POINT_DISTANCE`). Decisions are taken when the wheel axle is at a cell boundary.

## Current operating mode

Only **EXPLORE_HANDWALL at SPEED_EXPLORE** is used: wall following (left or right), 600 mm/s, no suction. Floodfill, time-trial and the fast speed profiles are inherited from ZoroBot3 and unmaintained. Their maze goals assume a 16×16 or 6×6 maze with a centre goal, which does not match LNR.

Start sequence: on the menu CARRERA screen (all 10 LEDs), arm the side with UP/DOWN, PREV/NEXT, or by covering a front sensor. Then press **Play/Pause on the IR remote**. Play/Pause also stops the robot at any time.

## Key tunables (where they live)

- Maze geometry: `include/config.h` (`CELL_DIMENSION`, `WALL_WIDTH`, `MIDDLE_MAZE_DISTANCE`, `WALL_LOSS_TO_SENSING_POINT_DISTANCE`).
- Wall detection thresholds (raw ADC, per sensor): `src/sensors.c` (`SENSOR_*_NAV_RAW_*`).
- Wall-follow and heading-lock gains: top of `src/control.c`.
- Explore turn profile (`turns_explore`), in-place turns (`turns_inplace`), PID gains (`kpi_explore`), speed/accel (`kinematics_settings[SPEED_EXPLORE]`): `src/move.c`.
- Dead-end behaviour: `handwall_dead_end_u_turn()` in `src/handwall.c`.

Arc-turn angle check: total angle = `max_angular_speed / linear_speed * (4*transition/PI + arc)`. It must equal π/2 for a 90° turn.

## Persistence

The emulated EEPROM is flash sector 11 at `0x080E0000` and holds an `int16_t` array (`include/eeprom.h`). Its layout is: gyro offsets (3) | sensor offsets (4) | maze (256) | menu (7) | RC5 (5) | IR mappings (22). `DATA_INDEX_*` and `DATA_LENGTH` must stay consistent. Calibrations reach flash only through "Calibración → guardar EEPROM", with two exceptions that save immediately: arming the side and IR pairing.

## Conventions

- C99, libopencm3, 2-space indent in original files. Newer team code often uses 4 spaces and long Spanish block comments. `.clang-format` exists but is not enforced.
- Comments and identifiers mix Spanish and English. New comments should follow the surrounding file.
- Commits: Conventional-ish prefixes (`feat:`, `fix:`, `docs:`, `refactor:`), messages in Spanish or English, one PR per change into `juanig0508-code/master`.
