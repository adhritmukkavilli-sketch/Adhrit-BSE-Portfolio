// ====================================================================
//  POSITION TEST for the plasma scan stage    (Arduino Mega, Serial Monitor 9600 baud, "New Line")
//
//  Same wiring, pins, directions, steps/mm, speed and homing as the scan code.
//  It takes NO temperature readings. It only checks if the stage really stops where it is told.
//
//  The Arduino cannot see where the stage is. It only counts the step pulses it sends.
//  So "X 10.15 mm" in the Serial Monitor is where it TOLD the motor to go, not where the stage is.
//  There are 3 ways the real position can be different, and each test finds one of them:
//
//    T1  STEPS PER MM   Does 1 inch of steps really move 1 inch?                  (calipers or ruler)
//        Wrong if the driver's microstep DIP switches (or the pulley / lead screw) don't match
//        X_STEPS_PER_MM / Y_STEPS_PER_MM. The error grows the further it goes.
//
//    T2  LOST STEPS     Runs the scan path a few times (no waiting), then touches both switches.
//        If a motor skips (too fast / too little current / supply sagging) or a pulley or
//        coupling set screw slips, the stage is no longer where the step count says.  (no tools)
//
//    T3  BACKLASH       Goes to the same spot from both sides, you measure both.  (calipers)
//        Play in the belt / lead-screw nut / coupling: right after a motor reverses, the first
//        few steps don't move the stage. The scan zig-zags, so X reverses every row, and rows
//        2 and 4 end up shifted sideways from rows 1, 3 and 5.
//        IMPORTANT: the switch check (T2, and the scan's accuracy check) CANNOT see backlash.
//        It can say "OK" while the points are off.
//
//    T4  POINT TOUR     Visits the 25 scan points in the same order as the scan and stops at each,
//        so you can mark where the sensor tip really is (graph paper fixed in front of the tip).
//
//  Settings you can change while it runs, so you can try the fixes one at a time:
//    K <mm>   backlash fix: every stop is reached moving AWAY from home (goes K mm past, comes back)
//    A <n>    acceleration: start slow and speed up over n steps (0 = off, like the scan code)
//    V <us>   speed: half step time in microseconds (1500 = scan code speed, bigger = slower)
//  They start like the scan code (K 0, A 0, V 1500), so first you test the code as it is now.
//
//  Commands (type, then Enter):
//    H          home both axes                   X <mm>  /  Y <mm>   go to a position (mm from home)
//    W          where am I + switch states       S                   stop now, motors off
//    T1 [mm]    steps/mm test (default 25.4)     T2 [runs]           lost steps test (default 3)
//    T3         backlash test                    T4 [seconds]        point tour (no number = wait for Enter)
//    K, A, V    settings (see above)             ?                   show this list
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

// ---- scan path (must match the scan code) ----
const int   ROWS = 5, COLS = 5;
const float X_GAP_MM = 5.08, Y_GAP_MM = 5.08;
const float MAX_MM = 30.0;                 // X/Y commands and T1 never go further than this from home

// ---- settings you can change with K, A, V (they start like the scan code) ----
float backlashMm = 0.0;                    // K: 0 = off
int   rampSteps  = 0;                      // A: 0 = off
unsigned int stepUs = 1500;                // V: half step time in us
const unsigned int RAMP_START_US = 3000;   // with A on, every move starts and ends at this speed

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
//  movement (same as the scan code, plus the optional ramp)
//  steps > 0 = away from home (X left / Y down), steps < 0 = toward home.
//  untilSwitch = true: stop when the switch clicks. Returns steps made, -1 = stopped / problem.
// ====================================================================
long move(bool isX, long steps, unsigned int us, bool untilSwitch) {
  if (steps == 0) return 0;
  int stepPin = isX ? xStepPin : yStepPin;
  bool away = steps > 0;
  long n = labs(steps);
  digitalWrite(isX ? xDirPin : yDirPin, isX ? (away ? X_LEFT : X_RIGHT) : (away ? Y_DOWN : Y_UP));
  delay(5);
  for (long i = 0; i < n; i++) {
    if (!away && pressed(isX)) {
      if (untilSwitch) return i;
      Serial.println(F("!! A limit switch clicked in the middle of a move. Stopped."));
      stopNow = true;
      return -1;
    }
    if (i % 100 == 0) { checkStop(); if (stopNow) return -1; }
    unsigned int d = us;
    if (rampSteps > 0 && RAMP_START_US > us) {             // ramp: slow at both ends of the move
      long edge = min(i, n - 1 - i);
      if (edge < rampSteps) d = RAMP_START_US - (unsigned long)(RAMP_START_US - us) * edge / rampSteps;
    }
    digitalWrite(stepPin, HIGH); delayMicroseconds(d);
    digitalWrite(stepPin, LOW);  delayMicroseconds(d);
    if (isX) xPos += away ? 1 : -1; else yPos += away ? 1 : -1;
  }
  return n;
}

