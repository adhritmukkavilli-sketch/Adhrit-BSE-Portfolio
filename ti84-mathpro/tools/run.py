"""Run MATHPRO in the simulator with scripted inputs: python3 tools/run.py "input1" "input2" ..."""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from tisim import Sim, TIError
dist = pathlib.Path(__file__).parent.parent / "dist"
s = Sim(dist, inputs=sys.argv[1:])
try:
    s.run()
    print(s.text())
except TIError as e:
    print(s.text())
    print("\n!!!", e)
