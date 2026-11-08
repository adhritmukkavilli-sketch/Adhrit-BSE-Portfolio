// Desktop simulator for the plasma-scan sketches.
// Mocks the Arduino API and models the stage: step counting, backlash (dead band), wrong steps/mm,
// lost steps, switch hysteresis and switch noise. Prints the sketch output plus "[sim]" lines
// with where the carriage REALLY is.
//
// build: g++ -O2 -std=c++17 -DSKETCH='"../plasma_scan_v2/plasma_scan_v2.ino"' sim.cpp -o sim
// run:   ./sim [options] -- CMD1 CMD2 ...      (each CMD is sent as one line when the sketch is idle)
//
// options (the simulated machine's faults):
//   bx=<mm> by=<mm>     backlash (play) on X / Y
//   sx=<n>  sy=<n>      REAL steps per mm of X / Y (the sketch still uses its own constants)
//   lx=<n>  fx=<us>     X skips every n-th step while moving away from home, only when the step
//                       period is shorter than fx microseconds (too fast)
//   g=<p>               switch noise: chance that one read of an un-pressed switch reads HIGH
//   x0=<mm> y0=<mm>     where the carriage starts (mm from each switch)
//   q                   quiet: only print the per-stop table at the end
// e.g.  ./sim bx=0.3 -- G          (the scan on a machine with 0.3 mm play in X)
//       ./sim bx=0.3 sx=20.4 -- T2 T3 "K 1" T3 T1      (position_test; "the user" answers with caliper readings)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <deque>
#include <vector>
#include <random>

typedef uint8_t byte;
using std::isnan;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define A0 54
#define F(x) (x)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

// ---------------- simulated machine ----------------
static uint64_t simUs = 0;
static int pinState[70];
static std::mt19937 rng(1);

struct Axis {
  const char *name;
  int stepPin, dirPin, enaPin, limitPin, awayLevel;
  double spmTrue;          // real steps per mm
  double m = 0, c = 0;     // motor / carriage position in steps (+ = away from home). switch trips at c <= 0
  double b = 0;            // backlash in steps
  double hyst = 2;         // switch hysteresis in steps
  bool sw = false;
  long loseEvery = 0;      // lose every Nth step (0 = never)
  bool loseOnlyFast = false; unsigned int fastUs = 0;  // lose only when the step period is shorter than fastUs
  long steps = 0;
  uint64_t lastStepUs = 0;
  void update() { if (c <= 0) sw = true; else if (c >= hyst) sw = false; }
  void step(int dir) {
    steps++;
    unsigned int period = (unsigned int)(simUs - lastStepUs);
    lastStepUs = simUs;
    bool lose = loseEvery && steps % loseEvery == 0 && (!loseOnlyFast || period < fastUs);
    if (lose && dir > 0) return;   // skips only while moving away (e.g. fighting friction one way)
    m += dir;
    if (m > c) c = m; else if (m < c - b) c = m + b;   // dead band: carriage only moves when pushed
    update();
  }
};
static Axis X{"X", 10, 9, 8, 2, HIGH, 20.0};
static Axis Y{"Y", 7, 4, 5, 3, HIGH, 160.0};
static double glitchProb = 0;  // chance that one read of an un-pressed switch reads HIGH (noise)
static long glitchesInjected = 0;

static void pinMode(int, int) {}
static void digitalWrite(int pin, int v) {
  int old = pinState[pin];
  pinState[pin] = v;
  for (Axis *a : {&X, &Y})
    if (pin == a->stepPin && old == LOW && v == HIGH && pinState[a->enaPin] == LOW)
      a->step(pinState[a->dirPin] == a->awayLevel ? +1 : -1);
}
static int digitalRead(int pin) {
  for (Axis *a : {&X, &Y})
    if (pin == a->limitPin) {
      if (a->sw) return HIGH;
      if (glitchProb > 0 && std::uniform_real_distribution<double>(0, 1)(rng) < glitchProb) { glitchesInjected++; return HIGH; }
      return LOW;
    }
  return LOW;
}
static int analogRead(int) { return 256 + (int)(rng() % 3); }  // ~ room temperature
static unsigned long millis() { return (unsigned long)(simUs / 1000); }
static unsigned long micros() { return (unsigned long)simUs; }
static void timeGuard() { if (simUs > 6ULL * 3600 * 1000000) { printf("\n[sim] TIMEOUT: sketch stuck (waiting for input?)\n"); exit(2); } }
static void delay(unsigned long ms) { simUs += ms * 1000ULL; timeGuard(); }
static void delayMicroseconds(unsigned int us) { simUs += us; timeGuard(); }

