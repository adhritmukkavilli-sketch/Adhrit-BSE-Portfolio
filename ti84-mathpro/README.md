# MATHPRO for the TI-84 Plus CE

A menu-driven math suite that **shows its work** step by step, then gives the answer.
It also has exact answers (fractions and simplified radicals), graphs, and a birthday mode.

## What it can do

| Menu | Tools |
| --- | --- |
| **Algebra** | Quadratic formula (discriminant, exact radical/complex roots, vertex, factored form, graph) · Linear equations · 2×2 and 3×3 systems (Cramer's rule) · Exponential equations A·Bˣ=C · Simplify √N |
| **Numbers + Code** | GCD/LCM with the Euclidean algorithm · Prime factorization + divisor count · Base converter (binary/octal/hex, any base 2–36) · Bitwise AND / OR / XOR / shifts · Fraction + − × ÷ with simplifying · Sequences (arithmetic, geometric, Fibonacci, Collatz) |
| **Geometry + Trig** | Right triangles · SSS / SAS / ASA triangle solvers (law of cosines and sines, area, type) and **draws the triangle** · Two points: distance, midpoint, slope, line equation · Circle, sphere, cylinder, cone |
| **Calculus** | Derivative at a point (secant-slope table, tangent line, concavity, graph) · Definite integral + shaded area · Left/right/midpoint/trapezoid/Simpson sums with **drawn rectangles** · Newton's method · Limit tables |
| **Stats + Money** | 1-variable stats · Line of best fit with r · Permutations/combinations · Compound interest · Percent change · Loan payment |
| **Settings + More** | Show work ON/OFF · Step pace (pause every screen, or every line for teaching) · Decimal places (Float / Fix 2 / Fix 4) · Birthday mode · About |

Birthday mode draws a cake with flickering candles. It plays the first time MATHPRO runs, and you can replay it from **Settings + More**.

## Put it on the calculator

1. Install **TI Connect CE** (free from education.ti.com) and plug in the calculator with its USB cable.
2. Drag **all four** files from `dist/` onto the calculator in TI Connect CE:
   - `MATHPRO.8xp`: the main program
   - `ZMPSHOW.8xp`, `ZMPFRAC.8xp`, `ZMPGRAPH.8xp`: helpers MATHPRO needs (the Z puts them at the bottom of the list)
3. On the calculator, press `prgm`, pick **MATHPRO**, and press `enter` twice.

You need OS 5.3 or newer, because MATHPRO uses `toString(`. To check, press `2nd` `+` (mem), then **1:About**. If the OS is older, update it with TI Connect CE.

### Tips
- When a screen fills up, it pauses. Press `enter` to keep going.
- For calculus, type the function with the `X,T,θ,n` key, e.g. `X²-2` or `sin(X)/X`.
- For lists (3×3 systems, stats), type them with braces: `2nd` `(` … `2nd` `)`, e.g. `{2,1,⁻1,8}`.
- In the base converter, use `alpha` to type hex letters A–F.
- If a tool ever stops with an error, choose **1:Quit** and run MATHPRO again.

### Good to know
MATHPRO uses the letter variables A–Z, `Str0`–`Str9`, matrices `[A]`–`[C]`, and `Y₁`, and it changes the graph window.
It saves its settings in a list called `MPSET` and uses lists `MPT` and `MPU` for data, so it doesn't touch L₁–L₆.
The geometry tools switch the calculator to Degree mode and the calculus tools switch it to Radian mode.

## Changing it / building it yourself

The readable source lives in `src/*.tib`. `build.py` turns it into `.8xp` files:

```bash
pip install tivars
python3 build.py --teacher "MR. SMITH" --from "ADHRIT"
```

`--teacher` sets the name on the birthday screen. The source supports `#` comments, indentation, and two shortcuts:
`SHOW "text"` for a work step (hidden when show work is off) and `ANS "text"` for an answer (always shown).
The build also catches TI-BASIC mistakes that otherwise only show up on the calculator: unbalanced `End`s,
a `Goto` inside a loop (that leaks memory), missing labels, `-` typed where the negative sign `⁻` belongs,
and menu text that's too long.
