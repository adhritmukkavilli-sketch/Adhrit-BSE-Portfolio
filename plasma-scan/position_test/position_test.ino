// ====================================================================
//  POSITION TEST for the plasma scan stage    (Arduino Mega, Serial Monitor 9600 baud, "New Line")
//
//  Same wiring, pins, directions, steps/mm, speed, homing and scan path as plasma_scan_v3.
//  It takes NO temperature readings. It only checks if the stage really stops where it is told.
//
//  The Arduino cannot see where the stage is. It only counts the step pulses it sends.
//  So "X 10.15 mm" in the Serial Monitor is where it TOLD the motor to go, not where the stage is.
//  There are 3 ways the real position can be different, and each test finds one of them:
//
//    T1  STEPS PER MM   Does 1 inch of steps really move 1 inch?                  (calipers or ruler)
//        Wrong if the driver's microstep DIP switches (or the pulley / lead screw) don't match
//        X_STEPS_PER_MM / Y_STEPS_PER_MM. Then the scan does NOT cover exactly 1 x 1 inch.
//
//    T2  LOST STEPS     Runs the scan path a few times (no readings), then touches both switches.
//        If a motor skips (too fast / too little current / supply sagging) or a pulley or
//        coupling set screw slips, the stage is no longer where the step count says.  (no tools)
//
//    T3  BACKLASH       Goes to the same spot from both sides, you measure both.  (calipers)
//        Play in the belt / lead-screw nut / coupling: right after a motor reverses, the first
//        steps don't move the stage. The v3 scan never moves backward into a stop (between rows,
//        X goes LEAD_IN_MM past the row start and comes forward), so the play does not matter as
//        long as it is smaller than LEAD_IN_MM. T3 tells you if it is.
//        (The switch check in T2 and at the end of the scan can NOT see backlash.)
//
//    T4  POINT TOUR     Visits the 25 scan stops in the same order and the same way as the scan,
//        stopping at each one, so you can mark where the sensor tip really is.
//
//  Settings you can change while it runs (they start like the v3 scan code):
//    A <n>    ramp: start slow and speed up over n steps (40 = like the scan, 0 = off)
//    V <us>   speed: half step time in microseconds (1500 = like the scan, bigger = slower)
//
//  Commands (type, then Enter):
//    H          home both axes                   X <mm>  /  Y <mm>   go to a position (mm from home)
//    W          where am I + switch states       S                   stop now, motors off
//    T1 [mm]    steps/mm test (default 25.4)     T2 [runs]           lost steps test (default 3)
//    T3         backlash test                    T4 [seconds]        point tour (no number = wait for Enter)
//    A, V       settings (see above)             ?                   show this list
//
//  Wiring (same as the scan code):
//    X driver: PUL+ 10, DIR+ 9, ENA+ 8      Y driver: PUL+ 7, DIR+ 4, ENA+ 5
//    PUL-, DIR-, ENA- of BOTH drivers -> Mega GND
//    X limit switch -> pin 2, Y limit switch -> pin 3 (C -> GND, NC -> pin)
// ====================================================================

// ---- pins ----
const int xStepPin = 10, xDirPin = 9, xEnaPin = 8;
const int yStepPin = 7,  yDirPin = 4, yEnaPin = 5;
const int xLimitPin = 2, yLimitPin = 3;

// ---- directions ----
const int X_LEFT = HIGH, X_RIGHT = LOW;    // right = toward the X switch (home)
const int Y_DOWN = HIGH, Y_UP    = LOW;    // up    = toward the Y switch (home)

// ---- motors (must match the scan code) ----
const float X_STEPS_PER_MM = 20.0;
const float Y_STEPS_PER_MM = 160.0;
const unsigned int HOME_US    = 1500;      // homing speed (same as the scan code, whatever V is set to)
const unsigned int X_TOUCH_US = 1500;      // careful touch of the X switch
const unsigned int Y_TOUCH_US = 3000;      // slow, careful touch of the Y switch
const float BACKOFF_MM = 2.0;              // home = 2 mm off each switch
const unsigned int RAMP_START_US = 3000;   // with the ramp on, every move starts and ends at this speed