// ---------------- Serial ----------------
void onOutputLine(const std::string &s);
static std::deque<char> serialIn;
static std::deque<std::pair<uint64_t, std::string>> pendingIn;   // typed by the simulated user later
static void pumpPending() {
  while (!pendingIn.empty() && pendingIn.front().first <= simUs) {
    for (char ch : pendingIn.front().second) serialIn.push_back(ch);
    pendingIn.pop_front();
  }
}
static std::string outLine;
static bool quiet = false;
struct SerialMock {
  void begin(long) {}
  int available() { pumpPending(); return (int)serialIn.size(); }
  int read() { if (serialIn.empty()) return -1; char c = serialIn.front(); serialIn.pop_front(); return c; }
  void put(const std::string &s) {
    for (char ch : s) {
      if (ch == '\n') { if (!quiet) puts(outLine.c_str()); std::string l = outLine; outLine.clear(); onOutputLine(l); }
      else outLine += ch;
    }
  }
  void print(const char *s) { put(s); }
  void print(char c) { put(std::string(1, c)); }
  void print(int v) { put(std::to_string(v)); }
  void print(long v) { put(std::to_string(v)); }
  void print(unsigned int v) { put(std::to_string(v)); }
  void print(unsigned long v) { put(std::to_string(v)); }
  void print(double v, int d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); put(b); }
  template <class T> void println(T v) { print(v); put("\n"); }
  void println(double v, int d) { print(v, d); put("\n"); }
  void println() { put("\n"); }
} Serial;

// ---------------- the sketch ----------------
#include SKETCH

// ---------------- harness ----------------
static double homeX = NAN, homeY = NAN;   // where the carriage REALLY was when the sketch called it home
static double zeroRef[2] = {0, 0};
static double worstX = 0, worstY = 0;
static int pointsSeen = 0;
static std::vector<std::string> pointLog;

static double realMm(const Axis &a) { return a.c / a.spmTrue; }
static void send(const std::string &s) { pendingIn.push_back({simUs + 1500000ULL, s}); }   // user answers 1.5 s later
static void sendNow(const std::string &s) { for (char ch : s) serialIn.push_back(ch); }

static void reportPoint(const std::string &label) {
  // commanded mm (what the sketch thinks) vs where the carriage really is, relative to the real home
  double cmdX = xPos / X_STEPS_PER_MM, cmdY = yPos / Y_STEPS_PER_MM;
  double rx = realMm(X) - homeX, ry = realMm(Y) - homeY;
  double ex = rx - cmdX, ey = ry - cmdY;
  worstX = (worstX > std::fabs(ex) ? worstX : std::fabs(ex)); worstY = (worstY > std::fabs(ey) ? worstY : std::fabs(ey));
  pointsSeen++;
  char b[200];
  snprintf(b, sizeof b, "%-12s commanded X %6.2f Y %6.2f | real X %6.2f Y %6.2f | error X %+5.2f Y %+5.2f mm",
           label.c_str(), cmdX, cmdY, rx, ry, ex, ey);
  pointLog.push_back(b);
  if (!quiet) printf("      [sim] %s\n", b);
}