// Move one axis to `target` (steps from home).
// Backlash fix (K > 0): a move TOWARD home goes K mm past the target and then comes back, so every
// stop is reached moving AWAY from home - the same way home itself was reached. Then the play in
// the belt / nut is always on the same side and does not shift the point.
bool moveTo(bool isX, long target) {
  long pos = isX ? xPos : yPos;
  long comp = lround(backlashMm * spm(isX));
  if (target < pos && comp > 0) {
    long past = target - comp;
    long lowest = -lround(BACKOFF_MM / 2 * spm(isX));     // never closer than 1 mm to the switch
    if (past < lowest) past = lowest;
    if (past < pos) {
      if (move(isX, past - pos, stepUs, false) < 0) return false;
      pos = past;
    }
  }
  return move(isX, target - pos, stepUs, false) >= 0;
}

// go to (x, y) in mm from home: Y first, then X (same as the scan code)
bool goTo(float xmm, float ymm) {
  return moveTo(false, lround(ymm * Y_STEPS_PER_MM)) && moveTo(true, lround(xmm * X_STEPS_PER_MM));
}

// ====================================================================
//  homing (same as the scan code, plus a check that the slow touch really reached the switch)
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

// x, y (mm) of scan point p (0..24), same order as the scan code (zig-zag)
void scanPoint(int p, float &x, float &y) {
  int r = p / COLS, j = p % COLS;
  int c = (r % 2 == 0) ? j : COLS - 1 - j;
  x = (c + 1) * X_GAP_MM;
  y = (r + 1) * Y_GAP_MM;
}

void printSettings() {
  Serial.print(F("   settings: V ")); Serial.print(stepUs); Serial.print(F(" us, A ")); Serial.print(rampSteps);
  Serial.print(F(" steps, K ")); Serial.print(backlashMm, 2); Serial.println(F(" mm"));
}

void stopped() { motorsOff(); Serial.println(F(">> STOPPED. Motors off. Type H to home again.")); }

// ====================================================================
//  T1: steps per mm. Moves out `dist` mm, you measure how far it really went.
// ====================================================================
void testStepsPerMm(float dist) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T1 STEPS PER MM ==="));
  Serial.println(F("Set your calipers between a fixed part of the frame and the moving carriage"));
  Serial.println(F("(or tape a ruler on and mark the carriage). Both readings are taken after moving AWAY"));
  Serial.println(F("from home, so backlash does not change the result."));
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 0);
    float s = spm(isX);
    if (!goTo(0, 0)) { stopped(); return; }
    if (!moveTo(isX, lround(1.0 * s))) { stopped(); return; }  // start point 1 mm out, reached moving away
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
//  T2: lost steps. Runs the 25-point scan path `runs` times (0.2 s per stop), then the switch check.
// ====================================================================
void testLostSteps(int runs) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T2 LOST STEPS ==="));
  printSettings();
  unsigned long glitches0 = switchGlitches;
  for (int run = 1; run <= runs; run++) {
    Serial.print(F(">> Run ")); Serial.print(run); Serial.print('/'); Serial.print(runs); Serial.println(F(" of the scan path..."));
    for (int p = 0; p < ROWS * COLS; p++) {
      float x, y; scanPoint(p, x, y);
      if (!goTo(x, y) || !pause(200)) { stopped(); return; }
    }
  }
  if (!switchCheck()) { stopped(); return; }
  Serial.print(F("   switch noise spikes ignored: ")); Serial.println(switchGlitches - glitches0);
  if (switchGlitches > glitches0)
    Serial.println(F("   !! The switch wires pick up noise. The scan code (no filter) can home at the wrong spot or stop mid-scan."));
  Serial.println(F("What it means: OK on both = the motors don't skip at this speed. If you still see wrong"));
  Serial.println(F("points, it is backlash (T3) or steps/mm (T1). LOST STEPS = slower speed (V 3000), ramp (A 40),"));
  Serial.println(F("more driver current, tighten the pulley/coupling set screws, check the supply isn't current-limiting."));
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
    if (!goTo(0, 0) || !moveTo(isX, mid)) { stopped(); return; }       // arrive moving AWAY from home
    Serial.println(isX ? F(">> (X) Reading A: X arrived moving AWAY from home. MEASURE the X position, type it, Enter:")
                       : F(">> (Y) Reading A: Y arrived moving AWAY from home. MEASURE the Y position, type it, Enter:"));
    float a = askNumber(); if (stopNow) { stopped(); return; }
    if (!moveTo(isX, mid + lround(5 * s)) || !moveTo(isX, mid)) { stopped(); return; }   // arrive moving TOWARD home
    Serial.println(isX ? F(">> (X) Reading B: X came back to the same spot moving TOWARD home. MEASURE again, type it, Enter:")
                       : F(">> (Y) Reading B: Y came back to the same spot moving TOWARD home. MEASURE again, type it, Enter:"));
    float b = askNumber(); if (stopNow) { stopped(); return; }
    if (!isnan(a) && !isnan(b)) {
      float play = fabs(a - b);
      Serial.print(isX ? F("   X backlash = ") : F("   Y backlash = ")); Serial.print(play, 2); Serial.print(F(" mm  "));
      if (play < 0.06) Serial.println(F("(none you can measure)"));
      else if (backlashMm > 0) Serial.println(F("(K is on but the readings still differ: make K bigger)"));
      else { Serial.print(F("-> use K ")); Serial.print(max(0.5, play * 2), 1); Serial.println(F(" (twice the play, at least 0.5)")); }
      if (play > 0.5) Serial.println(F("   That is a lot of play: tighten the belt and the pulley/coupling set screws too."));
    }
  }
  if (!goTo(0, 0)) { stopped(); return; }
  Serial.println(F("With K 0 the two readings differ by the backlash. Type K 1 and run T3 again: they should match."));
  Serial.println(F("=== T3 done. Back home. ==="));
}