// ---- scan path (must match the scan code) ----
const int   POINTS = 5;                    // 5 x 5 stops
const float SIDE_MM = 25.4;                // first stop at 0, last at 25.4 mm (1 inch)
const float GAP_MM = SIDE_MM / (POINTS - 1);   // 5.08 mm
const float LEAD_IN_MM = 1.0;              // between rows X goes this far past the row start, then forward
const float LEAD_IN_SAFE_MM = (LEAD_IN_MM < BACKOFF_MM - 0.5) ? LEAD_IN_MM : BACKOFF_MM - 0.5;
const float MAX_MM = 30.0;                 // X/Y commands and T1 never go further than this from home

// ---- settings you can change with A and V (they start like the scan code) ----
int   rampSteps = 40;                      // A: 0 = off
unsigned int stepUs = 1500;                // V: half step time in us

long xPos = 0, yPos = 0;                   // position in steps from home (what the Arduino THINKS)
bool homed = false, stopNow = false;
unsigned long switchGlitches = 0;          // switch read HIGH once but not on the re-check = electrical noise

// ====================================================================
//  basics
// ====================================================================
float spm(bool isX) { return isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM; }

// NC switch with pull-up: HIGH = pressed (or a broken wire). It must read HIGH 5 times in a row,
// so a spike of noise from the motor wires is not taken as a click (and gets counted instead).
bool pressed(bool isX) {
  int pin = isX ? xLimitPin : yLimitPin;
  if (digitalRead(pin) == LOW) return false;
  for (byte k = 0; k < 4; k++) {
    delayMicroseconds(20);
    if (digitalRead(pin) == LOW) { switchGlitches++; return false; }
  }
  return true;
}

void motorsOn()  { digitalWrite(xEnaPin, LOW);  digitalWrite(yEnaPin, LOW);  delay(50); }
void motorsOff() { digitalWrite(xEnaPin, HIGH); digitalWrite(yEnaPin, HIGH); homed = false; }

void checkStop() {
  while (Serial.available()) { char c = Serial.read(); if (c == 'S' || c == 's') stopNow = true; }
}

// wait ms milliseconds, motors locked. false = S was typed
bool pause(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) { checkStop(); if (stopNow) return false; delay(2); }
  return true;
}

// ---- reading lines from the Serial Monitor ----
char line[24];
byte lineLen = 0;
unsigned long lastCharMs = 0;

// true when a whole line has arrived (Enter, or nothing more typed for 0.3 s). The line is in `line`.
bool lineReady() {
  while (Serial.available()) {
    char c = Serial.read();
    lastCharMs = millis();
    if (c == '\n' || c == '\r') { line[lineLen] = 0; lineLen = 0; return true; }
    if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
  }
  if (lineLen > 0 && millis() - lastCharMs > 300) { line[lineLen] = 0; lineLen = 0; return true; }
  return false;
}

void dropInput() { delay(50); while (Serial.available()) Serial.read(); lineLen = 0; }

// Waits for you to type something + Enter. Returns the number typed, or NAN if no number (just Enter).
// Typing S stops the test.
float askNumber() {
  dropInput();
  while (!lineReady()) delay(2);
  if (line[0] == 'S' || line[0] == 's') { stopNow = true; return NAN; }
  if (!strpbrk(line, "0123456789")) return NAN;
  return atof(line);
}

