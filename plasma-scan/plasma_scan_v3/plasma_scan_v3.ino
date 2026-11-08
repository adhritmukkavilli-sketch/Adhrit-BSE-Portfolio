// ====================================================================
//  PLASMA TEMPERATURE SCAN v3 — EXACTLY 1 x 1 INCH: 6 x 6 STOPS, 5.08 mm (0.2 in) APART   (Arduino Mega)
//
//  Serial Monitor: 9600 baud, "New Line". Type a letter + Enter:
//    G = home, then scan      S = stop now      H = home only
//
//  What it does:
//    1. Homes Y (up to its switch), then X (right to its switch). Home = 2 mm off both switches.
//       Home is the top-right corner of the square, and it is the first stop.
//    2. Covers the 1 x 1 inch square EXACTLY: 6 rows of 6 stops, at 0, 5.08, 10.16, 15.24, 20.32
//       and 25.40 mm from home on each axis (5 gaps x 5.08 mm = 25.40 mm = 1 inch).
//       The first and last stops sit on the edges of the square. 36 stops in all.
//    3. Never goes backward into a stop: every row is scanned the same way (right to left) and the
//       rows go top to bottom, so X only moves LEFT and Y only moves DOWN into a stop - the same way
//       home is reached. The play (backlash) in the belt / lead screw can't shift any stop.
//       The only backward move: between rows, X returns to 1 mm right of the row start (no readings
//       there), Y moves down one row, then X moves 1 mm left into the first stop of the new row.
//    4. At every stop: waits SETTLE_MS, then 30 readings 1 s apart, then the average.
//    5. Goes home, touches both switches and prints how accurate the positions were (finds lost steps).
//    6. Prints a table of the averages (laid out like the stage) and a CSV block for Excel.
//
//  Also: every move starts and ends slowly (ramp), and a limit switch must read "pressed" 5 times
//  in a row, so noise from the motor wires can't stop the scan or home at the wrong spot.
//
//  The stops are only exactly 1 inch apart edge to edge if X_STEPS_PER_MM / Y_STEPS_PER_MM are
//  right for the drivers' microstep settings. Check them once with position_test (T1).
//
//  Wiring:
//    X driver: PUL+ 10, DIR+ 9, ENA+ 8      Y driver: PUL+ 7, DIR+ 4, ENA+ 5
//    PUL-, DIR-, ENA- of BOTH drivers -> Mega GND
//    AD8495 OUT -> A0, V+ -> 5V, GND -> GND
//    X limit switch -> pin 2, Y limit switch -> pin 3 (C -> GND, NC -> pin)
// ====================================================================

// ---- pins ----
const int xStepPin = 10, xDirPin = 9, xEnaPin = 8;
const int yStepPin = 7,  yDirPin = 4, yEnaPin = 5;
const int tempPin  = A0;
const int xLimitPin = 2, yLimitPin = 3;

// ---- directions ----
const int X_LEFT = HIGH, X_RIGHT = LOW;    // right = toward the X switch (home)
const int Y_DOWN = HIGH, Y_UP    = LOW;    // up    = toward the Y switch (home)

// ---- motors ----
const float X_STEPS_PER_MM = 20.0;         // 508 steps per inch
const float Y_STEPS_PER_MM = 160.0;        // 4064 steps per inch
const unsigned int STEP_US  = 1500;        // half step time (the original code's speed)
const unsigned int X_TOUCH_US = 1500;      // careful touch of the X switch (X at its normal speed: tested OK)
const unsigned int Y_TOUCH_US = 3000;      // slow, careful touch of the Y switch
const float BACKOFF_MM = 2.0;              // home = 2 mm off each switch
const int   RAMP_STEPS = 40;               // speed up / slow down over this many steps (0 = off)
const unsigned int RAMP_START_US = 3000;   // half step time at the start and end of every move

// ---- scan ----
const int   POINTS = 6;                    // stops per row AND number of rows (6 x 6 = 36 stops)
const float SIDE_MM = 25.4;                // the square: 1 inch. First stop at 0, last stop at exactly 25.4 mm
const float GAP_MM = SIDE_MM / (POINTS - 1);   // 5.08 mm (0.2 inch) between stops
const float LEAD_IN_MM = 1.0;              // between rows, X goes this far past the row start and then
                                           //   moves forward into the first stop. It must be bigger than
                                           //   the play position_test T3 measures. Max 1.5 mm, because home
                                           //   is only 2 mm from the switch (bigger values are cut to 1.5).