void onOutputLine(const std::string &s) {
  if (s.find(">> Home (X 0, Y 0)") != std::string::npos || s.find("Homed. ") != std::string::npos) {
    homeX = realMm(X); homeY = realMm(Y);
  }
  int ax = s.find("(X)") != std::string::npos ? 0 : s.find("(Y)") != std::string::npos ? 1 : -1;
  const Axis &A = ax == 1 ? Y : X;
  if (s.find("--- At point") != std::string::npos) { reportPoint(s.substr(4, s.find(":") - 4)); return; }
  if (s.find("Point ") == 0 && s.find("press Enter") != std::string::npos) { reportPoint(s.substr(0, 11)); send("\n"); return; }
  if (s.find("Point ") == 0 && s.find("(commanded)") != std::string::npos) { reportPoint(s.substr(0, 11)); return; }
  if (s.find("ZERO") != std::string::npos && ax >= 0) { zeroRef[ax] = realMm(A); send("\n"); return; }
  if (s.find("MEASURE") != std::string::npos && ax >= 0) {
    char b[32];
    // the "user" reads calipers: distance from the zero for a "how far" question, else an absolute reading
    double v = s.find("how far") != std::string::npos ? realMm(A) - zeroRef[ax] : realMm(A) + 37.0;
    snprintf(b, sizeof b, "%.2f\n", v);
    if (!quiet) printf("      [sim user types %s", b);
    send(b);
    return;
  }
  if (s.find("press Enter") != std::string::npos) send("\n");
}

int main(int argc, char **argv) {
  // options: bx=<backlash mm X> by=<..Y> sx=<real steps/mm X> sy=<..Y> lx=<lose every N X steps when faster than fx us>
  //          fx=<us> g=<glitch prob> q (quiet) x0=<start mm from switch> y0=<..>
  double x0 = 60, y0 = 30;
  int i = 1;
  for (; i < argc && strcmp(argv[i], "--"); i++) {
    std::string a = argv[i];
    auto val = [&](const char *k) { return atof(a.c_str() + strlen(k)); };
    if (!a.rfind("bx=", 0)) X.b = val("bx=");          // stored in mm for now
    else if (!a.rfind("by=", 0)) Y.b = val("by=");
    else if (!a.rfind("sx=", 0)) X.spmTrue = val("sx=");
    else if (!a.rfind("sy=", 0)) Y.spmTrue = val("sy=");
    else if (!a.rfind("lx=", 0)) X.loseEvery = (long)val("lx=");
    else if (!a.rfind("fx=", 0)) { X.loseOnlyFast = true; X.fastUs = (unsigned)val("fx="); }
    else if (!a.rfind("g=", 0)) glitchProb = val("g=");
    else if (!a.rfind("x0=", 0)) x0 = val("x0=");
    else if (!a.rfind("y0=", 0)) y0 = val("y0=");
    else if (a == "q") quiet = true;
  }
  X.b *= X.spmTrue; Y.b *= Y.spmTrue;
  X.c = X.m = x0 * X.spmTrue; Y.c = Y.m = y0 * Y.spmTrue;
  X.update(); Y.update();
  std::vector<std::string> cmds;
  for (i++; i < argc; i++) cmds.push_back(argv[i]);

  setup();
  size_t next = 0;
  uint64_t idleSince = simUs;
  while (true) {
    if (serialIn.empty() && pendingIn.empty() && simUs - idleSince > 1000000) {   // sketch idle for 1 s -> next command
      if (next >= cmds.size()) break;
      if (!quiet) printf("\n### sending: %s\n", cmds[next].c_str());
      sendNow(cmds[next++] + "\n");
      idleSince = simUs;
    }
    loop();
    simUs += 1000;
    if (!serialIn.empty()) idleSince = simUs;
  }
  printf("\n[sim] SUMMARY: %d stops checked, worst real-vs-commanded error X %.2f mm, Y %.2f mm, switch glitches injected %ld, sim time %.0f s\n",
         pointsSeen, worstX, worstY, glitchesInjected, simUs / 1e6);
  if (quiet) for (auto &l : pointLog) printf("  %s\n", l.c_str());
  return 0;
}