// ====================================================================
//  movement (same as the scan code)
//  steps > 0 = forward, away from home (X left / Y down), steps < 0 = backward, toward home.
//  untilSwitch = true: stop when the switch clicks. Returns steps made, -1 = stopped / problem.
// ====================================================================
long move(bool isX, long steps, unsigned int us, bool untilSwitch) {
  if (steps == 0) return 0;
  int stepPin = isX ? xStepPin : yStepPin;
  bool away = steps > 0;
  long n = labs(steps);
  digitalWrite(isX ? xDirPin : yDirPin, isX ? (away ? X_LEFT : X_RIGHT) : (away ? Y_DOWN : Y_UP));
  delay(5);
  // Every pulse is timed from micros(), so switch checks and maths never stretch a step: the speed
  // is perfectly steady (no stutter), and every move of the same length is timed exactly the same.
  float v0 = 500000.0 / RAMP_START_US, v1 = 500000.0 / us;   // start speed and full speed, steps per second
  unsigned long t = micros();
  for (long i = 0; i < n; i++) {
    if (!away && pressed(isX)) {
      if (untilSwitch) return i;
      Serial.println(F("!! A limit switch clicked in the middle of a move. Stopped."));
      stopNow = true;
      return -1;
    }
    if (i % 100 == 0) { checkStop(); if (stopNow) return -1; }
    unsigned long half = us;                              // half step time for THIS step
    if (rampSteps > 0 && RAMP_START_US > us) {                 // smooth ramp: constant acceleration
      long edge = min(i, n - 1 - i);                      // steps from the nearer end -> start and stop are mirror images
      if (edge < rampSteps) half = 500000.0 / sqrt(v0 * v0 + (v1 * v1 - v0 * v0) * edge / rampSteps);
    }
    while ((long)(micros() - t) < 0) { }                 // wait for the exact start of this step
    digitalWrite(stepPin, HIGH); t += half;
    while ((long)(micros() - t) < 0) { }
    digitalWrite(stepPin, LOW);  t += half;
    if (isX) xPos += away ? 1 : -1; else yPos += away ? 1 : -1;
  }
  while ((long)(micros() - t) < 0) { }                   // finish the last step
  return n;
}

// move one axis straight to `target` (steps from home)
bool moveStraight(bool isX, long target) {
  return move(isX, target - (isX ? xPos : yPos), stepUs, false) >= 0;
}

// Arrive at `target` moving FORWARD (away from home), the way the scan reaches every stop:
// if the target is behind, go LEAD_IN past it first, then come forward.
bool approach(bool isX, long target) {
  long pos = isX ? xPos : yPos;
  if (target < pos) {
    long lead = lround(LEAD_IN_SAFE_MM * spm(isX));
    long before = target - lead;
    if (before < -lead) before = -lead;                   // never closer to the switch than the scan goes
    if (before < pos && !moveStraight(isX, before)) return false;
  }
  return moveStraight(isX, target);
}

// go to (x, y) mm from home, arriving forward on both axes: Y first, then X
bool goTo(float xmm, float ymm) {
  return approach(false, lround(ymm * Y_STEPS_PER_MM)) && approach(true, lround(xmm * X_STEPS_PER_MM));
}

// Move to scan stop p (0 .. POINTS*POINTS-1) exactly the way the scan code does
bool goToStop(int p) {
  int r = p / POINTS, c = p % POINTS;
  if (p == 0) return goTo(0, 0);                         // the scan starts here, right after homing
  if (c > 0) return moveStraight(true, lround(c * GAP_MM * X_STEPS_PER_MM));      // along the row: forward
  // new row: X back past the row start (no readings), Y down one row, X forward into the first stop
  return moveStraight(true, -lround(LEAD_IN_SAFE_MM * X_STEPS_PER_MM))
      && moveStraight(false, lround(r * GAP_MM * Y_STEPS_PER_MM))
      && moveStraight(true, 0);
}

// ====================================================================
//  homing (same as the scan code)
// ====================================================================
bool homeOne(bool isX) {
  float s = spm(isX);
  if (pressed(isX) && move(isX, lround(3 * s), HOME_US, true) < 0) return false;
  if (pressed(isX)) { Serial.println(isX ? F("!! X could not move off its switch.") : F("!! Y could not move off its switch.")); return false; }
  if (move(isX, -lround(340 * s), HOME_US, true) < 0) return false;
  if (!pressed(isX)) { Serial.println(isX ? F("!! X never reached its switch (pin 2).") : F("!! Y never reached its switch (pin 3).")); return false; }
  if (move(isX, lround(3 * s), HOME_US, true) < 0) return false;
  if (move(isX, -lround(5 * s), isX ? X_TOUCH_US : Y_TOUCH_US, true) < 0) return false;
  if (!pressed(isX)) { Serial.println(isX ? F("!! X slow touch missed the switch.") : F("!! Y slow touch missed the switch.")); return false; }
  if (move(isX, lround(BACKOFF_MM * s), HOME_US, true) < 0) return false;
  if (isX) xPos = 0; else yPos = 0;
  return true;
}