const float LEAD_IN_SAFE_MM = (LEAD_IN_MM < BACKOFF_MM - 0.5) ? LEAD_IN_MM : BACKOFF_MM - 0.5;
const int   READINGS = 30;
const unsigned long READING_MS = 1000;     // 1 s apart
const unsigned long SETTLE_MS = 1000;      // wait this long after arriving before the first reading

long xPos = 0, yPos = 0;                   // position in steps from home (where the Arduino TOLD it to go)
bool stopNow = false;
unsigned long switchGlitches = 0;          // noise spikes on the switch wires that the filter ignored
float avgC[POINTS][POINTS], sdC[POINTS][POINTS];   // [row][column]

// ====================================================================
//  basics
// ====================================================================
// NC switch with pull-up: HIGH = pressed (or a broken wire). Must read HIGH 5 times in a row,
// so a spike of noise from the motor wires is not taken as a click.
bool switchPressed(int pin) {
  if (digitalRead(pin) == LOW) return false;
  for (byte k = 0; k < 4; k++) {
    delayMicroseconds(20);
    if (digitalRead(pin) == LOW) { switchGlitches++; return false; }
  }
  return true;
}
bool xSwitch() { return switchPressed(xLimitPin); }
bool ySwitch() { return switchPressed(yLimitPin); }

void motorsOn()  { digitalWrite(xEnaPin, LOW);  digitalWrite(yEnaPin, LOW);  delay(50); }
void motorsOff() { digitalWrite(xEnaPin, HIGH); digitalWrite(yEnaPin, HIGH); }

void checkStop() {
  while (Serial.available()) { char c = Serial.read(); if (c == 'S' || c == 's') stopNow = true; }
}

// Wait until millis() reaches `until`, motors locked. false = S was typed.
bool waitUntil(unsigned long until) {
  while ((long)(millis() - until) < 0) { checkStop(); if (stopNow) return false; delay(2); }
  return true;
}

// AD8495: average of 50 samples over 0.2 s.  C = (V - 1.25) / 0.005
float readTemp(float &volts) {
  long sum = 0;
  for (int i = 0; i < 50; i++) { sum += analogRead(tempPin); delayMicroseconds(4000); }
  volts = sum / 50.0 * 5.0 / 1023.0;
  return (volts - 1.25) / 0.005;
}

// ====================================================================
//  movement: one motor at a time
//  steps > 0 = forward, away from home (X left / Y down), steps < 0 = backward, toward home.
//  untilSwitch = true: stop when the switch clicks (homing). Returns steps made, -1 = stopped / problem.
// ====================================================================
long move(bool isX, long steps, unsigned int us, bool untilSwitch) {
  if (steps == 0) return 0;
  int stepPin = isX ? xStepPin : yStepPin;
  bool away = steps > 0;
  long n = labs(steps);
  digitalWrite(isX ? xDirPin : yDirPin, isX ? (away ? X_LEFT : X_RIGHT) : (away ? Y_DOWN : Y_UP));
  delay(5);
  for (long i = 0; i < n; i++) {
    bool sw = isX ? xSwitch() : ySwitch();
    if (!away && sw) {
      if (untilSwitch) return i;
      Serial.println(F("!! A limit switch clicked in the middle of a move. Stopped. Type G to start again."));
      return -1;
    }
    if (i % 100 == 0) { checkStop(); if (stopNow) return -1; }
    unsigned int d = us;
    if (RAMP_STEPS > 0 && RAMP_START_US > us) {            // ramp: slow at both ends of the move
      long edge = min(i, n - 1 - i);
      if (edge < RAMP_STEPS) d = RAMP_START_US - (unsigned long)(RAMP_START_US - us) * edge / RAMP_STEPS;
    }
    digitalWrite(stepPin, HIGH); delayMicroseconds(d);
    digitalWrite(stepPin, LOW);  delayMicroseconds(d);
    if (isX) xPos += away ? 1 : -1; else yPos += away ? 1 : -1;
  }
  return n;
}

// move one axis straight to `mm` from home (X: + = left, Y: + = down)
bool moveTo(bool isX, float mm) {
  long target = lround(mm * (isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM));
  return move(isX, target - (isX ? xPos : yPos), STEP_US, false) >= 0;
}

// where stop number i (0 .. POINTS-1) is on either axis, in mm from home
float stopMm(int i) { return i * GAP_MM; }

