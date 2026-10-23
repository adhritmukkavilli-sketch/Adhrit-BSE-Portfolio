"""A small TI-84 Plus CE TI-BASIC simulator, used to test MATHPRO without a calculator.

It runs the compiled .8xp files (so it tests exactly what goes on the calculator),
feeding scripted keypresses/inputs and recording what would appear on screen.
It covers the subset of TI-BASIC that MATHPRO uses, and follows the calculator's
rules closely: operator precedence, implicit multiplication, 1-based token strings,
Real vs a+bi mode, list math, and the usual error messages (ERR:DOMAIN, ...).
"""

import cmath
import math
import pathlib

from tivars import TIProgram
from tivars.tokenizer import decode, encode


class TIError(Exception):
    pass


class Done(Exception):
    pass


class ReturnFromProgram(Exception):
    pass


def tokens_of_bytes(data):
    toks, _ = decode(data)
    return [t.display.replace("\\'", "'") for t in toks]


def tokens_of_text(text):
    """Tokenize what a user types into an Input prompt."""
    data, _ = encode(text)
    return tokens_of_bytes(data)


LETTERS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZθ")
DIGITS = set("0123456789")
COLORS = {"BLUE": 10, "RED": 11, "BLACK": 12, "MAGENTA": 13, "GREEN": 14, "ORANGE": 15,
          "BROWN": 16, "NAVY": 17, "LTBLUE": 18, "YELLOW": 19, "WHITE": 20, "LTGRAY": 21,
          "MEDGRAY": 22, "GRAY": 23, "DARKGRAY": 24}
WINDOW = {"Xmin", "Xmax", "Ymin", "Ymax", "Xscl", "Yscl", "ΔX", "ΔY"}
REL = {"=", "≠", "<", ">", "≤", "≥"}


def is_func(t):
    return t.endswith("(") and t not in ("(",)


def split_statements(toks):
    """Split a token list into statements on newline and ':' outside strings."""
    stmts, cur, in_str = [], [], False
    for t in toks:
        if in_str:
            if t == '"':
                in_str = False
            elif t == "→" or t == "\n":
                in_str = False
                if t == "\n":
                    stmts.append(cur); cur = []
                    continue
            cur.append(t)
            continue
        if t == '"':
            in_str = True; cur.append(t)
        elif t in ("\n", ":"):
            stmts.append(cur); cur = []
        else:
            cur.append(t)
    stmts.append(cur)
    return [s for s in stmts]


# --------------------------------------------------------------------------- parser