bool homeAll() {
  stopNow = false;
  motorsOn();
  Serial.println(F(">> Homing Y..."));
  if (!homeOne(false)) { motorsOff(); Serial.println(F(">> Homing failed.")); return false; }
  Serial.println(F(">> Homing X..."));
  if (!homeOne(true))  { motorsOff(); Serial.println(F(">> Homing failed.")); return false; }
  homed = true;
  Serial.println(F(">> Home (X 0, Y 0)."));
  return true;
}

bool ensureHomed() {
  stopNow = false;
  if (homed) return true;
  Serial.println(F(">> Not homed yet - homing first."));
  return homeAll();
}

// Go home, then creep toward each switch, count the steps until it clicks and compare with the
// 2 mm it should be. Shows LOST STEPS. It cannot show backlash (see T3).
// Ends at home again (re-homed). Returns false if stopped or a switch was not found.
bool switchCheck() {
  if (!goTo(0, 0)) return false;
  Serial.println(F(">> Switch check (touching both switches slowly):"));
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 1);
    float s = spm(isX);
    long expected = lround(BACKOFF_MM * s);
    long made = move(isX, -lround((BACKOFF_MM + 4) * s), isX ? X_TOUCH_US : Y_TOUCH_US, true);
    if (made < 0) return false;
    Serial.print(isX ? F("   X: ") : F("   Y: "));
    if (!pressed(isX)) {
      Serial.println(F("switch NOT reached within 6 mm -> lost a lot of steps (or a switch wiring problem). Type H."));
      homed = false;
      return false;
    }
    long err = made - expected;
    Serial.print(F("off by ")); Serial.print(err / s, 2); Serial.print(F(" mm (")); Serial.print(err); Serial.print(F(" steps)  "));
    if (fabs(err / s) <= 0.10 || labs(err) <= 2) Serial.println(F("OK - no lost steps"));
    else if (err > 0) Serial.println(F("LOST STEPS - the stage was further from home than the step count"));
    else              Serial.println(F("LOST STEPS - the stage was closer to home than the step count"));
    if (move(isX, expected, HOME_US, true) < 0) return false;
    if (isX) xPos = 0; else yPos = 0;
  }
  return true;
}

void printSettings() {
  Serial.print(F("   settings: V ")); Serial.print(stepUs); Serial.print(F(" us, A ")); Serial.print(rampSteps);
  Serial.print(F(" steps (lead-in ")); Serial.print(LEAD_IN_SAFE_MM, 2); Serial.println(F(" mm)"));
}

void stopped() { motorsOff(); Serial.println(F(">> STOPPED. Motors off. Type H to home again.")); }