// home one motor: off the switch if on it, drive to it, back off 3 mm, touch slowly, back off 2 mm = 0
bool homeOne(bool isX) {
  float spm = isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM;
  bool (*sw)() = isX ? xSwitch : ySwitch;
  if (sw()) { if (move(isX, lround(3 * spm), STEP_US, true) < 0) return false; }
  if (sw()) { Serial.println(isX ? F("!! X could not move off its switch.") : F("!! Y could not move off its switch.")); return false; }
  if (move(isX, -lround(340 * spm), STEP_US, true) < 0) return false;
  if (!sw()) { Serial.println(isX ? F("!! X never reached its switch (pin 2).") : F("!! Y never reached its switch (pin 3).")); return false; }
  if (move(isX, lround(3 * spm), STEP_US, true) < 0) return false;
  if (move(isX, -lround(5 * spm), isX ? X_TOUCH_US : Y_TOUCH_US, true) < 0) return false;
  if (!sw()) { Serial.println(isX ? F("!! X slow touch missed the switch.") : F("!! Y slow touch missed the switch.")); return false; }
  if (move(isX, lround(BACKOFF_MM * spm), STEP_US, true) < 0) return false;
  if (isX) xPos = 0; else yPos = 0;
  return true;
}

bool home() {
  motorsOn();
  Serial.println(F(">> Homing Y..."));
  if (!homeOne(false)) { motorsOff(); return false; }
  Serial.println(F(">> Homing X..."));
  if (!homeOne(true))  { motorsOff(); return false; }
  Serial.println(F(">> Home (X 0, Y 0)."));
  return true;
}

// from home: touch each switch slowly and compare with the step count.
// This finds LOST STEPS (a motor skipped, or a pulley / coupling slipped).
void accuracyCheck() {
  Serial.println(F(">> Accuracy check (touching both switches - finds lost steps):"));
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 1);
    float spm = isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM;
    long made = move(isX, -lround((BACKOFF_MM + 4) * spm), isX ? X_TOUCH_US : Y_TOUCH_US, true);
    bool hit = isX ? xSwitch() : ySwitch();
    float off = made / spm - BACKOFF_MM;
    Serial.print(isX ? F("   X off by ") : F("   Y off by "));
    if (made >= 0 && hit) { Serial.print(off, 3); Serial.println(fabs(off) <= 0.10 ? F(" mm  OK") : F(" mm  (steps were lost)")); }
    else Serial.println(F("?? (switch not reached)"));
    move(isX, lround(BACKOFF_MM * spm), STEP_US, true);
    if (isX) xPos = 0; else yPos = 0;
  }
  if (switchGlitches > 0) {
    Serial.print(F("   Switch noise spikes ignored during this run: ")); Serial.println(switchGlitches);
    Serial.println(F("   (keep the switch wires away from the motor wires, or twist each switch pair together)"));
  }
}

// ====================================================================
//  the scan
// ====================================================================
// readings at the stop the stage is at now (row r, column c). false = S was typed
bool measure(int r, int c, int point) {
  Serial.print(F("--- At point ")); Serial.print(point); Serial.print('/'); Serial.print(POINTS * POINTS);
  Serial.print(F(" (row ")); Serial.print(r + 1); Serial.print(F(", col ")); Serial.print(c + 1); Serial.print(F("): X "));
  Serial.print(xPos / X_STEPS_PER_MM, 2); Serial.print(F(" mm, Y ")); Serial.print(yPos / Y_STEPS_PER_MM, 2); Serial.println(F(" mm ---"));

  float vals[READINGS], sum = 0;
  unsigned long t0 = millis() + SETTLE_MS;               // first reading SETTLE_MS after arriving, then 1 s apart
  for (int k = 0; k < READINGS; k++) {
    if (!waitUntil(t0 + k * READING_MS)) return false;
    float v, t = readTemp(v);
    vals[k] = t; sum += t;
    Serial.print(F("Point ")); Serial.print(point); Serial.print(F(" | Reading ")); Serial.print(k + 1); Serial.print('/'); Serial.print(READINGS);
    Serial.print(F(" | C = ")); Serial.print(t, 2); Serial.print(F(" | F = ")); Serial.println(t * 1.8 + 32, 2);
  }
  float m = sum / READINGS, ss = 0;
  for (int k = 0; k < READINGS; k++) ss += (vals[k] - m) * (vals[k] - m);
  float sd = sqrt(ss / (READINGS - 1));                  // sample standard deviation
  avgC[r][c] = m; sdC[r][c] = sd;
  Serial.println(F("---------------------------"));
  Serial.print(F("AVERAGE at point ")); Serial.print(point); Serial.print(F(" | C = ")); Serial.print(m, 2);
  Serial.print(F(" | F = ")); Serial.print(m * 1.8 + 32, 2); Serial.print(F(" | SD = ")); Serial.println(sd, 2);
  Serial.println(F("---------------------------"));
  return true;
}