class Parser:
    def __init__(self, toks):
        self.t = toks
        self.i = 0

    def peek(self, k=0):
        j = self.i + k
        return self.t[j] if j < len(self.t) else None

    def next(self):
        tok = self.peek()
        self.i += 1
        return tok

    def accept(self, tok):
        if self.peek() == tok:
            self.i += 1
            return True
        return False

    def close(self):
        # closing parens are optional at the end of a statement / before →
        if self.peek() in (None, "→"):
            return
        if not self.accept(")"):
            raise TIError(f"ERR:SYNTAX (expected ')' at {self.peek()!r} in {''.join(self.t)})")

    def at_end(self):
        return self.peek() in (None, "→", ",", ")", "}")

    # precedence: or/xor < and < relational < +- < */ implicit < nPr nCr < negation < ^ < postfix
    def expr(self):
        left = self.p_and()
        while self.peek() in (" or ", " xor "):
            op = self.next()
            right = self.p_and()
            left = ("bin", op.strip(), left, right)
        return left

    def p_and(self):
        left = self.p_rel()
        while self.peek() == " and ":
            self.next()
            right = self.p_rel()
            left = ("bin", "and", left, right)
        return left

    def p_rel(self):
        left = self.p_add()
        while self.peek() in REL:
            op = self.next()
            right = self.p_add()
            left = ("bin", op, left, right)
        return left

    def p_add(self):
        left = self.p_mul()
        while self.peek() in ("+", "-"):
            op = self.next()
            right = self.p_mul()
            left = ("bin", op, left, right)
        return left

    def starts_primary(self, t):
        if t is None:
            return False
        if t in DIGITS or t == "." or t in LETTERS or t.startswith("Str") or t in ("ʟ", "(", "{", '"', "π", "𝑖", "𝑒", "Ans") :
            return True
        if t in ("[A]", "[B]", "[C]") or t.startswith("Y₁"[:1]) and t in ("Y₁", "Y₂", "Y₃") or t in WINDOW or t in COLORS:
            return True
        if t in ("L₁", "L₂", "L₃", "L₄", "L₅", "L₆", "getKey"):
            return True
        if is_func(t) and t not in ("For(", "Menu(", "Output(", "Line(", "Pt-On(", "Text("):
            return True
        return False

    def p_mul(self):
        left = self.p_ncr()
        while True:
            t = self.peek()
            if t in ("*", "/"):
                self.next()
                right = self.p_ncr()
                left = ("bin", t, left, right)
            elif self.starts_primary(t):
                right = self.p_ncr()
                left = ("bin", "*", left, right)
            else:
                return left

    def p_ncr(self):
        left = self.p_neg()
        while self.peek() in (" nPr ", " nCr "):
            op = self.next().strip()
            right = self.p_neg()
            left = ("bin", op, left, right)
        return left

    def p_neg(self):
        if self.accept("⁻"):
            return ("neg", self.p_neg())
        return self.p_pow()

    def p_pow(self):
        left = self.p_post()
        while self.peek() == "^":
            self.next()
            if self.accept("⁻"):
                right = ("neg", self.p_powoperand())
            else:
                right = self.p_post()
            left = ("bin", "^", left, right)
        return left

    def p_powoperand(self):
        if self.accept("⁻"):
            return ("neg", self.p_powoperand())
        return self.p_post()

    def p_post(self):
        node = self.primary()
        while self.peek() in ("²", "³", "⁻¹", "!", "°"):
            node = ("post", self.next(), node)
        return node

    def number(self):
        s = ""
        while self.peek() is not None and (self.peek() in DIGITS or self.peek() == "."):
            s += self.next()
        if self.peek() == "ᴇ":
            self.next()
            e = ""
            if self.accept("⁻"):
                e = "-"
            while self.peek() is not None and self.peek() in DIGITS:
                e += self.next()
            s = (s or "1") + "e" + e
        return ("num", float(s))

    def args(self):
        out = []
        if self.peek() in (")", None, "→"):
            self.close()
            return out
        while True:
            out.append(self.expr())
            if self.accept(","):
                continue
            self.close()
            return out

    def primary(self):
        t = self.peek()
        if t is None:
            raise TIError("ERR:SYNTAX (unexpected end)")
        if t in DIGITS or t == "." or t == "ᴇ":
            return self.number()
        if t in LETTERS:
            self.next()
            return ("var", t)
        if t.startswith("Str") and len(t) == 4:
            self.next()
            return ("strvar", t)
        if t == '"':
            self.next()
            s = []
            while self.peek() is not None and self.peek() not in ('"', "→"):
                s.append(self.next())
            self.accept('"')
            return ("str", s)
        if t == "(":
            self.next()
            e = self.expr()
            self.close()
            return e
        if t == "{":
            self.next()
            items = []
            while True:
                items.append(self.expr())
                if self.accept(","):
                    continue
                if not self.accept("}") and self.peek() not in (None, "→"):
                    raise TIError("ERR:SYNTAX in list")
                break
            return ("listlit", items)
        if t == "ʟ":
            self.next()
            name = ""
            while len(name) < 5 and self.peek() is not None and (self.peek() in LETTERS or self.peek() in DIGITS):
                name += self.next()
            return self.list_ref(name)
        if t in ("L₁", "L₂", "L₃", "L₄", "L₅", "L₆"):
            self.next()
            return self.list_ref(t)
        if t in ("[A]", "[B]", "[C]"):
            self.next()
            if self.accept("("):
                a = self.expr(); self.accept(","); b = self.expr(); self.close()
                return ("matidx", t, a, b)
            return ("mat", t)
        if t in ("Y₁", "Y₂", "Y₃"):
            self.next()
            if self.accept("("):
                a = self.expr(); self.close()
                return ("ycall", t, a)
            return ("yvar", t)
        if t == "π":
            self.next(); return ("num", math.pi)
        if t == "𝑖":
            self.next(); return ("num", 1j)
        if t == "𝑒":
            self.next(); return ("num", math.e)
        if t == "Ans":
            self.next(); return ("ans",)
        if t == "getKey":
            self.next(); return ("getkey",)
        if t in WINDOW:
            self.next(); return ("win", t)
        if t in COLORS:
            self.next(); return ("num", float(COLORS[t]))
        if is_func(t):
            self.next()
            return ("func", t, self.args())
        raise TIError(f"ERR:SYNTAX (unexpected {t!r} in {''.join(self.t)})")

    def list_ref(self, name):
        if self.peek() == "(":
            self.next()
            idx = self.expr()
            self.close()
            return ("listidx", name, idx)
        return ("list", name)


# --------------------------------------------------------------------------- values

def fmt_real(x, mode):
    if x == 0:
        return "0"
    neg = x < 0
    a = abs(x)
    if mode["fix"] is not None and 1e-3 <= a < 1e10:
        s = f"{a:.{mode['fix']}f}"
    elif 1e-3 <= a < 1e10:
        s = f"{a:.10g}"
        if "e" in s:
            s = f"{a:.10f}".rstrip("0").rstrip(".")
    else:
        m, e = f"{a:.9e}".split("e")
        m = m.rstrip("0").rstrip(".")
        e = int(e)
        s = m + "ᴇ" + ("⁻" if e < 0 else "") + str(abs(e))
    if s.startswith("0.") and mode["fix"] is None:
        s = s[1:]
    return ("⁻" if neg else "") + s


def fmt_num(v, mode):
    if isinstance(v, complex):
        re, im = v.real, v.imag
        if im == 0:
            return fmt_real(re, mode)
        ims = fmt_real(abs(im), mode)
        ims = "" if ims == "1" else ims
        if re == 0:
            return ("⁻" if im < 0 else "") + ims + "𝑖"
        return fmt_real(re, mode) + ("-" if im < 0 else "+") + ims + "𝑖"
    return fmt_real(v, mode)


def norm(v):
    """Turn complex values with 0 imaginary part back into reals (like the TI does)."""
    if isinstance(v, complex):
        if v.imag == 0:
            return v.real
        return v
    if isinstance(v, list) and v and isinstance(v[0], list):
        return v
    if isinstance(v, list):
        return [norm(x) for x in v]
    return v


def chop(x):
    """TI floats have 14 digits; snap tiny rounding noise like 1e-15 away."""
    if isinstance(x, float):
        if x != 0 and abs(x) < 1e-99:
            return 0.0
        r = float(f"{x:.14g}")
        return r
    if isinstance(x, complex):
        re, im = chop(x.real), chop(x.imag)
        if im == 0:
            return re
        return complex(re, im)
    return x


