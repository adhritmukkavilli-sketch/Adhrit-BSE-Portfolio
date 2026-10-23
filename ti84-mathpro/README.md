# MATHPRO for the TI-84 Plus CE

MATHPRO turns the calculator into one that **shows its work**. It opens in a calculator mode that
works like the normal home screen, but you can also type things like `FACTOR X³-7X+6` or
`X²-5X+6≥0` and it solves them step by step. It covers Advanced Algebra 2 (factoring, inequalities,
quadratics, complex numbers, polynomials, rational functions), plus geometry, calculus, stats and more.

## Download it and put it on the calculator

**1. Get the file `MATHPRO.8xg`** (one file holds all 22 programs). Either:
- open https://github.com/adhritmukkavilli-sketch/Adhrit-BSE-Portfolio/blob/ti84-mathpro/ti84-mathpro/dist/MATHPRO.8xg
  and click the **download** button (the arrow pointing down, top right of the file view), or
- download it straight from https://github.com/adhritmukkavilli-sketch/Adhrit-BSE-Portfolio/raw/ti84-mathpro/ti84-mathpro/dist/MATHPRO.8xg

**2. Install TI Connect CE** on a Windows or Mac computer: search "TI Connect CE download" or go to
education.ti.com → Downloads → TI Connect CE. It's free.

**3. Send it to the calculator:**
1. Turn the calculator on and plug it into the computer with its USB cable.
2. Open TI Connect CE and click **Calculator Explorer** (the second icon on the left side).
3. Drag `MATHPRO.8xg` from your Downloads folder onto the calculator's name.
4. If it asks where to put the programs, the default is fine. Click **Send**.

**4. Run it:** press `prgm`, pick **MATHPRO**, and press `enter` twice.
Next time, `2nd` `enter` (entry recall) brings back `prgmMATHPRO` right away.

You need calculator OS 5.3 or newer. To check, press `2nd` `+` (mem), then **1:About**. If it's older,
TI Connect CE can update it.
If the calculator says it's out of memory, delete or archive other programs and apps you don't need
(`2nd` `+` → **2:Mem Management**). MATHPRO needs about 64 KB.

### "Normal calculator + a button for the special mode"
The calculator itself stays completely normal. Nothing changes until you run MATHPRO.
TI blocks programs from taking over keys on calculators with OS 5.5 or newer, so the "button" is
`prgm` → MATHPRO. Once it's running, **calculator mode** works like the regular home screen,
and typing **`M`** opens the special tool menus.

## Calculator mode

| You type | What happens |
| --- | --- |
| `2+3*4`, `√(⁻4)` | Normal math. Complex answers work (`2𝑖`) |
| `FACTOR X³-7X+6` | Factors it, showing GCF, rational root theorem, AC method, grouping |
| `EXPAND (2X+3)^5` | Multiplies it all out |
| `ROOTS X³-1` | Every root, real and complex, with multiplicities (Fundamental Theorem of Algebra) |
| `X²=2X+3` or `2^X=X+3` | Solves it (anything with `=`) |
| `X²-5X+6≥0`, `abs(X-3)<5` | Solves the inequality with a sign chart, answer in **interval notation** |
| `GRAPH X³-4X` | Graphs it with a window that fits |
| `TABLE X²-3` | Table of values from X=⁻4 to 4 |
| any answer | Also shows the **exact form** when there is one: `.6666…` → `2/3`, `2.356…` → `3π/4`, `1.414…` → `√(2)` |
| `M` | Tool menus |
| `Q` | Quit |

Type letters with `alpha` (`2nd` `alpha` locks it). `=`, `<`, `≥`... are in `2nd` `math`. `abs(` is in `math` → NUM.

## Tool menus

| Menu | Tools |
| --- | --- |
| **Algebra** | Solve anything (equations + inequalities) · **Quadratics:** solve + graph with exact radical/complex answers, completing the square, parabola through 3 points, vertical motion, **complex numbers** (+ − × ÷, powers of 𝑖, √ of negatives) · Linear equations · **Transformations** of parent functions (described + graphed) · 2×2 / 3×3 systems · Exponential equations · Simplify √N |
| **Polynomials** | Expand · Factor · All roots (lists the possible rational roots) · **Graph features** (end behavior, zeros crossing/touching, turning points, local max/min) · Long division · Build a polynomial from its roots · Binomial theorem · FOIL · Synthetic division |
| **Rational (Ch 7)** | **Graph features** (holes, vertical/horizontal/slant asymptotes, intercepts, domain, graph) · Simplify · Multiply/divide · Add/subtract (finds the LCD) · Solve rational equations |
| **More math** | **Matrices:** row reduce showing every row operation (solves systems), determinant by cofactors, inverse, multiply · **Exp + logs:** log of any base (change of base), exponent equations, exponential model through 2 points, half-life/doubling, log rules, solve log equations · **Geometry + trig:** right triangles, SSS/SAS/ASA and the **SSA ambiguous case** (0, 1 or 2 triangles), **unit circle exact values**, degrees ↔ radians in terms of π, distance/midpoint/slope, circles + solids · **Calculus:** function tools (tables, composition f(g(x)) simplified, average rate of change), derivatives, integrals, Riemann sums, Newton's method, limits · **Stats + probability:** 1-variable stats, line of best fit, perms/combos, **binomial**, **normal curve** (shaded), percentiles, interest, loans · **Numbers + code:** GCD, primes, base converter, bitwise, fractions, sequences |
| **Settings** | Show work ON/OFF · Step pace (pause every screen, or every line) · Decimal places · Title screen ON/OFF |

### Tips
- When the screen fills up, it pauses. Press `enter` to keep going.
- Type X with the `X,T,θ,n` key.
- Factoring and the rational tools need whole-number coefficients. Polynomials can go up to degree 10.
- "Solve anything" looks for answers between two X values you choose (−25 to 25 in calculator mode).
- Big problems can take a few seconds. TI-BASIC is not fast.
- If a tool ever stops with an error, choose **1:Quit** and run MATHPRO again.

### Good to know
MATHPRO uses the letter variables A–Z (calculator mode included), `Str0`–`Str9`, matrices `[A]`–`[C]`, `Y₁` and `Y₂`, and it changes the graph window.
Its settings and scratch work live in lists whose names start with `MP`, so it doesn't touch L₁–L₆.
Geometry switches to Degree mode, and calculus switches to Radian mode.

## How it's built and tested

- `src/*.tib`: the readable TI-BASIC source. It supports `#` comments, indentation, and shortcuts
  (`SHOW` a work step, `ANS` an answer, `POLY` a polynomial as text).
- `build.py`: turns the source into `.8xp` files plus the `MATHPRO.8xg` bundle (`pip install tivars`,
  then `python3 build.py`). It refuses to build if it finds a TI-BASIC mistake that would only show up
  on the calculator. Examples: a `Goto` inside a loop (leaks memory), `-` where the negative sign `⁻` belongs,
  `R²` turning into the regression variable, or `>=` silently becoming `≥` inside text.
- `tools/tisim.py`: a TI-BASIC simulator that runs the real compiled files.
  `python3 tools/test_mathpro.py` runs 148 tests that cover every tool and check the answers.

The tests run on the simulator, not a real calculator. If something acts differently on the real
calculator, note the error and what you typed.
