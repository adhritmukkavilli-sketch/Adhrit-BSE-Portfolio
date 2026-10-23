#!/usr/bin/env python3
"""Build MATHPRO's TI-BASIC source (src/*.tib) into TI-84 Plus CE .8xp files.

    pip install tivars
    python3 build.py --from "ADHRIT"

Source format:
    # comment         removed
    SHOW expr         expr→Str0  +  prgmZMPSHOW    (a step of work)
    ANS expr          "*"+expr→Str0 + prgmZMPSHOW  (an answer, always shown)
    leading spaces    removed (indent however you like)

The build also lints for TI-BASIC mistakes that only show up on the calculator:
unbalanced blocks, Goto/Menu( inside blocks (memory leak), missing labels,
"-" used for a negative number, long menu items, and so on.
"""

import argparse
import pathlib
import re
import sys

from tivars import TIProgram
from tivars.tokenizer import encode

ROOT = pathlib.Path(__file__).parent
SRC = ROOT / "src"
DIST = ROOT / "dist"

# Commands that need their trailing space to be recognized as a token
BARE_COMMANDS = {"Pause", "SetUpEditor", "FnOff", "PlotsOff", "PlotsOn", "FnOn"}

# tivars can't type π inside strings, so we swap it for a placeholder token
# (ʳ, the radian mark, never used in this project) and patch the bytes after.
PI_PLACEHOLDER = "ʳ"
PI_PLACEHOLDER_BYTE = 0x0A
PI_BYTE = 0xAC
TWO_BYTE_PREFIXES = {0x5C, 0x5D, 0x5E, 0x60, 0x61, 0x62, 0x63, 0x7E, 0xAA, 0xBB, 0xEF}


def split_code_strings(line):
    """Yield (is_string, text) chunks. Strings end at a quote, → or end of line."""
    out, buf, in_str = [], "", False
    for ch in line:
        if in_str:
            if ch == '"':
                out.append((True, buf + ch)); buf, in_str = "", False
            elif ch == "→":
                out.append((True, buf)); buf, in_str = ch, False
            else:
                buf += ch
        else:
            if ch == '"':
                out.append((False, buf)); buf, in_str = ch, True
            else:
                buf += ch
    out.append((in_str, buf))
    return [c for c in out if c[1]]


def statements(line):
    """Split a line on ':' that are outside strings."""
    parts, buf = [], ""
    for is_str, text in split_code_strings(line):
        if is_str:
            buf += text
        else:
            pieces = text.split(":")
            buf += pieces[0]
            for p in pieces[1:]:
                parts.append(buf); buf = p
    parts.append(buf)
    return [p.strip() for p in parts]


def preprocess(text, subs):
    for key, value in subs.items():
        text = text.replace("{{%s}}" % key, value)
    lines = []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line in BARE_COMMANDS:
            line += " "
        m = re.match(r"^(SHOW|ANS) (.+)$", line)
        if m:
            expr = m.group(2)
            if m.group(1) == "ANS":
                expr = '"*"+' + expr
            lines.append((n, expr + "→Str0"))
            lines.append((n, "prgmZMPSHOW"))
        else:
            lines.append((n, line))
    return lines


def lint(name, lines, errors, warnings):
    depth = 0
    labels, targets = {}, []
    prev_bare_if = False
    for n, line in lines:
        where = f"{name}.tib:{n}"
        if "{{" in line:
            errors.append(f"{where}: unfilled placeholder: {line}")
        if PI_PLACEHOLDER in line:
            errors.append(f"{where}: {PI_PLACEHOLDER} is reserved by the build")
        for is_str, text in split_code_strings(line):
            if is_str:
                if '"' in text[1:-1]:
                    errors.append(f"{where}: quote inside string: {line}")
                continue
            if re.search(r"(^|[(,=+*/^{:<>≤≥≠→])\s*-", text) or text.lstrip().startswith("-"):
                errors.append(f"{where}: '-' used as a negative sign (use ⁻): {line}")
        for st in statements(line):
            if prev_bare_if and st == "prgmZMPSHOW":
                errors.append(f"{where}: SHOW/ANS right after a one-line If (needs Then/End)")
            opens = st == "Then" or st.startswith(("While ", "For(", "Repeat "))
            if st.startswith("If ") and "Then" not in st:
                prev_bare_if = True
            elif st == "Then":
                prev_bare_if = False
            else:
                prev_bare_if = False
            if opens:
                depth += 1
            if st == "End":
                depth -= 1
                if depth < 0:
                    errors.append(f"{where}: End without a block"); depth = 0
            m = re.match(r"^Lbl (\S+)$", st)
            if m:
                lab = m.group(1)
                if len(lab) > 2:
                    errors.append(f"{where}: label too long: {lab}")
                if lab in labels:
                    errors.append(f"{where}: duplicate label {lab}")
                labels[lab] = n
                if depth:
                    warnings.append(f"{where}: Lbl inside a block")
            m = re.match(r"^Goto (\S+)$", st)
            if m:
                targets.append((where, m.group(1)))
                if depth:
                    errors.append(f"{where}: Goto inside a block leaks memory")
            if st.startswith("Menu("):
                if depth:
                    errors.append(f"{where}: Menu( inside a block leaks memory")
                strs = re.findall(r'"([^"]*)"', st)
                labs = re.findall(r'",(\w+)', st)
                if len(labs) > 7:
                    errors.append(f"{where}: Menu( has more than 7 items")
                for s in strs:
                    if len(s) > 16:
                        warnings.append(f"{where}: menu text over 16 chars: {s}")
                for lab in labs:
                    targets.append((where, lab))
            if st.startswith(("Disp ", "Output(")):
                for s in re.findall(r'"([^"]*)"', st):
                    if len(s) > 26:
                        warnings.append(f"{where}: text over 26 chars: {s}")
    if depth:
        errors.append(f"{name}: {depth} block(s) never closed with End")
    for where, t in targets:
        if t not in labels:
            errors.append(f"{where}: Goto/Menu target {t} has no Lbl")


def tokenize(lines):
    text = "\n".join(line for _, line in lines).replace("π", PI_PLACEHOLDER)
    data, _ = encode(text)
    out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        if b in TWO_BYTE_PREFIXES:
            out += data[i:i + 2]; i += 2
        else:
            out.append(PI_BYTE if b == PI_PLACEHOLDER_BYTE else b); i += 1
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from", dest="sender", default="ADHRIT", help="your name")
    args = ap.parse_args()

    sender = args.sender.upper()
    if len(sender) > 18:
        sys.exit("Your name is too long for the screen (18 characters max)")
    subs = {"FROM": sender}

    errors, warnings = [], []
    built = {}
    for path in sorted(SRC.glob("*.tib")):
        name = path.stem
        lines = preprocess(path.read_text(encoding="utf-8"), subs)
        lint(name, lines, errors, warnings)
        built[name] = lines

    for w in warnings:
        print("warning:", w)
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)

    DIST.mkdir(exist_ok=True)
    for name, lines in built.items():
        prog = TIProgram(name=name)
        prog.data = tokenize(lines)
        out = DIST / f"{name}.8xp"
        prog.save(str(out))
        print(f"built {out.relative_to(ROOT)}  ({len(prog.data)} bytes, {len(lines)} lines)")


if __name__ == "__main__":
    main()
