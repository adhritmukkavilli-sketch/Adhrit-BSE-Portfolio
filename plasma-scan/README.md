# Plasma temperature scan: positioning

A 2-axis stage (X = left/right, Y = up/down) moves a thermocouple (AD8495) over a 1 × 1 inch square and takes 30 readings at each stop.

**The problem:** the stage didn't stop exactly at the grid positions, and the grid didn't cover the full inch.

| Folder | What it is |
|---|---|
| `plasma_scan_v3/` | **The scan code to use.** Covers exactly 1 × 1 inch and never moves backward into a stop. |
| `position_test/` | Test sketch: finds out *why* stops would be off. No temperature readings. |
| `plasma_scan_original/` | The scan code as it was |
| `simulator/` | A desktop simulator of the stage, used to check the sketches (optional) |

## How v3 scans

```
 row 1  (Y = 0)      25.40 ← 20.32 ← 15.24 ← 10.16 ← 5.08 ← 0  = home (top-right corner)
 row 2  (Y = 5.08)   25.40 ← 20.32 ← 15.24 ← 10.16 ← 5.08 ← 0
  ...                (X goes back to -1 mm between rows, Y moves down, X comes forward to 0)
 row 6  (Y = 25.40)  25.40 ← 20.32 ← 15.24 ← 10.16 ← 5.08 ← 0
```

- **Exactly 1 × 1 inch:** 6 rows of 6 stops at 0, 5.08, 10.16, 15.24, 20.32 and 25.40 mm on both axes. The first and last stops sit on the edges of the square. The original stopped at 5.08 to 25.40 mm, so the corner and two edges were never measured.
- **Never moves backward into a stop:** every row goes right to left, and the rows go top to bottom. X only moves left into a stop and Y only moves down, the same way home is reached. Play in the belt or lead screw can't shift a stop.
- **The one backward move:** to start a new row, X returns to 1 mm right of the row start (`LEAD_IN_MM`) with no readings, Y moves down, then X moves 1 mm left into the first stop. Every area scan has to return like this once per row.
- **How long:** 36 stops × 30 readings takes about 19 minutes. To keep 25 stops instead, set `POINTS = 5`; the stops are then 6.35 mm (0.25 in) apart, still exactly 1 inch edge to edge.

## What the simulator showed

The simulator runs the real sketch code against a model of the stage and reports where the carriage really stopped.

| Simulated machine | Original | v3 |
|---|---|---|
| Area the stops cover | X 5.10–25.40, Y 5.08–25.40 mm | X 0.00–25.40, Y 0.00–25.40 mm |
| Stops reached moving backward | 10 of 25 | 0 of 36 |
| Worst stop, perfect machine | 0.00 mm | 0.00 mm |
| Worst stop, 0.3 mm play in X | 0.30 mm (accuracy check said OK) | 0.00 mm |
| Worst stop, 0.8 mm play in X, 0.2 mm in Y | 0.80 mm | 0.00 mm |
| Noisy switch wires | Stopped early ("limit switch clicked in the middle of a move") | Ignored the noise and finished |

The accuracy check at the end of a scan touches both switches. It catches **lost steps** but **cannot see backlash**, which is why it said OK on the original. v3 also tightens it from 0.25 mm to 0.10 mm.

## If stops are still off: run the tests

Upload `position_test/position_test.ino`. Open the Serial Monitor at **9600 baud** with **New Line**. It uses the same path and settings as v3.

1. **`T2`: lost steps (no tools).** Runs the scan path 3 times, then slowly touches both switches.
   - `LOST STEPS`: try `V 3000` (slower) and run `T2` again.
   - If slower fixes it, the motors were skipping: raise the driver current, tighten the set screws, and check that the supply isn't current-limiting.
2. **`T4`: point tour.** Fix graph paper in front of the sensor tip. It stops at the 36 points the same way the scan does; mark each one and press Enter (`T4 3` waits 3 s per point instead). You should see a 6 × 6 grid, 5.08 mm apart and 25.4 mm edge to edge.
3. **`T3`: backlash (calipers).** Measures the play on each axis. It must be smaller than the 1 mm lead-in, and T3 tells you if it is.
4. **`T1`: steps/mm (calipers or a ruler).** The stage moves 25.4 mm; you type how far it really moved. If `X_STEPS_PER_MM` or `Y_STEPS_PER_MM` is wrong, the grid is not exactly 1 inch. T1 prints the right value. About 2× or 4× off means the driver's microstep DIP switches don't match the code.

Other commands: `H` home, `X 12.7` / `Y 5.08` go to a position, `W` show position and switch states, `S` stop, `A` ramp on/off, `?` help.

## Settings in plasma_scan_v3

| Constant | Default | What it does |
|---|---|---|
| `POINTS` | 6 | Stops per row and number of rows. The gap is always `SIDE_MM / (POINTS - 1)`. |
| `SIDE_MM` | 25.4 | Size of the square |
| `LEAD_IN_MM` | 1.0 | How far X goes past the row start before coming forward. Must be bigger than the play (T3). Max 1.5. |
| `SETTLE_MS` | 1000 | Wait after arriving before the first reading. Raise it if the first readings of each row look off: that stop comes right after the 26 mm return. |
| `X_STEPS_PER_MM`, `Y_STEPS_PER_MM` | 20, 160 | Change only if T1 says so |
| `RAMP_STEPS` | 40 | Moves start and end slowly (0 = off) |

## Hardware to check if the tests point at it

- The **set screws** on the X pulley/coupling and the Y flexible coupling should be tight, with one on the motor shaft's flat.
- The **belt** should be tight enough that it doesn't sag or jump teeth.
- **Driver current DIP switches**: set them close to the motor's rated current.
- The **bench supply** must not hit its current limit while moving. If the CC light comes on, the voltage drops and the motors skip.

## Simulator (optional)

`simulator/sim.cpp` runs a sketch on a computer against a model of the stage. You can add play, wrong steps/mm, skipped steps, switch noise or a press of S. It prints where the carriage really stopped, which direction it arrived from, and the area the stops cover. Build it with a C++17 compiler (tested with g++); the options are listed at the top of the file. Example:

```
cd plasma-scan/simulator
g++ -O2 -std=c++17 -DSKETCH='"../plasma_scan_v3/plasma_scan_v3.ino"' sim.cpp -o sim
./sim q bx=0.3 -- G
```
