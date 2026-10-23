"""Runs every MATHPRO tool in the simulator and checks the answers.

    python3 tools/test_mathpro.py          # all tests
    python3 tools/test_mathpro.py factor   # only tests whose name contains "factor"
    python3 tools/test_mathpro.py -v ...   # also print the screen of each test
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from tisim import Sim, TIError  # noqa: E402

DIST = pathlib.Path(__file__).parent.parent / "dist"

# Each test: name, the keys/inputs typed in order, and text that must appear.
# The program starts in calculator mode; "M" opens the menu.
CALC = []
TESTS = [
    # ------------------------------------------------------------ calculator mode
    ("calc math", ["2+3*4", "√(⁻4)", "Q"], ["14", "2𝑖"]),
    ("calc factor", ["FACTOR X³-7X+6", "Q"], ["= (X-1)(X+3)(X-2)"]),
    ("calc factor AC", ["FACTOR 6X²+X-2", "Q"], ["SPLIT: 6X²-3X+4X-2", "GROUP: 3X(2X-1)+2(2X-1)", "= (3X+2)(2X-1)"]),
    ("calc factor gcf", ["FACTOR ⁻2X³+8X", "Q"], ["= ⁻2X(X-2)(X+2)"]),
    ("calc factor square", ["FACTOR 4X²+12X+9", "Q"], ["PERFECT SQUARE", "= (2X+3)²"]),
    ("calc factor x4-16", ["FACTOR X^4-16", "Q"], ["= (X+2)(X-2)(X²+4)"]),
    ("calc factor prime", ["FACTOR X²+1", "Q"], ["(IT IS PRIME)"]),
    ("calc factor cubes", ["FACTOR X³-8", "Q"], ["= (X-2)(X²+2X+4)"]),
    ("calc factor 3x+6", ["FACTOR 3X+6", "Q"], ["= 3(X+2)"]),
    ("calc factor big", ["FACTOR 2X^4-X³-18X²+9X", "Q"], ["= X(2X-1)(X-3)(X+3)"]),
    ("calc expand", ["EXPAND (2X+3)^5", "Q"], ["= 32X^5+240X^4+720X³+1080X²+810X+243"]),
    ("calc expand product", ["EXPAND (X-1)(X²+4)", "Q"], ["= X³-X²+4X-4"]),
    ("calc expand not poly", ["EXPAND sin(X)", "Q"], ["THAT IS NOT A POLYNOMIAL"]),
    ("calc roots", ["ROOTS X³-1", "Q"], ["X=1", "X=⁻1/2+/-.866025404𝑖", "1 REAL + 2 COMPLEX = 3 ROOTS"]),
    ("calc roots repeated", ["ROOTS X³-3X²+3X-1", "Q"], ["X=1 (MULTIPLICITY 3)"]),
    ("calc roots quartic", ["ROOTS X^4-5X²+4", "Q"], ["X=⁻2", "X=⁻1", "X=1", "X=2", "4 REAL"]),
    ("calc roots irrational", ["ROOTS X³-2X-5", "Q"], ["X=2.094551482", "1 REAL + 2 COMPLEX"]),
    ("calc roots rational list", ["ROOTS 2X³-3X²-11X+6", "Q"], ["POSSIBLE RATIONAL ROOTS:", "X=1/2", "X=3", "X=⁻2"]),
    ("calc solve eq", ["X²=2X+3", "NO, GO BACK", "Q"], ["X=⁻1", "X=3"]),
    ("calc solve exp", ["SOLVE 2^X=X+3", "NO, GO BACK", "Q"], ["X=⁻2.86", "X=2.44"]),
    ("calc solve frac", ["SOLVE 3X=2", "NO, GO BACK", "Q"], ["X=2/3"]),
    ("calc ineq quad", ["X²-5X+6≥0", "NO, GO BACK", "Q"], ["SOLUTION: (-INF,2] U [3,INF)"]),
    ("calc ineq abs", ["abs(X-3)<5", "NO, GO BACK", "Q"], ["SOLUTION: (⁻2,8)"]),
    ("calc ineq linear", ["2X+1>7", "NO, GO BACK", "Q"], ["SOLUTION: (3,INF)"]),
    ("calc ineq touch", ["(X-2)²≤0", "NO, GO BACK", "Q"], ["SOLUTION: {2}"]),
    ("calc ineq touch strict", ["(X-2)²>0", "NO, GO BACK", "Q"], ["SOLUTION: (-INF,2) U (2,INF)"]),
    ("calc ineq all", ["X²+1>0", "NO, GO BACK", "Q"], ["SOLUTION: ALL REAL NUMBERS"]),
    ("calc ineq none", ["X²+1<0", "NO, GO BACK", "Q"], ["SOLUTION: NO SOLUTION"]),
    ("calc ineq rational", ["(X+1)/(X-2)≥0", "NO, GO BACK", "Q"], ["SOLUTION: (-INF,⁻1] U (2,INF)"]),
    ("calc ineq sqrt", ["√(X)<2", "NO, GO BACK", "Q"], ["SOLUTION: [0,4)"]),
    ("calc ineq cubic", ["X³-4X≤0", "NO, GO BACK", "Q"], ["SOLUTION: (-INF,⁻2] U [0,2]"]),
    ("calc solve graph", ["X²=4", "YES", "Q"], ["X=⁻2", "X=2"]),
    # ------------------------------------------------------------ algebra menu
    ("quadratic", ["M", "ALGEBRA", "QUADRATICS", "SOLVE + GRAPH", "2", "⁻4", "⁻6", "YES", "QUIT"],
     ["X1=3", "X2=⁻1", "VERTEX: (1,⁻8)", "FACTORED: 2(X-3)(X+1)"]),
    ("quadratic radical", ["M", "ALGEBRA", "QUADRATICS", "SOLVE + GRAPH", "1", "⁻2", "⁻1", "NO, GO BACK", "QUIT"],
     ["EXACT: X=(1+/-√(2))"]),
    ("quadratic complex", ["M", "ALGEBRA", "QUADRATICS", "SOLVE + GRAPH", "1", "2", "5", "NO, GO BACK", "QUIT"],
     ["X=⁻1+/-2𝑖", "EXACT: X=(⁻1+/-2𝑖)"]),
    ("complete square", ["M", "ALGEBRA", "QUADRATICS", "COMPLETE SQUARE", "2", "8", "3", "QUIT"],
     ["Y=2(X+2)²-5", "VERTEX: (⁻2, ⁻5)"]),
    ("complete square odd", ["M", "ALGEBRA", "QUADRATICS", "COMPLETE SQUARE", "1", "3", "1", "QUIT"],
     ["Y=(X+1.5)²-1.25", "VERTEX: (⁻3/2, ⁻5/4)"]),
    ("3 points", ["M", "ALGEBRA", "QUADRATICS", "FROM 3 POINTS", "{0,1,2}", "{1,2,5}", "QUIT"], ["Y=X²+1"]),
    ("vertical motion", ["M", "ALGEBRA", "QUADRATICS", "VERTICAL MOTION", "FEET (-16T²)", "6", "64", "QUIT"],
     ["MAX HEIGHT 70 AT T=2", "HITS THE GROUND AT T=4.0916"]),
    ("complex mult", ["M", "ALGEBRA", "QUADRATICS", "COMPLEX NUMBERS", "+ - * / COMPLEX", "2", "3", "4", "⁻5", "MULTIPLY *", "QUIT"],
     ["= 23+2𝑖"]),
    ("complex div", ["M", "ALGEBRA", "QUADRATICS", "COMPLEX NUMBERS", "+ - * / COMPLEX", "3", "2", "1", "⁻1", "DIVIDE /", "QUIT"],
     ["EXACT: 1/2 + (5/2)𝑖", "= .5+2.5𝑖"]),
    ("i power", ["M", "ALGEBRA", "QUADRATICS", "COMPLEX NUMBERS", "POWERS OF 𝑖", "27", "QUIT"], ["= -𝑖"]),
    ("i power neg", ["M", "ALGEBRA", "QUADRATICS", "COMPLEX NUMBERS", "POWERS OF 𝑖", "⁻1", "QUIT"], ["= -𝑖"]),
    ("sqrt negative", ["M", "ALGEBRA", "QUADRATICS", "COMPLEX NUMBERS", "SQRT OF NEGATIVE", "⁻48", "QUIT"], ["= 4𝑖√(3)"]),
    ("solve menu", ["M", "ALGEBRA", "SOLVE ANYTHING", "X³-2X=5", "⁻10", "10", "NO, GO BACK", "QUIT"], ["X=2.094551482"]),
    ("linear", ["M", "ALGEBRA", "LINEAR EQUATION", "2", "3", "5", "⁻1", "QUIT"], ["X=4/3"]),
    ("transform abs", ["M", "ALGEBRA", "TRANSFORMATIONS", "Y=|X|", "⁻2", "1", "3", "4", "YES", "QUIT"],
     ["REFLECT OVER THE X-AXIS", "VERTICAL STRETCH BY 2", "SHIFT RIGHT 3", "SHIFT UP 4", "RANGE: Y≤4"]),
    ("transform sqrt", ["M", "ALGEBRA", "TRANSFORMATIONS", "Y=√(X)", "1", "⁻1", "0", "0", "YES", "QUIT"],
     ["REFLECT OVER THE Y-AXIS", "DOMAIN: X≤0"]),
    ("transform 1/x", ["M", "ALGEBRA", "TRANSFORMATIONS", "Y=1/X", "1", "1", "2", "⁻3", "YES", "QUIT"],
     ["ASYMPTOTES: X=2, Y=⁻3"]),
    ("system 2x2", ["M", "ALGEBRA", "MORE EQUATIONS", "SYSTEM 2X2", "2", "3", "8", "1", "⁻1", "⁻1", "QUIT"], ["X=1", "Y=2"]),
    ("system 3x3", ["M", "ALGEBRA", "MORE EQUATIONS", "SYSTEM 3X3", "{1,1,1,6}", "{0,2,5,⁻4}", "{2,5,⁻1,27}", "QUIT"],
     ["X=5", "Y=3", "Z=⁻2"]),
    ("exponential", ["M", "ALGEBRA", "MORE EQUATIONS", "EXPONENTIAL", "3", "2", "24", "QUIT"], ["X=3"]),
    ("radical", ["M", "ALGEBRA", "SIMPLIFY RADICAL", "72", "QUIT"], ["√(72)=6√(2)"]),
    # ------------------------------------------------------------ polynomials menu
    ("poly expand", ["M", "POLYNOMIALS", "EXPAND", "(X+1)^4", "QUIT"], ["= X^4+4X³+6X²+4X+1"]),
    ("poly factor", ["M", "POLYNOMIALS", "FACTOR", "X³+2X²-5X-6", "QUIT"], ["= (X+1)(X+3)(X-2)"]),
    ("poly roots", ["M", "POLYNOMIALS", "ALL ROOTS (FTA)", "X^4-1", "QUIT"], ["X=⁻1", "X=1", "X=0+/-1𝑖", "2 REAL + 2 COMPLEX"]),
    ("poly graph", ["M", "POLYNOMIALS", "GRAPH FEATURES", "X³-4X", "YES", "QUIT"],
     ["LEFT END DOWN, RIGHT END UP", "ZERO X=⁻2 CROSSES", "ZERO X=0 CROSSES", "LOCAL MAX (⁻1.1547,3.0792)", "LOCAL MIN (1.1547,⁻3.0792)"]),
    ("poly graph touch", ["M", "POLYNOMIALS", "GRAPH FEATURES", "⁻(X-1)²(X+2)", "NO, GO BACK", "QUIT"],
     ["LEFT END UP, RIGHT END DOWN", "ZERO X=1 TOUCHES (MULT 2)", "ZERO X=⁻2 CROSSES (MULT 1)"]),
    ("poly long division", ["M", "POLYNOMIALS", "LONG DIVISION", "2X³+3X²-1", "X²+1", "QUIT"],
     ["QUOTIENT: 2X+3", "REMAINDER: -2X-4"]),
    ("poly long division exact", ["M", "POLYNOMIALS", "LONG DIVISION", "X³-6X²+11X-6", "X-1", "QUIT"],
     ["QUOTIENT: X²-5X+6", "REMAINDER: 0"]),
    ("poly build", ["M", "POLYNOMIALS", "BUILD FROM ROOTS", "{2,3𝑖}", "1", "QUIT"], ["P(X)=X³-2X²+9X-18"]),
    ("poly build lead", ["M", "POLYNOMIALS", "BUILD FROM ROOTS", "{1,⁻1,2}", "3", "QUIT"], ["P(X)=3X³-6X²-3X+6"]),
    ("binomial", ["M", "POLYNOMIALS", "MORE…", "BINOMIAL THEOREM", "2", "⁻1", "4", "QUIT"],
     ["PASCAL ROW: 1 4 6 4 1", "= 16X^4-32X³+24X²-8X+1"]),
    ("foil", ["M", "POLYNOMIALS", "MORE…", "FOIL", "2", "3", "1", "⁻4", "QUIT"], ["= 2X²-5X-12"]),
    ("synthetic", ["M", "POLYNOMIALS", "MORE…", "SYNTHETIC DIV", "X³-6X²+11X-6", "2", "QUIT"],
     ["QUOTIENT: X²-4X+3", "REMAINDER: 0", "IS A FACTOR"]),
    # ------------------------------------------------------------ rational (ch 7)
    ("rational graph", ["M", "RATIONAL (CH 7)", "GRAPH FEATURES", "X²-4", "X²-X-2", "YES", "QUIT"],
     ["HOLE AT (2,4/3)", "VERTICAL ASYMPTOTE X=⁻1", "HORIZONTAL ASYMPTOTE Y=1", "X-INTERCEPT (⁻2,0)", "Y-INTERCEPT (0,2)"]),
    ("rational slant", ["M", "RATIONAL (CH 7)", "GRAPH FEATURES", "X²+1", "X-1", "YES", "QUIT"],
     ["VERTICAL ASYMPTOTE X=1", "SLANT ASYMPTOTE Y=X+1", "NO X-INTERCEPTS", "Y-INTERCEPT (0,⁻1)"]),
    ("rational ha zero", ["M", "RATIONAL (CH 7)", "GRAPH FEATURES", "3", "X²-9", "YES", "QUIT"],
     ["VERTICAL ASYMPTOTE X=⁻3", "VERTICAL ASYMPTOTE X=3", "HORIZONTAL ASYMPTOTE Y=0", "Y-INTERCEPT (0,⁻1/3)"]),
    ("rational simplify", ["M", "RATIONAL (CH 7)", "SIMPLIFY", "X²-9", "X²+X-6", "QUIT"],
     ["= (X-3) / (X-2)", "X≠⁻3, 2"]),
    ("rational multiply", ["M", "RATIONAL (CH 7)", "MULTIPLY/DIVIDE", "MULTIPLY *", "X²-1", "X+2", "X+2", "X-1", "QUIT"],
     ["= (X+1)", "X≠⁻2, 1"]),
    ("rational divide", ["M", "RATIONAL (CH 7)", "MULTIPLY/DIVIDE", "DIVIDE /", "X", "X+1", "X²", "X²-1", "QUIT"],
     ["= (X-1) / X", "X≠⁻1, 0, 1"]),
    ("rational add", ["M", "RATIONAL (CH 7)", "ADD/SUBTRACT", "ADD +", "1", "X", "1", "X+1", "QUIT"],
     ["= (2X+1) / X(X+1)", "X≠⁻1, 0"]),
    ("rational subtract", ["M", "RATIONAL (CH 7)", "ADD/SUBTRACT", "SUBTRACT -", "X", "X-2", "2", "X-2", "QUIT"],
     ["= 1", "X≠2"]),
    ("rational solve", ["M", "RATIONAL (CH 7)", "SOLVE EQUATION", "1/X+1/(X+1)=3/2", "⁻10", "10", "NO, GO BACK", "QUIT"],
     ["X=⁻2/3", "X=1"]),
    # ------------------------------------------------------------ more math
    ("gcd", ["M", "MORE MATH", "NUMBERS + CODE", "GCD/LCM (EUCLID)", "84", "36", "QUIT"], ["GCD=12", "LCM=252"]),
    ("primes", ["M", "MORE MATH", "NUMBERS + CODE", "PRIME FACTORS", "360", "QUIT"], ["360=2^3*3^2*5", "24 DIVISORS"]),
    ("base", ["M", "MORE MATH", "NUMBERS + CODE", "BASE CONVERTER", "DEC TO BASE B", "255", "16", "QUIT"], ["255 = FF (BASE 16)"]),
    ("base back", ["M", "MORE MATH", "NUMBERS + CODE", "BASE CONVERTER", "BASE B TO DEC", "1011", "2", "QUIT"], ["= 11"]),
    ("bitwise", ["M", "MORE MATH", "NUMBERS + CODE", "BITWISE OPS", "12", "10", "QUIT"], ["AND=1000 (8)", "OR =1110 (14)", "XOR=0110 (6)"]),
    ("fraction", ["M", "MORE MATH", "NUMBERS + CODE", "FRACTION MATH", "1", "2", "1", "3", "ADD +", "QUIT"], ["ANSWER: 5/6"]),
    ("fibonacci", ["M", "MORE MATH", "NUMBERS + CODE", "SEQUENCES", "FIBONACCI", "10", "QUIT"], ["F10 = 55"]),
    ("collatz", ["M", "MORE MATH", "NUMBERS + CODE", "SEQUENCES", "COLLATZ (3N+1)", "6", "QUIT"], ["REACHED 1 IN 8 STEPS"]),
    ("arithmetic", ["M", "MORE MATH", "NUMBERS + CODE", "SEQUENCES", "ARITHMETIC", "2", "3", "10", "QUIT"], ["TERM 10 = 29", "= 155"]),
    ("geometric", ["M", "MORE MATH", "NUMBERS + CODE", "SEQUENCES", "GEOMETRIC", "3", ".5", "4", "QUIT"], ["TERM 4 = .375", "INFINITE SUM = 6"]),
    ("right triangle", ["M", "MORE MATH", "GEOMETRY + TRIG", "RIGHT TRIANGLE", "3", "4", "0", "QUIT"], ["C=5", "ANGLE A=36.86989765°"]),
    ("sss", ["M", "MORE MATH", "GEOMETRY + TRIG", "TRIANGLE: SSS", "3", "4", "5", "YES", "QUIT"], ["TYPE: RIGHT SCALENE", "AREA=6"]),
    ("sas", ["M", "MORE MATH", "GEOMETRY + TRIG", "TRIANGLE: SAS", "5", "7", "60", "NO, GO BACK", "QUIT"], ["A=6.245"]),
    ("asa", ["M", "MORE MATH", "GEOMETRY + TRIG", "TRIANGLE ASA/AAS", "40", "60", "10", "NO, GO BACK", "QUIT"], ["ANGLE C=180-A-B=80"]),
    ("two points", ["M", "MORE MATH", "GEOMETRY + TRIG", "2 POINTS + LINE", "1", "2", "4", "8", "QUIT"],
     ["DISTANCE=6.708203932", "EXACT: 3√(5)", "SLOPE=2", "LINE: Y=2X+0"]),
    ("circles", ["M", "MORE MATH", "GEOMETRY + TRIG", "CIRCLES + SOLIDS", "3", "4", "QUIT"],
     ["AREA=28.27433388", "VOLUME=113.0973355", "SLANT L=√(R²+H²)=5"]),
    ("derivative", ["M", "MORE MATH", "CALCULUS", "DERIVATIVE", "X³", "2", "YES", "QUIT"], ["F'(2)=12", "TANGENT: Y=12X-16"]),
    ("integral", ["M", "MORE MATH", "CALCULUS", "INTEGRAL + AREA", "X²", "0", "3", "YES", "QUIT"], ["INTEGRAL=9"]),
    ("riemann", ["M", "MORE MATH", "CALCULUS", "RIEMANN SUMS", "X²", "0", "2", "4", "YES", "QUIT"],
     ["LEFT SUM  =1.75", "RIGHT SUM =3.75", "SIMPSON   =2.666666667"]),
    ("newton", ["M", "MORE MATH", "CALCULUS", "NEWTONS METHOD", "X²-2", "1", "YES", "QUIT"], ["ROOT: X=1.414213562"]),
    ("limit", ["M", "MORE MATH", "CALCULUS", "LIMIT TABLE", "sin(X)/X", "0", "QUIT"], ["LIMIT IS ABOUT 1"]),
    ("stats", ["M", "MORE MATH", "STATS + MONEY", "1-VARIABLE STATS", "{2,4,4,4,5,5,7,9}", "QUIT"],
     ["MEAN=5", "MEDIAN=4.5", "POPULATION SD=2"]),
    ("regression", ["M", "MORE MATH", "STATS + MONEY", "LINE OF BEST FIT", "{1,2,3,4}", "{3,5,7,9}", "QUIT"],
     ["Y=2X+1", "R=1  R²=1"]),
    ("combos", ["M", "MORE MATH", "STATS + MONEY", "PERMS + COMBOS", "5", "2", "QUIT"], ["NPR=20", "NCR=10"]),
    ("interest", ["M", "MORE MATH", "STATS + MONEY", "INTEREST", "1000", "5", "12", "10", "QUIT"], ["FINAL AMOUNT=1647.01"]),
    ("percent", ["M", "MORE MATH", "STATS + MONEY", "PERCENT CHANGE", "80", "100", "QUIT"], ["UP 25%"]),
    ("loan", ["M", "MORE MATH", "STATS + MONEY", "LOAN PAYMENT", "10000", "6", "5", "QUIT"], ["MONTHLY PAYMENT=193.33"]),
    # ------------------------------------------------------------ first run (title screen plays)
    ("fresh install", ["2^10", "Q"], ["1024"]),
    # ------------------------------------------------------------ settings
    ("settings", ["M", "SETTINGS", "SHOW WORK ON/OFF", "ABOUT + STATUS", "BACK", "QUIT"],
     ["SHOW WORK IS NOW:", "OFF (ANSWERS ONLY)", "SHOW WORK: OFF"]),
    ("answers only", ["M", "SETTINGS", "SHOW WORK ON/OFF", "BACK", "CALCULATOR MODE", "FACTOR X²-1", "Q"], ["= (X-1)(X+1)"]),
    ("title screen", ["M", "SETTINGS", "TITLE SCREEN", "BACK", "QUIT"], ["TITLE SCREEN IS NOW:", "ON (PLAYS AT START)"]),
]


def main():
    verbose = "-v" in sys.argv
    pick = [a for a in sys.argv[1:] if a != "-v"]
    failed = 0
    ran = 0
    for name, inputs, wants in TESTS:
        if pick and not any(p in name for p in pick):
            continue
        ran += 1
        sim = Sim(DIST, inputs=inputs)
        if not name.startswith("fresh"):
            sim.lists["MPSET"] = [1, 1, 0, 1, 0, 0, 0, 0]   # skip the title screen
        err = None
        try:
            sim.run()
        except TIError as e:
            err = str(e)
        text = sim.text()
        # long lines wrap: at a space (next line starts with " ") or mid-word
        flat = text.replace("\n--- ClrHome ---\n", "\n").replace("\n ", " ")
        flat2 = flat.replace("\n", "")
        missing = [w for w in wants if w not in text and w not in flat and w not in flat2]
        if err or missing or sim.inputs:
            failed += 1
            print(f"FAIL {name}")
            if err:
                print(f"     error: {err}")
            for m in missing:
                print(f"     missing: {m}")
            if sim.inputs:
                print(f"     unused inputs: {sim.inputs}")
            if verbose or err or missing:
                print("     " + text.replace("\n", "\n     ")[-3000:])
        else:
            print(f"ok   {name}")
            if verbose:
                print("     " + text.replace("\n", "\n     "))
    print(f"\n{ran - failed}/{ran} passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