// ====================================================================
//  T1: steps per mm. Moves out `dist` mm, you measure how far it really went.
// ====================================================================
void testStepsPerMm(float dist) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T1 STEPS PER MM ==="));
  Serial.println(F("Set your calipers between a fixed part of the frame and the moving carriage"));
  Serial.println(F("(or tape a ruler on and mark the carriage). Both readings are taken after moving forward,"));
  Serial.println(F("so backlash does not change the result."));
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 0);
    float s = spm(isX);
    if (!goTo(0, 0) || !approach(isX, lround(1.0 * s))) { stopped(); return; }   // start 1 mm out
    Serial.println(isX ? F(">> (X) Put your calipers on X now: this spot is ZERO. Then press Enter.")
                       : F(">> (Y) Put your calipers on Y now: this spot is ZERO. Then press Enter."));
    askNumber(); if (stopNow) { stopped(); return; }
    long steps = lround(dist * s);
    if (move(isX, steps, stepUs, false) < 0) { stopped(); return; }
    Serial.print(isX ? F(">> (X) X moved ") : F(">> (Y) Y moved ")); Serial.print(dist, 2); Serial.print(F(" mm (")); Serial.print(steps);
    Serial.println(F(" steps). MEASURE how far it really moved, type it in mm and press Enter (or just Enter to skip):"));
    float measured = askNumber(); if (stopNow) { stopped(); return; }
    if (!isnan(measured) && measured > 0.5) {
      float real = s * dist / measured;
      Serial.print(F("   You measured ")); Serial.print(measured, 2); Serial.print(F(" mm -> error "));
      Serial.print(measured - dist, 2); Serial.print(F(" mm over ")); Serial.print(dist, 2); Serial.println(F(" mm"));
      Serial.print(F("   Real steps per mm = ")); Serial.print(real, 2); Serial.print(F("  (code has ")); Serial.print(s, 2); Serial.println(F(")"));
      if (fabs(real - s) / s < 0.01) Serial.println(F("   OK (less than 1% off)."));
      else {
        Serial.print(F("   -> change ")); Serial.print(isX ? F("X_STEPS_PER_MM") : F("Y_STEPS_PER_MM")); Serial.print(F(" to ")); Serial.print(real, 2);
        Serial.println(F(" in BOTH sketches (and check the driver's microstep DIP switches)."));
        if (fabs(real / s - 2) < 0.1 || fabs(real / s - 0.5) < 0.05 || fabs(real / s - 4) < 0.2 || fabs(real / s - 0.25) < 0.02)
          Serial.println(F("   It is about 2x or 4x off: that is almost always the microstep DIP switches on the driver."));
      }
    }
  }
  if (!goTo(0, 0)) { stopped(); return; }
  Serial.println(F("=== T1 done. Back home. ==="));
}

// ====================================================================
//  T2: lost steps. Runs the scan path `runs` times (0.2 s per stop), then the switch check.
// ====================================================================
void testLostSteps(int runs) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T2 LOST STEPS ==="));
  printSettings();
  unsigned long glitches0 = switchGlitches;
  for (int run = 1; run <= runs; run++) {
    Serial.print(F(">> Run ")); Serial.print(run); Serial.print('/'); Serial.print(runs); Serial.println(F(" of the scan path..."));
    for (int p = 0; p < POINTS * POINTS; p++)
      if (!goToStop(p) || !pause(200)) { stopped(); return; }
  }
  if (!switchCheck()) { stopped(); return; }
  Serial.print(F("   switch noise spikes ignored: ")); Serial.println(switchGlitches - glitches0);
  if (switchGlitches > glitches0)
    Serial.println(F("   The switch wires pick up noise. The filter handles it, but keep them away from the motor wires."));
  Serial.println(F("What it means: OK on both = the motors don't skip at this speed. If you still see wrong"));
  Serial.println(F("points, it is steps/mm (T1) or backlash bigger than the lead-in (T3). LOST STEPS = slower"));
  Serial.println(F("speed (V 3000), more driver current, tighten the pulley/coupling set screws, and check that"));
  Serial.println(F("the supply isn't current-limiting."));
  Serial.println(F("=== T2 done. Home. ==="));
}