void stopScan() { motorsOff(); Serial.println(F(">> STOPPED. Type G to start again.")); }

void scan() {
  stopNow = false;
  switchGlitches = 0;
  if (!home()) { Serial.println(F(">> Homing failed. Type G to try again.")); return; }

  Serial.println();
  Serial.print(F(">>> Starting scan: exactly 1 x 1 inch, ")); Serial.print(POINTS); Serial.print(F(" x ")); Serial.print(POINTS);
  Serial.print(F(" stops, ")); Serial.print(GAP_MM, 2); Serial.print(F(" mm apart, ")); Serial.print(READINGS); Serial.println(F(" readings 1 s apart"));
  Serial.println(F(">>> Every row right to left, rows top to bottom: every stop is reached moving left / down."));
  int point = 0;

  for (int r = 0; r < POINTS; r++) {
    for (int c = 0; c < POINTS; c++) {
      point++;
      if (c > 0) {
        // along the row: always forward (left)
        Serial.print(F("Moving to point ")); Serial.print(point); Serial.println(F("..."));
        if (!moveTo(true, stopMm(c))) { stopScan(); return; }
      } else if (r > 0) {
        // new row: X back past the row start (the only backward move, no readings there),
        // Y down one row, then X forward (left) into the first stop
        Serial.print(F("Moving to point ")); Serial.print(point); Serial.println(F(" (next row: X back to the start, Y down)..."));
        if (!moveTo(true, -LEAD_IN_SAFE_MM) || !moveTo(false, stopMm(r)) || !moveTo(true, stopMm(0))) { stopScan(); return; }
      }
      // (point 1 is home itself: it was reached by backing off the switches, also forward)
      if (!measure(r, c, point)) { stopScan(); return; }
    }
  }

  Serial.println(F(">>> Scan finished. Going home..."));
  if (moveTo(false, 0) && moveTo(true, 0)) accuracyCheck();
  motorsOff();

  Serial.println();
  Serial.println(F("=== AVERAGE TEMPERATURE (C), laid out like the stage (row 1 on top, home on the right) ==="));
  for (int r = 0; r < POINTS; r++)
    for (int c = POINTS - 1; c >= 0; c--) { Serial.print(avgC[r][c], 2); Serial.print(c ? F("\t") : F("\n")); }

  Serial.println();
  Serial.println(F("=== CSV (copy into Excel) ==="));
  Serial.println(F("row,col,x_mm,y_mm,avg_C,avg_F,sd_C"));
  for (int r = 0; r < POINTS; r++)
    for (int c = 0; c < POINTS; c++) {
      Serial.print(r + 1); Serial.print(','); Serial.print(c + 1); Serial.print(',');
      Serial.print(lround(stopMm(c) * X_STEPS_PER_MM) / X_STEPS_PER_MM, 2); Serial.print(',');
      Serial.print(lround(stopMm(r) * Y_STEPS_PER_MM) / Y_STEPS_PER_MM, 2); Serial.print(',');
      Serial.print(avgC[r][c], 2); Serial.print(','); Serial.print(avgC[r][c] * 1.8 + 32, 2); Serial.print(',');
      Serial.println(sdC[r][c], 2);
    }
  Serial.println(F("=== end ==="));
  Serial.println(F(">> Done. Motors off. Type G to scan again."));
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
  Serial.println(F("  PLASMA SCAN v3 - EXACTLY 1 x 1 INCH - READY"));
  Serial.print(F("  ")); Serial.print(POINTS * POINTS); Serial.print(F(" stops (")); Serial.print(POINTS); Serial.print(F(" x ")); Serial.print(POINTS);
  Serial.print(F(", ")); Serial.print(GAP_MM, 2); Serial.print(F(" mm apart), ")); Serial.print(READINGS); Serial.print(F(" readings each, about "));
  Serial.print(POINTS * POINTS * ((SETTLE_MS + (READINGS - 1) * READING_MS) / 1000.0 + 1.5) / 60.0, 0); Serial.println(F(" min"));
  Serial.println(F("  G = home + scan   S = stop   H = home only"));
  Serial.println(F("========================================"));
  float v; Serial.print(F("Temperature now: ")); Serial.print(readTemp(v), 2); Serial.println(F(" C"));
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  stopNow = false;
  if (c == 'G' || c == 'g') scan();
  else if (c == 'H' || c == 'h') { if (home()) Serial.println(F(">> Homed. Motors locked. Type G to scan.")); }
  else if (c == 'S' || c == 's') { motorsOff(); Serial.println(F(">> Motors off.")); }
}