// ====================================================================
//  T4: point tour. Visits the 25 scan points like the scan does.
// ====================================================================
void testTour(bool waitEnter, float seconds) {
  if (!ensureHomed()) return;
  Serial.println(F("\n=== T4 POINT TOUR (same order as the scan) ==="));
  printSettings();
  Serial.println(F("Fix a piece of graph paper right in front of the sensor tip and mark where the tip is at each stop."));
  Serial.println(F("Type S to stop."));
  for (int p = 0; p < ROWS * COLS; p++) {
    float x, y; scanPoint(p, x, y);
    if (!goTo(x, y)) { stopped(); return; }
    Serial.print(F("Point ")); Serial.print(p + 1); Serial.print(F("/25 (row ")); Serial.print(p / COLS + 1);
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
  Serial.println(F("What to look for on the paper: a square grid with 5.08 mm gaps."));
  Serial.println(F("  rows 2 and 4 shifted sideways from rows 1, 3, 5   -> backlash (T3, then K)"));
  Serial.println(F("  grid too big or too small                         -> steps/mm (T1)"));
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
  Serial.println(F("  H = home            X <mm> / Y <mm> = go there (0 to 30 mm from home)"));
  Serial.println(F("  W = where + switches                 S = stop, motors off"));
  Serial.println(F("  T1 [mm] = steps/mm test (needs calipers)     T2 [runs] = lost steps test (no tools)"));
  Serial.println(F("  T3 = backlash test (needs calipers)          T4 [s] = point tour (no number = Enter at each)"));
  Serial.println(F("  K <mm> = backlash fix (0 = off)   A <steps> = ramp (0 = off)   V <us> = speed (1500 = scan)"));
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
      if (!moveTo(cmd == 'X', lround(num * spm(cmd == 'X')))) { stopped(); break; }
      where();
      break;
    case 'K':
      if (hasNum) backlashMm = constrain(num, 0.0, 2.0);
      Serial.println(backlashMm > 0 ? F(">> Backlash fix ON: every stop is reached moving away from home.") : F(">> Backlash fix OFF (like the scan code)."));
      printSettings();
      break;
    case 'A':
      if (hasNum) rampSteps = constrain((int)num, 0, 400);
      Serial.println(rampSteps > 0 ? F(">> Ramp ON: moves start and end slowly.") : F(">> Ramp OFF (like the scan code)."));
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
  Serial.println(F("  POSITION TEST - plasma scan stage - READY"));
  Serial.println(F("  Start with T2 (no tools), then T4, T3, T1."));
  Serial.println(F("========================================"));
  help();
  printSettings();
}

void loop() {
  if (lineReady() && line[0] != 0) runCommand();
}