// ====================================================================
//  T3: backlash. Same spot reached from both sides, you measure both.
// ====================================================================
void testBacklash() {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T3 BACKLASH ==="));
  printSettings();
  Serial.println(F("Set your calipers between a fixed part of the frame and the carriage of the axis being tested."));
  Serial.println(F("Type each reading (any zero you like, only the difference counts)."));
  const float MID = 12.7;                                    // middle of the scan area
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 0);
    float s = spm(isX);
    long mid = lround(MID * s);
    if (!goTo(0, 0) || !approach(isX, mid)) { stopped(); return; }                // A: forward, like every scan stop
    Serial.println(isX ? F(">> (X) Reading A: X arrived moving FORWARD (left), like the scan. MEASURE the X position, type it, Enter:")
                       : F(">> (Y) Reading A: Y arrived moving FORWARD (down), like the scan. MEASURE the Y position, type it, Enter:"));
    float a = askNumber(); if (stopNow) { stopped(); return; }
    if (!moveStraight(isX, mid + lround(5 * s)) || !moveStraight(isX, mid)) { stopped(); return; }   // B: backward, on purpose
    Serial.println(isX ? F(">> (X) Reading B: X came back to the same spot moving BACKWARD. MEASURE again, type it, Enter:")
                       : F(">> (Y) Reading B: Y came back to the same spot moving BACKWARD. MEASURE again, type it, Enter:"));
    float b = askNumber(); if (stopNow) { stopped(); return; }
    if (!isnan(a) && !isnan(b)) {
      float play = fabs(a - b);
      Serial.print(isX ? F("   X backlash = ") : F("   Y backlash = ")); Serial.print(play, 2); Serial.print(F(" mm  "));
      if (!isX) Serial.println(F("(Y only moves down during the scan, so this does not shift any stop)"));
      else if (play < 0.06) Serial.println(F("(none you can measure)"));
      else if (play < LEAD_IN_SAFE_MM - 0.25) Serial.println(F("-> OK: smaller than the lead-in, so the scan is not affected"));
      else Serial.println(F("-> TOO BIG for the lead-in: tighten the belt and the pulley/coupling set screws"));
    }
  }
  if (!goTo(0, 0)) { stopped(); return; }
  Serial.println(F("=== T3 done. Back home. ==="));
}

// ====================================================================
//  T4: point tour. Visits the scan stops exactly like the scan does.
// ====================================================================
void testTour(bool waitEnter, float seconds) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T4 POINT TOUR (same path as the scan) ==="));
  printSettings();
  Serial.println(F("Fix a piece of graph paper right in front of the sensor tip and mark where the tip is at each stop."));
  Serial.println(F("Type S to stop."));
  for (int p = 0; p < POINTS * POINTS; p++) {
    if (!goToStop(p)) { stopped(); return; }
    Serial.print(F("Point ")); Serial.print(p + 1); Serial.print('/'); Serial.print(POINTS * POINTS); Serial.print(F(" (row ")); Serial.print(p / POINTS + 1);
    Serial.print(F("): X ")); Serial.print(xPos / X_STEPS_PER_MM, 2); Serial.print(F(" mm, Y ")); Serial.print(yPos / Y_STEPS_PER_MM, 2);
    if (waitEnter) {
      Serial.println(F(" mm (commanded) - mark it, then press Enter"));
      askNumber();
    } else {
      Serial.println(F(" mm (commanded)"));
      pause((unsigned long)(seconds * 1000));
    }
    if (stopNow) { stopped(); return; }
  }
  if (!switchCheck()) { stopped(); return; }
  Serial.print(F("What to look for on the paper: ")); Serial.print(POINTS); Serial.print(F(" x ")); Serial.print(POINTS);
  Serial.print(F(" dots in a square grid, ")); Serial.print(GAP_MM, 2); Serial.print(F(" mm apart, "));
  Serial.print(SIDE_MM, 2); Serial.println(F(" mm edge to edge."));
  Serial.println(F("  grid too big or too small                         -> steps/mm (T1)"));
  Serial.println(F("  rows shifted sideways from each other             -> backlash bigger than the lead-in (T3)"));
  Serial.println(F("  points drift more and more / switch check fails   -> lost steps (T2)"));
  Serial.println(F("=== T4 done. Home. ==="));
}

// ====================================================================
void where() {
  Serial.print(F(">> Commanded position: X ")); Serial.print(xPos / X_STEPS_PER_MM, 2); Serial.print(F(" mm (")); Serial.print(xPos);
  Serial.print(F(" steps), Y ")); Serial.print(yPos / Y_STEPS_PER_MM, 2); Serial.print(F(" mm (")); Serial.print(yPos); Serial.println(F(" steps)"));
  Serial.print(F("   homed: ")); Serial.print(homed ? F("yes") : F("NO"));
  Serial.print(F(" | X switch (pin 2): ")); Serial.print(digitalRead(xLimitPin) == HIGH ? F("PRESSED") : F("not pressed"));
  Serial.print(F(" | Y switch (pin 3): ")); Serial.println(digitalRead(yLimitPin) == HIGH ? F("PRESSED") : F("not pressed"));
  Serial.print(F("   switch noise spikes so far: ")); Serial.println(switchGlitches);
  printSettings();
}