class Sim:
    def __init__(self, dist, inputs=(), keys=(), max_steps=3_000_000):
        self.dist = pathlib.Path(dist)
        self.programs = {}
        self.vars = {c: 0.0 for c in LETTERS}
        self.strs = {}
        self.lists = {f"L₁": [], "L₂": [], "L₃": [], "L₄": [], "L₅": [], "L₆": []}
        self.mats = {}
        self.yvars = {}
        self.win = {"Xmin": -10.0, "Xmax": 10.0, "Ymin": -10.0, "Ymax": 10.0, "Xscl": 1.0, "Yscl": 1.0}
        self.mode = {"complex": False, "deg": False, "fix": None}
        self.ans = 0.0
        self.inputs = list(inputs)
        self.keys = list(keys)
        self.screen = []          # everything shown on the home screen, in order
        self.graph = []           # graph-screen commands that ran
        self.steps = 0
        self.max_steps = max_steps
        self.pauses = 0
        self.depth = 0

    # ------------------------------------------------------------- program loading
    def load(self, name):
        if name not in self.programs:
            path = self.dist / f"{name}.8xp"
            if not path.exists():
                raise TIError(f"ERR:UNDEFINED (prgm{name})")
            toks = tokens_of_bytes(TIProgram.open(str(path)).data)
            stmts = split_statements(toks)
            labels = {}
            for i, s in enumerate(stmts):
                if s and s[0] == "Lbl ":
                    labels["".join(s[1:])] = i
            self.programs[name] = (stmts, labels, {})
        return self.programs[name]

    # ------------------------------------------------------------- numbers
    def check(self, v):
        v = chop(v)
        if isinstance(v, complex) and not self.mode["complex"]:
            raise TIError("ERR:NONREAL ANS")
        if isinstance(v, (float, complex)) and abs(v) >= 1e100:
            raise TIError("ERR:OVERFLOW")
        return v

    def num(self, v, what="number"):
        if isinstance(v, (list, str)) or (isinstance(v, list)):
            raise TIError(f"ERR:DATA TYPE (wanted {what})")
        return v

    def real(self, v):
        v = self.num(v)
        if isinstance(v, complex):
            if v.imag != 0:
                raise TIError("ERR:DATA TYPE (complex where real needed)")
            return v.real
        return v

    def integer(self, v):
        v = self.real(v)
        if v != int(v):
            raise TIError("ERR:DOMAIN (needs integer)")
        return int(v)

    # ------------------------------------------------------------- evaluation
    def ev(self, n):
        k = n[0]
        if k == "num":
            return n[1]
        if k == "str":
            return list(n[1])
        if k == "var":
            return self.vars[n[1]]
        if k == "strvar":
            if n[1] not in self.strs:
                raise TIError(f"ERR:UNDEFINED ({n[1]})")
            return list(self.strs[n[1]])
        if k == "list":
            if n[1] not in self.lists:
                raise TIError(f"ERR:UNDEFINED (list {n[1]})")
            return list(self.lists[n[1]])
        if k == "listidx":
            lst = self.lists.get(n[1])
            if lst is None:
                raise TIError(f"ERR:UNDEFINED (list {n[1]})")
            i = self.integer(self.ev(n[2]))
            if not 1 <= i <= len(lst):
                raise TIError(f"ERR:INVALID DIM ({n[1]}({i}) of {len(lst)})")
            return lst[i - 1]
        if k == "listlit":
            out = []
            for e in n[1]:
                v = self.ev(e)
                if isinstance(v, list):
                    raise TIError("ERR:DATA TYPE")
                out.append(v)
            return out
        if k == "mat":
            return [row[:] for row in self.mats[n[1]]]
        if k == "matidx":
            m = self.mats[n[1]]
            i, j = self.integer(self.ev(n[2])), self.integer(self.ev(n[3]))
            if not (1 <= i <= len(m) and 1 <= j <= len(m[0])):
                raise TIError("ERR:INVALID DIM (matrix)")
            return m[i - 1][j - 1]
        if k == "yvar":
            return self.eval_y(n[1], self.vars["X"])
        if k == "ycall":
            return self.eval_y(n[1], self.ev(n[2]))
        if k == "win":
            if n[1] == "ΔX":
                return (self.win["Xmax"] - self.win["Xmin"]) / 264
            if n[1] == "ΔY":
                return (self.win["Ymax"] - self.win["Ymin"]) / 164
            return self.win[n[1]]
        if k == "ans":
            return self.ans
        if k == "getkey":
            return float(self.keys.pop(0)) if self.keys else 105.0
        if k == "neg":
            return self.unary(lambda a: -a, self.ev(n[1]))
        if k == "post":
            return self.post(n[1], self.ev(n[2]))
        if k == "bin":
            return self.binop(n[1], n[2], n[3])
        if k == "func":
            return self.func(n[1], n[2])
        raise TIError(f"internal: {k}")

    def eval_y(self, name, x):
        if name not in self.yvars:
            raise TIError(f"ERR:UNDEFINED ({name})")
        old = self.vars["X"]
        self.vars["X"] = x
        try:
            return self.ev(Parser(self.yvars[name]).expr())
        finally:
            self.vars["X"] = old

    def unary(self, f, a):
        if isinstance(a, list):
            return [self.check(f(x)) for x in a]
        return self.check(f(self.num(a)))

    def post(self, op, a):
        def f(x):
            if op == "²":
                return x * x
            if op == "³":
                return x * x * x
            if op == "⁻¹":
                if x == 0:
                    raise TIError("ERR:DIVIDE BY 0")
                return 1 / x
            if op == "!":
                xi = self.integer(x)
                if xi < 0 or xi > 69:
                    raise TIError("ERR:DOMAIN (factorial)")
                return float(math.factorial(xi))
            if op == "°":
                return x if self.mode["deg"] else x * math.pi / 180
            raise TIError(op)
        if isinstance(a, list) and a and isinstance(a[0], list):
            raise TIError("matrix postfix not supported")
        return self.unary(f, a)

    def power(self, a, b):
        if a == 0 and b == 0:
            raise TIError("ERR:DOMAIN (0^0)")
        if a == 0 and self.real(b) < 0:
            raise TIError("ERR:DIVIDE BY 0")
        if isinstance(a, complex) or isinstance(b, complex):
            return complex(a) ** complex(b)
        if a < 0 and b != int(b):
            if not self.mode["complex"]:
                raise TIError("ERR:NONREAL ANS")
            return complex(a) ** b
        try:
            return float(a) ** float(b)
        except OverflowError:
            raise TIError("ERR:OVERFLOW")

    def arith(self, op, a, b):
        if op == "+":
            return a + b
        if op == "-":
            return a - b
        if op == "*":
            return a * b
        if op == "/":
            if b == 0:
                raise TIError("ERR:DIVIDE BY 0")
            return a / b
        if op == "^":
            return self.power(a, b)
        if op == "nPr":
            n, r = self.integer(a), self.integer(b)
            if r < 0 or n < 0:
                raise TIError("ERR:DOMAIN")
            return float(math.perm(n, r)) if r <= n else 0.0
        if op == "nCr":
            n, r = self.integer(a), self.integer(b)
            if r < 0 or n < 0:
                raise TIError("ERR:DOMAIN")
            return float(math.comb(n, r)) if r <= n else 0.0
        if op in ("=", "≠"):
            eq = a == b
            return float(eq if op == "=" else not eq)
        if op in ("<", ">", "≤", "≥"):
            a, b = self.real(a), self.real(b)
            return float({"<": a < b, ">": a > b, "≤": a <= b, "≥": a >= b}[op])
        if op == "and":
            return float(bool(a) and bool(b))
        if op == "or":
            return float(bool(a) or bool(b))
        if op == "xor":
            return float(bool(a) != bool(b))
        raise TIError(op)

    def binop(self, op, an, bn):
        a = self.ev(an)
        b = self.ev(bn)
        sa = isinstance(a, list) and (not a or not isinstance(a[0], list)) and an[0] in ("str", "strvar", "func") and self.is_str(an, a)
        if self.is_str(an, a) or self.is_str(bn, b):
            if op == "+":
                if not (self.is_str(an, a) and self.is_str(bn, b)):
                    raise TIError("ERR:DATA TYPE (string + number)")
                return a + b
            if op in ("=", "≠"):
                eq = a == b
                return float(eq if op == "=" else not eq)
            raise TIError("ERR:DATA TYPE (string math)")
        if isinstance(a, list) and isinstance(b, list):
            if len(a) != len(b):
                raise TIError("ERR:DIM MISMATCH")
            return [self.check(self.arith(op, x, y)) for x, y in zip(a, b)]
        if isinstance(a, list):
            return [self.check(self.arith(op, x, b)) for x in a]
        if isinstance(b, list):
            return [self.check(self.arith(op, a, y)) for y in b]
        return self.check(self.arith(op, a, b))

    def is_str(self, node, val):
        """Strings are lists of tokens; tell them apart from number lists by origin."""
        if node[0] in ("str", "strvar"):
            return True
        if node[0] == "func" and node[1] in ("sub(", "toString(", "String(",):
            return True
        if node[0] == "bin" and node[1] == "+":
            return self.is_str(node[2], None) or self.is_str(node[3], None)
        return False

    # ------------------------------------------------------------- functions
    def bind_eval(self, expr, var, value):
        old = self.vars[var]
        self.vars[var] = value
        try:
            return self.ev(expr)
        finally:
            self.vars[var] = old

    def trig_in(self, x):
        return x * math.pi / 180 if self.mode["deg"] else x

    def trig_out(self, x):
        return x * 180 / math.pi if self.mode["deg"] else x

    def func(self, name, args):
        if name == "seq(":
            expr, var = args[0], args[1][1]
            a, b = self.real(self.ev(args[2])), self.real(self.ev(args[3]))
            step = self.real(self.ev(args[4])) if len(args) > 4 else 1.0
            out = []
            x = a
            while (step > 0 and x <= b + 1e-12) or (step < 0 and x >= b - 1e-12):
                out.append(self.num(self.bind_eval(expr, var, x)))
                x += step
                if len(out) > 999:
                    raise TIError("ERR:INVALID DIM (seq too long)")
            return out
        if name == "fnInt(":
            expr, var = args[0], args[1][1]
            a, b = self.real(self.ev(args[2])), self.real(self.ev(args[3]))
            n = 2000
            h = (b - a) / n
            s = 0
            for i in range(n + 1):
                w = 1 if i in (0, n) else (4 if i % 2 else 2)
                s += w * self.real(self.bind_eval(expr, var, a + i * h))
            return chop(s * h / 3)
        if name == "nDeriv(":
            expr, var = args[0], args[1][1]
            x = self.ev(args[2])
            h = self.real(self.ev(args[3])) if len(args) > 3 else 1e-3
            return chop((self.bind_eval(expr, var, x + h) - self.bind_eval(expr, var, x - h)) / (2 * h))
        if name in ("fMin(", "fMax("):
            expr, var = args[0], args[1][1]
            lo, hi = self.real(self.ev(args[2])), self.real(self.ev(args[3]))
            sgn = 1 if name == "fMin(" else -1
            g = (math.sqrt(5) - 1) / 2
            f = lambda x: sgn * self.real(self.bind_eval(expr, var, x))
            for _ in range(120):
                c, d = hi - g * (hi - lo), lo + g * (hi - lo)
                if f(c) < f(d):
                    hi = d
                else:
                    lo = c
            return (lo + hi) / 2

        vals = [self.ev(a) for a in args]
        V = vals

        def listmap(f, x):
            if isinstance(x, list):
                return [self.check(f(self.num(e))) for e in x]
            return self.check(f(self.num(x)))

        if name == "sub(":
            s = V[0]
            st, ln = self.integer(V[1]), self.integer(V[2])
            if st < 1 or ln < 1 or st + ln - 1 > len(s):
                raise TIError(f"ERR:DOMAIN (sub of length {len(s)} at {st},{ln})")
            return s[st - 1:st - 1 + ln]
        if name == "length(":
            return float(len(V[0]))
        if name == "toString(":
            v = V[0]
            if isinstance(v, list):
                raise TIError("ERR:DATA TYPE (toString of list)")
            return list(fmt_num(v, self.mode).replace("𝑖", "\x00")).copy() if False else self.num_tokens(v)
        if name == "inString(":
            s, sub = V[0], V[1]
            start = self.integer(V[2]) if len(V) > 2 else 1
            for i in range(start - 1, len(s) - len(sub) + 1):
                if s[i:i + len(sub)] == sub:
                    return float(i + 1)
            return 0.0
        if name == "expr(":
            return self.ev(Parser(V[0]).expr())
        if name == "abs(":
            return listmap(abs, V[0])
        if name == "√(":
            def sq(x):
                if isinstance(x, complex) or x < 0:
                    if not self.mode["complex"]:
                        raise TIError("ERR:NONREAL ANS")
                    return cmath.sqrt(x)
                return math.sqrt(x)
            return listmap(sq, V[0])
        if name in ("iPart(", "fPart(", "int("):
            def f(x):
                x = self.real(x)
                if name == "iPart(":
                    return float(math.trunc(x))
                if name == "int(":
                    return float(math.floor(x))
                return x - math.trunc(x)
            return listmap(f, V[0])
        if name == "round(":
            d = self.integer(V[1]) if len(V) > 1 else 9
            def f(x):
                if isinstance(x, complex):
                    return complex(round(x.real, d), round(x.imag, d))
                return float(round(x, d))
            return listmap(f, V[0])
        if name == "gcd(":
            a, b = self.real(V[0]), self.real(V[1])
            if a != int(a) or b != int(b) or a < 0 or b < 0:
                raise TIError(f"ERR:DOMAIN (gcd of {a},{b})")
            return float(math.gcd(int(a), int(b)))
        if name == "remainder(":
            a, b = self.integer(V[0]), self.integer(V[1])
            if b == 0:
                raise TIError("ERR:DIVIDE BY 0")
            return float(a % b if a >= 0 else -((-a) % b))
        if name in ("min(", "max("):
            f = min if name == "min(" else max
            if len(V) == 1:
                return f(self.real(x) for x in V[0])
            a, b = V
            if isinstance(a, list) or isinstance(b, list):
                raise TIError("min/max list form not supported")
            return f(self.real(a), self.real(b))
        if name == "not(":
            return listmap(lambda x: float(x == 0), V[0])
        if name == "sum(":
            return self.check(sum(V[0]))
        if name == "prod(":
            p = 1
            for x in V[0]:
                p *= x
            return self.check(p)
        if name == "dim(":
            v = V[0]
            if v and isinstance(v[0], list):
                return [float(len(v)), float(len(v[0]))]
            return float(len(v))
        if name == "augment(":
            return V[0] + V[1]
        if name == "ΔList(":
            if len(V[0]) < 2:
                raise TIError("ERR:INVALID DIM (ΔList)")
            return [self.check(V[0][i + 1] - V[0][i]) for i in range(len(V[0]) - 1)]
        if name == "mean(":
            return self.check(sum(V[0]) / len(V[0]))
        if name == "median(":
            s = sorted(self.real(x) for x in V[0]); n = len(s)
            return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2
        if name == "log(":
            return listmap(lambda x: self.logf(x, 10), V[0])
        if name == "ln(":
            return listmap(lambda x: self.logf(x, math.e), V[0])
        if name == "𝑒^(":
            return listmap(lambda x: cmath.exp(x) if isinstance(x, complex) else math.exp(x), V[0])
        if name == "₁₀^(":
            return listmap(lambda x: 10 ** x, V[0])
        if name in ("sin(", "cos(", "tan("):
            f = {"sin(": math.sin, "cos(": math.cos, "tan(": math.tan}[name]
            return listmap(lambda x: f(self.trig_in(self.real(x))), V[0])
        if name in ("sin⁻¹(", "cos⁻¹(", "tan⁻¹("):
            f = {"sin⁻¹(": math.asin, "cos⁻¹(": math.acos, "tan⁻¹(": math.atan}[name]
            def g(x):
                x = self.real(x)
                if name != "tan⁻¹(" and abs(x) > 1:
                    raise TIError("ERR:DOMAIN (inverse trig)")
                return self.trig_out(f(x))
            return listmap(g, V[0])
        if name == "real(":
            return listmap(lambda x: complex(x).real, V[0])
        if name == "imag(":
            return listmap(lambda x: complex(x).imag, V[0])
        if name == "conj(":
            return listmap(lambda x: complex(x).conjugate(), V[0])
        if name == "det(":
            return self.det(V[0])
        if name == "rref(":
            return self.rref(V[0])
        if name == "randInt(":
            import random
            return float(random.randint(self.integer(V[0]), self.integer(V[1])))
        raise TIError(f"function {name} not supported by simulator")

    def num_tokens(self, v):
        return list(fmt_num(v, self.mode))

    def logf(self, x, base):
        if x == 0:
            raise TIError("ERR:DOMAIN (log 0)")
        if isinstance(x, complex) or x < 0:
            if not self.mode["complex"]:
                raise TIError("ERR:NONREAL ANS")
            return cmath.log(x) / math.log(base)
        return math.log(x) / math.log(base)

    def det(self, m):
        n = len(m)
        a = [row[:] for row in m]
        d = 1.0
        for c in range(n):
            p = max(range(c, n), key=lambda r: abs(a[r][c]))
            if a[p][c] == 0:
                return 0.0
            if p != c:
                a[c], a[p] = a[p], a[c]; d = -d
            d *= a[c][c]
            for r in range(c + 1, n):
                f = a[r][c] / a[c][c]
                for k in range(c, n):
                    a[r][k] -= f * a[c][k]
        return chop(d)

    def rref(self, m):
        a = [row[:] for row in m]
        rows, cols = len(a), len(a[0])
        r = 0
        for c in range(cols):
            p = max(range(r, rows), key=lambda i: abs(a[i][c]), default=None)
            if p is None or abs(a[p][c]) < 1e-12:
                continue
            a[r], a[p] = a[p], a[r]
            pv = a[r][c]
            a[r] = [x / pv for x in a[r]]
            for i in range(rows):
                if i != r:
                    f = a[i][c]
                    a[i] = [x - f * y for x, y in zip(a[i], a[r])]
            r += 1
            if r == rows:
                break
        return [[chop(x) for x in row] for row in a]

    # ------------------------------------------------------------- output
    def show(self, text):
        self.screen.append(text)

    def disp_value(self, v, node):
        if self.is_str(node, v):
            self.show("".join(v))
        elif isinstance(v, list) and v and isinstance(v[0], list):
            for row in v:
                self.show("[" + " ".join(fmt_num(x, self.mode) for x in row) + "]")
        elif isinstance(v, list):
            self.show("{" + " ".join(fmt_num(x, self.mode) for x in v) + "}")
        else:
            self.show(" " * 10 + fmt_num(v, self.mode))

    def take_input(self, prompt):
        if not self.inputs:
            raise TIError(f"test ran out of inputs at prompt {prompt!r}")
        v = self.inputs.pop(0)
        self.show(f"{prompt}{v}")
        return v

    # ------------------------------------------------------------- store
    def store(self, val, target_toks, node_for_val):
        p = Parser(target_toks)
        t = p.primary() if target_toks[0] != "dim(" else None
        if target_toks[0] == "dim(":
            p = Parser(target_toks)
            p.next()
            inner = p.primary()
            p.close()
            if inner[0] == "list":
                n = self.integer(val)
                lst = self.lists.setdefault(inner[1], [])
                self.lists[inner[1]] = (lst + [0.0] * n)[:n]
            elif inner[0] == "mat":
                r, c = int(val[0]), int(val[1])
                old = self.mats.get(inner[1], [])
                self.mats[inner[1]] = [[(old[i][j] if i < len(old) and j < len(old[i]) else 0.0) for j in range(c)] for i in range(r)]
            return
        k = t[0]
        if k == "var":
            if isinstance(val, list):
                raise TIError("ERR:DATA TYPE (list into number)")
            self.vars[t[1]] = val
        elif k == "strvar":
            if not self.is_str(node_for_val, val):
                raise TIError("ERR:DATA TYPE (number into string)")
            self.strs[t[1]] = list(val)
        elif k == "list":
            if not isinstance(val, list):
                raise TIError("ERR:DATA TYPE (number into list)")
            self.lists[t[1]] = [norm(x) for x in val]
        elif k == "listidx":
            lst = self.lists.setdefault(t[1], [])
            i = self.integer(self.ev(t[2]))
            if not 1 <= i <= len(lst) + 1:
                raise TIError(f"ERR:INVALID DIM (store {t[1]}({i}) dim {len(lst)})")
            if isinstance(val, list):
                raise TIError("ERR:DATA TYPE")
            if i == len(lst) + 1:
                lst.append(val)
            else:
                lst[i - 1] = val
        elif k == "matidx":
            m = self.mats[t[1]]
            i, j = self.integer(self.ev(t[2])), self.integer(self.ev(t[3]))
            m[i - 1][j - 1] = val
        elif k == "mat":
            self.mats[t[1]] = [row[:] for row in val]
        elif k == "yvar":
            if not self.is_str(node_for_val, val):
                raise TIError("ERR:DATA TYPE (Y= needs a string)")
            self.yvars[t[1]] = list(val)
        elif k == "win":
            self.win[t[1]] = self.real(val)
        else:
            raise TIError(f"cannot store into {target_toks}")

    # ------------------------------------------------------------- running
    def run(self, name="MATHPRO"):
        try:
            self.call(name)
        except Done:
            pass
        return self

    def call(self, name):
        self.depth += 1
        if self.depth > 20:
            raise TIError("ERR:MEMORY (program nesting)")
        stmts, labels, cache = self.load(name)
        try:
            self.exec_prog(stmts, labels, cache)
        except ReturnFromProgram:
            pass
        finally:
            self.depth -= 1

    def find_block_end(self, stmts, i, stop_at_else):
        """From statement i (just after an opener), find the matching Else/End."""
        depth = 0
        while i < len(stmts):
            s = stmts[i]
            head = s[0] if s else None
            if head in ("While ", "Repeat ", "For(") or (head == "Then"):
                depth += 1
            elif head == "End":
                if depth == 0:
                    return i
                depth -= 1
            elif head == "Else" and depth == 0 and stop_at_else:
                return i
            i += 1
        raise TIError("ERR:SYNTAX (missing End)")

    def cond(self, toks, cache, key):
        if key not in cache:
            cache[key] = Parser(toks).expr()
        v = self.ev(cache[key])
        if isinstance(v, list):
            raise TIError("ERR:DATA TYPE (list as condition)")
        return v != 0

    def exec_prog(self, stmts, labels, cache):
        pc = 0
        stack = []
        while pc < len(stmts):
            self.steps += 1
            if self.steps > self.max_steps:
                raise TIError("simulator: too many steps (infinite loop?)")
            s = stmts[pc]
            if not s:
                pc += 1
                continue
            head = s[0]
            if head == "If ":
                ok = self.cond(s[1:], cache, ("if", pc))
                nxt = stmts[pc + 1] if pc + 1 < len(stmts) else []
                if nxt and nxt[0] == "Then":
                    if ok:
                        stack.append(("if", pc))
                        pc += 2
                    else:
                        j = self.find_block_end(stmts, pc + 2, True)
                        if stmts[j][0] == "Else":
                            stack.append(("else", pc))
                        pc = j + 1
                else:
                    pc += 1 if ok else 2
                continue
            if head == "Else":
                # reached the end of a true If-branch: skip the Else part
                j = self.find_block_end(stmts, pc + 1, False)
                stack.pop()
                pc = j + 1
                continue
            if head == "Then":
                raise TIError("ERR:SYNTAX (stray Then)")
            if head == "While ":
                if self.cond(s[1:], cache, ("w", pc)):
                    stack.append(("while", pc))
                    pc += 1
                else:
                    pc = self.find_block_end(stmts, pc + 1, False) + 1
                continue
            if head == "Repeat ":
                stack.append(("repeat", pc))
                pc += 1
                continue
            if head == "For(":
                if ("for", pc) not in cache:
                    p = Parser(s[1:])
                    var = p.next()
                    p.accept(",")
                    args = []
                    while p.peek() not in (None,):
                        args.append(p.expr())
                        if not p.accept(","):
                            break
                    p.close()
                    cache[("for", pc)] = (var, args)
                var, args = cache[("for", pc)]
                start, end = self.real(self.ev(args[0])), self.real(self.ev(args[1]))
                step = self.real(self.ev(args[2])) if len(args) > 2 else 1.0
                self.vars[var] = start
                if (step > 0 and start > end) or (step < 0 and start < end):
                    pc = self.find_block_end(stmts, pc + 1, False) + 1
                else:
                    stack.append(("for", pc, var, end, step))
                    pc += 1
                continue
            if head == "End":
                if not stack:
                    raise TIError("ERR:SYNTAX (End without block)")
                top = stack[-1]
                kind = top[0]
                if kind in ("if", "else"):
                    stack.pop(); pc += 1
                elif kind == "while":
                    stack.pop(); pc = top[1]
                elif kind == "repeat":
                    rs = stmts[top[1]]
                    if self.cond(rs[1:], cache, ("r", top[1])):
                        stack.pop(); pc += 1
                    else:
                        pc = top[1] + 1
                elif kind == "for":
                    _, fpc, var, end, step = top
                    self.vars[var] = self.real(self.vars[var]) + step
                    v = self.vars[var]
                    if (step > 0 and v > end + 1e-12) or (step < 0 and v < end - 1e-12):
                        stack.pop(); pc += 1
                    else:
                        pc = fpc + 1
                continue
            if head == "Goto ":
                lab = "".join(s[1:])
                if lab not in labels:
                    raise TIError(f"ERR:LABEL ({lab})")
                if stack:
                    raise TIError(f"Goto {lab} from inside a block (memory leak)")
                pc = labels[lab] + 1
                continue
            if head == "Lbl ":
                pc += 1
                continue
            if head == "Menu(":
                if stack:
                    raise TIError("Menu( inside a block (memory leak)")
                p = Parser(s[1:])
                title = p.primary()
                items = []
                while p.accept(","):
                    text = "".join(p.primary()[1])
                    p.accept(",")
                    lab = ""
                    while p.peek() not in (",", ")", None):
                        lab += p.next()
                    items.append((text, lab))
                choice = self.take_input(f"MENU[{''.join(title[1])}] ")
                for text, lab in items:
                    if choice == text or (choice.isdigit() and items[int(choice) - 1][0] == text):
                        pc = labels[lab] + 1
                        break
                else:
                    raise TIError(f"menu choice {choice!r} not in {[t for t, _ in items]}")
                continue
            if head == "prgm":
                self.call("".join(s[1:]))
                pc += 1
                continue
            if head == "Return":
                raise ReturnFromProgram()
            if head == "Stop":
                raise Done()
            self.statement(s, cache, pc)
            pc += 1
        if self.depth == 1:
            raise Done()

    def statement(self, s, cache, pc):
        head = s[0]
        rest = s[1:]
        simple = {"ClrHome", "Float", "Real", "a+b𝑖", "Func", "Degree", "Radian", "ExprOff", "AxesOn", "AxesOn ", "AxesOff",
                  "GridOff", "LabelOff", "FnOff ", "PlotsOff ", "ClrDraw", "DispGraph", "ZStandard",
                  "SetUpEditor ", "DetectAsymptotesOn", "CoordOff", "Pause "}
        if head == "ClrHome":
            self.show("--- ClrHome ---")
        if head == "Float":
            self.mode["fix"] = None
        elif head == "Fix ":
            self.mode["fix"] = int("".join(rest))
        elif head == "Real":
            self.mode["complex"] = False
        elif head == "a+b𝑖":
            self.mode["complex"] = True
        elif head == "Degree":
            self.mode["deg"] = True
        elif head == "Radian":
            self.mode["deg"] = False
        elif head == "SetUpEditor ":
            if rest:
                self.lists.setdefault("".join(rest), [])
        elif head == "Pause ":
            self.pauses += 1
            if rest:
                self.disp_value(self.ev(Parser(rest).expr()), Parser(rest).expr())
        elif head in ("DispGraph", "ZStandard"):
            if head == "ZStandard":
                self.win.update({"Xmin": -10.0, "Xmax": 10.0, "Ymin": -10.0, "Ymax": 10.0})
            self.check_window()
        if head in simple:
            return
        if head == "FnOn ":
            return
        if head == "ZoomFit":
            self.zoomfit()
            return
        if head == "Disp ":
            p = Parser(rest)
            while True:
                node = p.expr()
                self.disp_value(self.ev(node), node)
                if not p.accept(","):
                    break
            return
        if head == "Output(":
            p = Parser(rest)
            r = self.integer(self.ev(p.expr())); p.accept(",")
            c = self.integer(self.ev(p.expr())); p.accept(",")
            node = p.expr()
            v = self.ev(node)
            if not (1 <= r <= 10 and 1 <= c <= 26):
                raise TIError("ERR:DOMAIN (Output position)")
            self.show(f"[Output {r},{c}] " + ("".join(v) if self.is_str(node, v) else fmt_num(v, self.mode)))
            return
        if head == "Prompt ":
            for name in "".join(rest).split(","):
                raw = self.take_input(f"{name}=?")
                self.vars[name] = self.ev(Parser(tokens_of_text(raw)).expr())
            return
        if head == "Input ":
            p = Parser(rest)
            prompt = "?"
            if p.peek() == '"':
                prompt = "".join(p.primary()[1])
                p.accept(",")
            tgt = rest[p.i:]
            raw = self.take_input(prompt)
            node = Parser(tgt).primary() if tgt[0] != "ʟ" else Parser(tgt).primary()
            if node[0] == "strvar":
                self.strs[node[1]] = tokens_of_text(raw) if raw else []
            else:
                v = self.ev(Parser(tokens_of_text(raw)).expr())
                self.store(v, tgt, ("num", 0))
            return
        if head in ("Line(", "Pt-On(", "Pt-Off(", "Circle(", "Shade(", "GraphColor(", "TextColor(", "Text("):
            vals = [self.ev(a) for a in Parser(rest).args()] if head != "Shade(" else []
            if head == "Text(":
                big = vals[0] == -1
                row, col = (vals[1], vals[2]) if big else (vals[0], vals[1])
                if not (0 <= self.real(row) <= 164 and 0 <= self.real(col) <= 264):
                    raise TIError(f"ERR:DOMAIN (Text at {row},{col})")
            if head in ("Line(", "Pt-On(", "Circle(", "Text(", "Shade("):
                self.check_window()
            self.graph.append((head, vals))
            return
        if head in ("DrawF ", "Vertical ", "Horizontal "):
            Parser(rest).args() if False else None
            self.check_window()
            self.graph.append((head, "".join(rest)))
            return
        if head == "String►Equ(":
            p = Parser(rest)
            sv = p.primary(); p.accept(",")
            y = p.next()
            self.yvars[y] = list(self.ev(sv))
            return
        if head == "SortA(":
            p = Parser(rest)
            node = p.primary()
            lst = self.lists[node[1]]
            lst.sort(key=lambda x: self.real(x))
            return
        if head == "DelVar ":
            return
        # assignment or bare expression
        if ("st", pc) not in cache:
            if "→" in s:
                # the last → at top level (strings end at → anyway)
                k = len(s) - 1 - s[::-1].index("→")
                cache[("st", pc)] = (Parser(s[:k]).expr(), s[k + 1:])
            else:
                cache[("st", pc)] = (Parser(s).expr(), None)
        node, target = cache[("st", pc)]
        v = self.ev(node)
        if target is None:
            self.ans = v
        else:
            self.store(v, target, node)
            self.ans = v

    def check_window(self):
        w = self.win
        if not (w["Xmin"] < w["Xmax"] and w["Ymin"] < w["Ymax"]):
            raise TIError(f"ERR:WINDOW RANGE {w}")

    def zoomfit(self):
        self.check_window()
        ys = []
        a, b = self.win["Xmin"], self.win["Xmax"]
        for i in range(265):
            try:
                y = self.eval_y("Y₁", a + (b - a) * i / 264)
                if not isinstance(y, complex):
                    ys.append(y)
            except TIError:
                pass
        if not ys:
            raise TIError("ERR:WINDOW RANGE (ZoomFit found nothing)")
        lo, hi = min(ys), max(ys)
        if lo == hi:
            raise TIError("ERR:WINDOW RANGE (ZoomFit flat)")
        self.win["Ymin"], self.win["Ymax"] = lo, hi

    def text(self):
        return "\n".join(self.screen)
