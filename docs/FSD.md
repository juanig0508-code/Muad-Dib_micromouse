# Muad-Dib — Functional Specification (FSD)

| | |
|---|---|
| Product | Muad-Dib maze robot firmware (fork of OPRobots ZoroBot3) |
| Target event | LNR Argentina, "Laberinto" category, 2026 season |
| Firmware baseline | `master` @ `b0b255d` (after PR #15) |
| Status | Draft v0.1, 2026-10-19 |

Requirement IDs (`FR-x`, `NFR-x`) are referenced from `docs/ISSUES.md`. Each requirement has a status:
- **Met**: implemented and believed to work.
- **Partial**: implemented with known defects.
- **Gap**: not implemented, or not compliant.

---

## 1. Purpose and scope

The robot must reach a single goal cell in an unknown maze as fast as possible and score under the LNR rules. The expected competition maze is small: **5×4 or 5×5 cells**, near the rules' 4×5 minimum.

In scope: the robot firmware in `source_code/src` and `source_code/include`.
Out of scope: the simulator (`source_code/simulator`, `lib/mmsim_api`), the PCB, and the 3D model.

## 2. Competition rules that drive requirements

Source: LNR "Reglamento técnico de Laberinto", retrieved 2026-10-19.

| # | Rule | Consequence for firmware |
|---|---|---|
| R1 | The rules allow cells of **18×18×10 cm** or **25×25×15 cm** (±0.5 cm), measured centre to centre, during 2026. **The next tournament uses 25 cm cells only.** The real pitch varies from maze to maze. | All geometry derives from one constant, `CELL_DIMENSION`, set to the **measured** pitch of the maze in use (practice maze: 260 mm). 18 cm support is out of scope for this tournament. |
| R2 | Matte black floor, white walls. | IR wall sensing on white walls is the right approach. |
| R3 | The jury picks the start cell. It has three walls, may be anywhere (not necessarily a corner), and the jury sets the orientation. | No fixed start corner or heading can be assumed. The robot starts with its back against a wall. |
| R4 | The goal is **one cell with a white floor**. It is not the centre. | The goal position is unknown. Floodfill-to-centre is useless without a goal detector. Time stops when the robot is fully inside the goal cell, and it does not need to stop there. |
| R5 | One valid goal and one solution path "without equivalent bifurcations". No free-standing posts. No 2×2 open areas. | Wall following works if the goal sits on a wall connected to the start's wall system. Loops are not explicitly forbidden, so wall following is not guaranteed in general (see FR-N5). |
| R6 | The minimum area is 4×5 cells. Expected: 5×4 or 5×5. | A ≤ 6×6 map fits easily. Straights are at most about 1.3 m. |
| R7 | There is a visible switch or push button to **start and stop** the robot. | A **physical** start/stop control is mandatory (FR-S1). The IR remote is only for testing. An on-board button will be added later. |
| R8 | The robot is fully autonomous with no external control. Telemetry is allowed, but the robot must work without it. | The competition firmware must not depend on the IR remote (FR-S1, FR-S2). |
| R9 | The robot may not be operated once the run has started. | No mid-run inputs. |
| R10 | Two configurations, **2 passes each** (§5.1.2 says "5 pasadas", which is an erratum), best pass counts. A max time is set per configuration. A pass with no finish is scored by remaining cells. | There are only 4 attempts in total, so **reliability beats speed**. A robot that fails mid-way should still be as close to the goal as possible. Memory between passes is allowed if the robot is not modified. |
| R11 | A pass ends if the robot stays **≥10 s in the same cell** or **≥15 s still or pushing a wall**. | Stuck detection and recovery have to happen within a few seconds (FR-C6). |
| R12 | Robots are impounded after practice, with no modifications until the 30 min break between configurations. | Measure the maze pitch, set `CELL_DIMENSION`, flash and calibrate during practice time, before handing the robot over. This assumes the competition maze is built from the same parts as the practice maze. Side choice must take seconds. |

## 3. System overview

### 3.1 Hardware (as built)

| Item | Part | Notes |
|---|---|---|
| MCU | STM32F405RGT6, 168 MHz | 24 MHz HSE crystal |
| Gyro | MPU6500 (SPI3) | replaces the original LSM6DSR |
| Encoders | AS5145B, quadrature on TIM3/TIM4 | 10.494 µm/tick |
| Motors | 2× 8520 coreless, MP6551 driver, 20 kHz PWM | 8T/40T gearing; nylon gears recommended after breakages |
| Wall sensors | 4× SFH-4550 emitters + ST-1KL3A receivers | FL, FR (front), SL, SR (side) |
| UI | 3 analog buttons (MODE/UP/DOWN), 10 info LEDs, status LED, IR receiver (TSSP77038) | RGB LED not populated |
| Battery | 2S LiPo 260 mAh | voltage measured on the ADC |
| Fan | present, **unused** | |

### 3.2 Software architecture

See the timing table in `CLAUDE.md`. There are three interrupt contexts (TIM2 sensors, SysTick housekeeping, TIM5 control) and one blocking foreground loop that runs the menu, then navigation and motion primitives.

```
main loop
 ├─ menu_handler() / check_selftest_buttons() / start logic     (robot idle)
 └─ handwall_loop()                                             (race)
      └─ get_walls() → decision → move(MOVE_FRONT|LEFT|RIGHT) / dead-end u-turn
            └─ move_straight / move_arc_turn / move_inplace_turn / keep_front_distance
                  └─ set targets; busy-wait on encoders/gyro/time
TIM5 control_loop(): ideal speeds → PID (linear: encoders; angular: gyro)
                     + heading lock + single-wall follow + front-wall PIDs → PWM
```

## 4. Functional requirements

### 4.1 Start, stop and modes (S)

| ID | Requirement | Status |
|---|---|---|
| FR-S1 | The robot shall start a run with an **on-board physical control** and no remote (R7, R8). | **Gap (planned).** Today the run starts only with IR Play/Pause, which is for testing. Covering a sensor only arms a side. An on-board button will be added. |
| FR-S2 | The robot shall stop immediately on a physical control. The IR Play/Pause stop is kept for testing. | **Partial (planned).** IR stop works. There is no physical stop besides the power switch. |
| FR-S3 | The wall side (left/right) shall be selectable before the start and shown on the LEDs. | Met (UP/DOWN, PREV/NEXT, or sensor cover). |
| FR-S4 | After a start command the robot shall wait a short, fixed time so the operator's hand is clear, then move. | Partial (`delay(500)` in `handwall_start`, and 2 s inside `check_start_run` when armed by sensor). |
| FR-S5 | The robot shall take its current heading as 0° at start, and its current position as "back against the start wall, centred". | Met (`reset_heading_reference`, `set_starting_position`). |
| FR-S6 | The robot shall be able to do several passes without a power cycle, keeping calibration. | Partial. The MENU.md warning about needing a restart is inherited from ZoroBot3 and has not been re-verified for handwall. |

### 4.2 Sensing (W)

| ID | Requirement | Status |
|---|---|---|
| FR-W1 | Each of the 4 IR sensors shall be sampled with ambient-light cancellation (emitter on minus emitter off) at ≥1 kHz. | Met (~2 kHz per sensor). |
| FR-W2 | Wall presence (front/left/right) for navigation shall be decided per sensor with a raw threshold, calibrated for the venue's lighting and the cell size in use. | Partial. The thresholds are hard-coded and some predate the sensor repair. |
| FR-W3 | Distances in mm shall be accurate to ±5 mm in 20–150 mm (side) and 40–250 mm (front) for centring and front alignment. | **Gap.** The log linearisation uses ZoroBot3 A/B/C coefficients. The team reports that the mm values are unreliable. |
| FR-W4 | Wall detection shall be taken at a fixed, known position within the cell (the decision point). | Partial. It depends on position tracking (FR-M6). |
| FR-W5 | Gyro Z bias shall be calibrated at rest and stored. Heading shall be integrated at the true sample rate. | Partial. Calibration exists, but the stored bias likely overflows `int16` (ISSUES L-22). The control loop integrates at ≈1003 Hz assuming 1000 Hz (L-09). |

### 4.3 Navigation (N)

| ID | Requirement | Status |
|---|---|---|
| FR-N1 | Left-hand and right-hand wall following with symmetric rules: open on the followed side → turn to it; else front open → straight; else other side open → turn; else dead end → U-turn. | Met (`handwall.c`). |
| FR-N2 | A dead end shall be handled by an in-place 180° that ends centred in the cell and facing out, with position tracking updated. | **Partial.** The two 90° turns are implemented, but the cell position is not re-referenced afterwards. |
| FR-N3 | The robot shall keep exploring until stopped. It need not stop at the goal (R4). | Met (the loop is unbounded; `time_limit` is 0). |
| FR-N4 | The robot shall not need to know the goal location. | Met for handwall. Floodfill violates it (centre goals). |
| FR-N5 *(future)* | If the start and goal are not on the same wall system, the robot should still reach the goal (e.g. a Trémaux/floodfill exploration of the whole maze bounded by the maze size, or an alternating-hand strategy across passes). | Gap (optional). |
| FR-N6 *(future)* | Use passes 2–5 to run faster on a remembered path once the goal has been seen. That needs a goal detector (floor sensor) or the operator entering the goal position. | Gap (optional, needs hardware). |

### 4.4 Motion (M)

| ID | Requirement | Status |
|---|---|---|
| FR-M1 | Straight moves shall accelerate and brake on a trapezoidal profile and end within ±5 mm of the commanded distance. | Met (encoders, `move_straight`). |
| FR-M2 | Explore turns (`MOVE_LEFT`/`MOVE_RIGHT`) shall be smooth arc turns of **90° ± 2°**. They shall start at a fixed point relative to the cell and end centred in the next cell's corridor, aligned with it. | **Partial.** The profile integrates to ≈85°, and the team reports over- or under-turn and loss of centring. |
| FR-M3 | In-place turns of ±90°/180° shall end within ±2° of target, gyro-closed. | Partial. `move_inplace_turn` is time-based (open loop on angle). `move_inplace_angle` is closed loop, but only the self-tests use it. |
| FR-M4 | During straights the robot shall hold its heading (heading lock on the gyro) and its lateral position relative to the followed wall. | Partial. Heading lock works. Wall-follow uses mm distances, which are unreliable (FR-W3). |
| FR-M5 | Front-wall alignment (distance and angle) shall centre the robot in the cell before in-place turns. | Partial. It depends on FR-W3. |
| FR-M6 | Position along the corridor shall be tracked per cell and re-synchronised at side-wall loss (post edges) to cancel encoder drift. | Partial (the dead-end path and the 25 cm geometry constants are suspect). |
| FR-M7 | All motion parameters (turn entry/exit distances, wall-loss distance, wall-follow target, detection distances) shall be derived from `CELL_DIMENSION` and `WALL_WIDTH`, so a new measured pitch needs only one change (R1). | **Gap.** The turn tables and wall-loss distances are hand-tuned constants. |

### 4.5 Control and safety (C)

| ID | Requirement | Status |
|---|---|---|
| FR-C1 | Linear speed PID on encoders and angular speed PID on the gyro shall run at a fixed 1 kHz. | Met. |
| FR-C2 | Motor voltage commands shall be compensated for battery voltage. | Met (`voltage_to_motor_pwm`). |
| FR-C3 | Integral terms shall be bounded (anti-windup). | Gap. |
| FR-C4 | On a crash or stall (PWM saturated, or encoder-derived angular speed too high), the motors shall be cut and the run ended. | Met (PR #15). |
| FR-C5 | The fan shall stay off in explore. | Met. |
| FR-C6 *(recommended)* | Before R11's 10 s / 15 s limits, a stall should trigger a **recovery** (back off, re-align, continue) rather than end the run. A finished run still scores remaining distance, but a recovered run can still finish. | Gap. |
| FR-C7 | A low battery shall be indicated at boot. | Met (LED bar). There is no in-run cut-off. |

### 4.6 User interface and configuration (U)

| ID | Requirement | Status |
|---|---|---|
| FR-U1 | The menu is driven by the analog buttons and the paired IR remote. Screens: speed, CARRERA (arm side), explore type, floodfill type, maze type, strategy, algorithm. | Met (see `MENU.md`). |
| FR-U2 | Config menu: gyro calibration, side-sensor offset, front-sensor offset, save EEPROM, IR pairing. Debug screens. | Met. |
| FR-U3 | IR pairing shall learn 11 buttons and persist them across power cycles. | **Partial.** An EEPROM layout overflow loses the last button's upper half-word. |
| FR-U4 | Remote buttons 1–5 shall run isolated motion self-tests (90°, 180°, 1 m straight, explore turn L/R). | Met. |
| FR-U5 *(optional)* | Fine-tune `CELL_DIMENSION` (±10 mm) from the menu without reflashing, for a venue whose maze differs from the practice one. | Gap. Low priority, since reflashing during practice is acceptable. |

### 4.7 Persistence (P)

| ID | Requirement | Status |
|---|---|---|
| FR-P1 | Calibrations, menu values, the maze and IR mappings shall be stored in flash sector 11 with a consistent layout. | **Partial** (layout overflow, see FR-U3). |
| FR-P2 | Flash writes shall not happen during a run, and should not happen on every menu click. | Partial. Arming a side triggers a full sector erase and write. |

## 5. Non-functional requirements

| ID | Requirement | Status |
|---|---|---|
| NFR-1 | Reliability first: ≥9 of 10 practice passes reach the goal on a 5×5 maze with 25 cm cells, with both wall sides tested. With only 2 passes per configuration, every pass counts. | Not yet measured. |
| NFR-2 | Explore speed of 600 mm/s with acceptable reliability. A 5×5 maze in 25 cm cells should finish in < 30 s. | Not yet measured. |
| NFR-3 | ISR budget: TIM5 + SysTick + TIM2 together under 50% CPU. No blocking calls in ISRs. | Believed met. Flash erase during arming stalls the ISRs. |
| NFR-4 | Rule compliance (R7, R8) verified with the jury before the event. | Open. |
| NFR-5 | Firmware builds cleanly with no warnings in team-modified files. | Not verified. |

## 6. Interfaces

- **UART3** at 115200 8N1: `printf` telemetry and macroarray dumps (debug mode).
- **IR remote**: any protocol. A frame hash is learned per button.
- **Buttons**: one ADC line (PA7) with a resistor ladder. UP ≈1300–1700, MODE ≈2300–2700, DOWN ≥3700.

## 7. Acceptance tests (bench and maze)

| Test | Pass criteria | Covers |
|---|---|---|
| T1 Gyro 90°/180° (buttons 1, 2) | Final angle within ±2°, 10 repetitions | FR-M3, FR-W5 |
| T2 1 m straight (button 3) | Distance ±5 mm, lateral drift < 10 mm without walls | FR-M1, FR-M4 |
| T3 Explore turn L/R (buttons 4, 5) on a bare floor | Heading change 90° ± 2°. Exit point within ±5 mm of the theoretical arc end | FR-M2 |
| T4 Wall detection sweep | Each sensor classifies wall/no-wall correctly at the decision point, in the measured 25 cm maze | FR-W2, FR-W4 |
| T5 Corridor 5 cells | Exits centred ±10 mm, no wall contact | FR-M4, FR-M6 |
| T6 Dead end | Exits the dead-end cell centred, then the next decision is taken at the correct cell boundary | FR-N2, FR-M6 |
| T7 Full 5×5 maze, left and right hand, 5 passes each | NFR-1 | all |
| T8 Start/stop without remote | Physical control starts and stops | FR-S1, FR-S2 |
| T9 Power-cycle after IR pairing | All 11 buttons still work | FR-U3, FR-P1 |

## 8. Open questions

1. ~~Cell size~~ **Resolved:** 25 cm nominal, no 18 cm cells. `CELL_DIMENSION` takes the measured pitch (practice maze: 260 mm). The comments in `config.h` still say 272 and need fixing.
2. ~~IR remote at the competition~~ **Resolved:** the remote is for testing only. An on-board start button will be implemented.
3. Is a floor sensor (white goal detection) planned? It would enable FR-N5 and FR-N6.
4. Is the competition maze built from the same parts as the practice maze, so that the 260 mm pitch carries over? Check this at the venue.