void help() {
  Serial.println(F("  H = home            X <mm> / Y <mm> = go there (0 to 30 mm from home, arrives moving forward)"));
  Serial.println(F("  W = where + switches                 S = stop, motors off"));
  Serial.println(F("  T1 [mm] = steps/mm test (needs calipers)     T2 [runs] = lost steps test (no tools)"));
  Serial.println(F("  T3 = backlash test (needs calipers)          T4 [s] = point tour (no number = Enter at each)"));
  Serial.println(F("  A <steps> = ramp (40 = scan, 0 = off)        V <us> = speed (1500 = scan, bigger = slower)"));
}

void runCommand() {
  char cmd = toupper(line[0]);
  const char *p = line + 1;
  int test = 0;
  if (cmd == 'T') { test = atoi(p); while (*p == ' ') p++; while (isdigit(*p)) p++; }
  bool hasNum = strpbrk(p, "0123456789") != NULL;
  float num = atof(p);
  stopNow = false;

  switch (cmd) {
    case 'H': homeAll(); break;
    case 'W': where(); break;
    case '?': help(); break;
    case 'S': motorsOff(); Serial.println(F(">> Motors off. Type H to home again.")); break;
    case 'X': case 'Y':
      if (!hasNum || num < 0 || num > MAX_MM) { Serial.println(F("!! Give a distance from 0 to 30 mm, e.g. X 12.7")); break; }
      if (!ensureHomed()) break;
      if (!approach(cmd == 'X', lround(num * spm(cmd == 'X')))) { stopped(); break; }
      where();
      break;
    case 'A':
      if (hasNum) rampSteps = constrain((int)num, 0, 400);
      Serial.println(rampSteps > 0 ? F(">> Ramp ON: moves start and end slowly (like the scan).") : F(">> Ramp OFF."));
      printSettings();
      break;
    case 'V':
      if (hasNum) stepUs = (unsigned int)constrain(num, 300.0, 5000.0);
      Serial.println(F(">> Speed set (1500 = the scan code's speed, smaller = faster)."));
      printSettings();
      break;
    case 'T':
      if (test == 1) testStepsPerMm(hasNum ? constrain(num, 1.0, MAX_MM - 1) : 25.4);
      else if (test == 2) testLostSteps(hasNum ? constrain((int)num, 1, 20) : 3);
      else if (test == 3) testBacklash();
      else if (test == 4) testTour(!hasNum, hasNum ? constrain(num, 0.5, 60.0) : 0);
      else Serial.println(F("!! Tests are T1, T2, T3, T4."));
      break;
    default:
      Serial.println(F("!! Unknown command. Type ? for the list."));
  }
}

// ====================================================================
void setup() {
  pinMode(xStepPin, OUTPUT); pinMode(xDirPin, OUTPUT); pinMode(xEnaPin, OUTPUT);
  pinMode(yStepPin, OUTPUT); pinMode(yDirPin, OUTPUT); pinMode(yEnaPin, OUTPUT);
  pinMode(xLimitPin, INPUT_PULLUP); pinMode(yLimitPin, INPUT_PULLUP);
  digitalWrite(xStepPin, LOW); digitalWrite(yStepPin, LOW);
  motorsOff();
  Serial.begin(9600);
  delay(300);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("  POSITION TEST - plasma scan stage (v3 path) - READY"));
  Serial.println(F("  Start with T2 (no tools), then T4, T3, T1."));
  Serial.println(F("========================================"));
  help();
  printSettings();
}

void loop() {
  if (lineReady() && line[0] != 0) runCommand();
}
