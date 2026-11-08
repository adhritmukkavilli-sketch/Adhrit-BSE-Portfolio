# Plasma temperature scan: positioning tests

A 2-axis stage (X = left/right, Y = up/down) moves a thermocouple (AD8495) over a 1 × 1 inch area in a 5 × 5 grid with 5.08 mm (0.2 in) between stops. It takes 30 readings at each stop.

**The problem:** the stage doesn't stop exactly at the grid positions.

| Folder | What it is |
|---|---|
| `plasma_scan_original/` | The scan code as it was |
| `position_test/` | **Test sketch.** Finds out *why* the stops are off. No temperature readings. |
| `plasma_scan_v2/` | The scan code with the fixes |
| `simulator/` | A desktop simulator of the stage, used to check all three sketches (optional) |

## Why the stage can stop in the wrong place

The Arduino can't see where the stage is. It only counts the step pulses it sends. When the Serial Monitor shows `X 10.15 mm`, that's where it **told** the motor to go. The real position can differ in three ways:

| Cause | What you see | Found by | Fix |
|---|---|---|---|
| **Backlash** (play in the belt, lead-screw nut or coupling: when a motor reverses, the first few steps don't move the stage) | Rows 2 and 4 shifted sideways from rows 1, 3 and 5, because the scan zig-zags and X reverses every row. **The scan's accuracy check still says OK.** | `T3`, `T4` | v2 reaches every stop moving away from home (`BACKLASH_MM`) |
| **Lost steps** (motor skips: too fast, too little driver current, supply hitting its current limit, or a pulley/coupling set screw slipping) | Points drift more and more; the accuracy check says "steps were lost" | `T2` | Slower speed, ramp, more current, tighten set screws |
| **Wrong steps/mm** (driver microstep DIP switches don't match `X_STEPS_PER_MM` / `Y_STEPS_PER_MM`) | The whole grid is too big or too small | `T1` | Change the constant |

### What the simulator showed

The scan code's math is correct: on a perfect machine every stop is within 0.05 mm (one X step).

With **0.3 mm of play in X** (normal for a belt or lead screw), the original code put 10 of the 25 stops 0.3 mm off. Those were the stops after X reversed: points 7–11 and 17–21. The accuracy check at the end still printed `X off by 0.000 mm OK`.

That's because the switch check can only find lost steps. Every trip back to the switch ends moving the same way, so the play cancels out. v2 put all 25 stops within 0.00 mm on that machine, and also with 0.8 mm of play.

| Simulated machine | Original: worst stop | v2: worst stop |
|---|---|---|
| Perfect | 0.00 mm | 0.00 mm |
| 0.3 mm play in X | 0.30 mm (accuracy check said OK) | 0.00 mm |
| 0.8 mm play in X, 0.2 mm in Y | 0.80 mm | 0.00 mm |
| Noisy switch wires | Stopped with "limit switch clicked in the middle of a move" | Ignored 6 noise spikes, finished |

## How to test (about 20 minutes)

Upload `position_test/position_test.ino`. Open the Serial Monitor at **9600 baud** with **New Line**. It starts with the same settings as the original scan code (`K 0`, `A 0`, `V 1500`), so first you are testing the code as it is.

1. **`T2`: lost steps (no tools).** It homes, runs the 25-point scan path 3 times with only a short stop at each point, then slowly touches both switches.
   - `OK - no lost steps` on both axes: the motors aren't skipping. Go to step 2.
   - `LOST STEPS`: try `V 3000` (slower) or `A 40` (ramp), then `T2` again. Also check the set screws, the driver current and the supply.
   - `switch noise spikes ignored: N` with N > 0: the switch wires pick up noise from the motor wires. Move them apart or twist each switch pair.
2. **`T4`: point tour.** Fix a piece of graph paper right in front of the sensor tip. It stops at each of the 25 points in scan order. Mark where the tip is, then press Enter for the next one (`T4 3` waits 3 s per point instead).
   - Rows 2 and 4 shifted sideways means backlash (step 3).
   - A grid that's too big or too small means steps/mm (step 4).
3. **`T3`: backlash (calipers).** Clamp the calipers between the frame and the moving carriage. The stage goes to 12.7 mm twice, once arriving from each side. Type both readings. It prints the play and suggests a `K` value. Type `K 1`, then `T3` again: both readings should now match.
4. **`T1`: steps/mm (calipers or ruler).** Zero the calipers when asked. The stage moves 25.4 mm, and you type how far it really moved. It prints the real steps/mm. If it is about 2× or 4× off, the driver's microstep DIP switches don't match the code.

Other commands: `H` home, `X 12.7` / `Y 5.08` go to a position, `W` show position and switch states, `S` stop, `?` help.

## Then run the fixed scan

Upload `plasma_scan_v2/plasma_scan_v2.ino`. It works the same as before (`G` = home + scan, `S` = stop, `H` = home). Set these constants first:

- `BACKLASH_MM`: default 1.0 mm. It must be bigger than the play T3 measured. Near home the code shortens the overshoot so it never gets closer than 1 mm to a switch.
- `X_STEPS_PER_MM` / `Y_STEPS_PER_MM`: change these only if T1 said so.
- `RAMP_STEPS`: default 40. Use 0 to turn the ramp off.

Everything else is unchanged: the same 25 stops, 30 readings, table and CSV.

## Hardware to check if the tests point at it

- The **set screws** on the X pulley/coupling and the Y flexible coupling should be tight, with one on the motor shaft's flat.
- The **belt** should be tight enough that it doesn't sag or jump teeth.
- **Driver current DIP switches**: set them close to the motor's rated current.
- The **bench supply** must not hit its current limit while moving. If the CC light comes on, the voltage drops and the motors skip.
- The **sensor arm** sticks out a long way, so the tip can wobble after a stop. The scan already waits 1 s before the first reading.

> Note on the grid: the stops are at 5.08, 10.16, 15.24, 20.32 and 25.40 mm from home on each axis. The home corner itself (0, 0) is not measured.

## Simulator (optional)

`simulator/sim.cpp` runs a sketch on a computer against a model of the stage. You can add play, wrong steps/mm, skipped steps or switch noise, and it prints where the carriage really stopped. Build it with a C++17 compiler (tested with g++); the options are listed at the top of the file. Example:

```
cd plasma-scan/simulator
g++ -O2 -std=c++17 -DSKETCH='"../plasma_scan_original/plasma_scan_original.ino"' sim.cpp -o sim
./sim q bx=0.3 -- G
```
