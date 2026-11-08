// ====================================================================
//  PLASMA TEMPERATURE SCAN v2 — 1 x 1 INCH: 5 x 5 STOPS, 5.08 mm (0.2 in) APART   (Arduino Mega, Serial Monitor)
//
//  v2 = the original scan code + fixes for "it does not stop exactly at the points":
//    * BACKLASH FIX: every stop is reached moving AWAY from home. When X has to go back toward home
//      (rows 2 and 4 of the zig-zag), it goes BACKLASH_MM past the point and comes back.
//      Before, rows 2 and 4 were shifted sideways by the play in the belt / nut, and the accuracy
//      check could not see it (it only finds lost steps).  Measure the play with position_test T3.
//    * RAMP: every move starts and ends slowly (RAMP_STEPS), so the motors are less likely to skip.
//    * SWITCH FILTER: a switch must read "pressed" 5 times in a row, so noise from the motor wires
//      can't home at the wrong spot or stop the scan.
//    * Homing checks that the slow touch really reached the switch.
//  Set X_STEPS_PER_MM / Y_STEPS_PER_MM from position_test T1 if they turn out wrong.
//
//  Serial Monitor: 9600 baud, "New Line". Type a letter + Enter:
//    G = home, then scan      S = stop now      H = home only
//
//  What it does:
//    1. Homes Y (up to its switch), then X (right to its switch). Home = 2 mm off both switches.
//    2. Scans a 1 x 1 inch square that starts at home (the corner), like the original code:
//       every stop is one 5.08 mm move away from the last, and it measures AFTER each move.
//       Left/right: 5 stops at 5.08, 10.16, 15.24, 20.32, 25.40 mm  (5 moves x 5.08 = 1 inch)
//       Up/down:    5 rows  at 5.08, 10.16, 15.24, 20.32, 25.40 mm  (5 moves x 5.08 = 1 inch)
//       Zig-zag: row 1 left, row 2 right, ...  25 stops in all. The corner itself is not measured.
//    3. At every point: 30 readings, 1 s apart, then the average.
//    4. Goes home, touches both switches and prints how accurate the positions were.
//    5. Prints a table of the averages (laid out like the stage) and a CSV block for Excel.
//
//  Movement: one motor at a time (Y, then X), 1500 us per half step (the original code's speed),
//  ramped at both ends, every stop reached moving away from home, both motors locked for the whole scan.
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
const float BACKLASH_MM = 1.0;             // backlash fix: go this far past a point, then come back (0 = off).
                                           //   Must be bigger than the play position_test T3 measures.
                                           //   Near home the overshoot is cut so it never gets closer than
                                           //   1 mm to a switch (home is only 2 mm from the switches).
const int   RAMP_STEPS = 40;               // ramp: speed up / slow down over this many steps (0 = off)
const unsigned int RAMP_START_US = 3000;   // ramp: half step time at the start and end of every move

// ---- scan ----
const int   ROWS = 5, COLS = 5;            // 5 stops each way, each one 5.08 mm further
const float X_GAP_MM = 5.08;               // left/right gap between stops (0.2 inch)
const float Y_GAP_MM = 5.08;               // up/down gap between rows (0.2 inch)
const float CORNER_X = 0.0, CORNER_Y = 0.0; // corner of the 1-inch square = home
const int   READINGS = 30;
const unsigned long READING_MS = 1000;     // 1 s apart

long xPos = 0, yPos = 0;                   // position in steps from home (where the Arduino TOLD it to go)
bool stopNow = false;
unsigned long switchGlitches = 0;          // noise spikes on the switch wires that the filter ignored
float avgC[ROWS][COLS], sdC[ROWS][COLS];

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
//  steps > 0 = away from home (X left / Y down), steps < 0 = toward home.
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

// Move one axis to `target` (steps from home).
// Backlash fix: a move TOWARD home goes BACKLASH_MM past the target and then comes back, so every
// stop is reached moving AWAY from home - the same way home itself was reached (backing off the switch).
// Then the play in the belt / nut is always on the same side and does not shift the point.
bool moveTo(bool isX, long target) {
  float spm = isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM;
  long pos = isX ? xPos : yPos;
  long comp = lround(BACKLASH_MM * spm);
  if (target < pos && comp > 0) {
    long past = target - comp;
    long lowest = -lround(BACKOFF_MM / 2 * spm);           // never closer than 1 mm to the switch
    if (past < lowest) past = lowest;
    if (past < pos) {
      if (move(isX, past - pos, STEP_US, false) < 0) return false;
      pos = past;
    }
  }
  return move(isX, target - pos, STEP_US, false) >= 0;
}

// go to (x, y) in mm: Y first, then X
bool goTo(float xmm, float ymm) {
  if (!moveTo(false, lround(ymm * Y_STEPS_PER_MM))) return false;
  if (!moveTo(true,  lround(xmm * X_STEPS_PER_MM))) return false;
  return true;
}

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
// This finds LOST STEPS. It cannot see backlash (that is what BACKLASH_MM fixes).
void accuracyCheck() {
  Serial.println(F(">> Accuracy check (touching both switches - finds lost steps):"));
  for (int k = 0; k < 2; k++) {
    bool isX = (k == 1);
    float spm = isX ? X_STEPS_PER_MM : Y_STEPS_PER_MM;
    long made = move(isX, -lround((BACKOFF_MM + 4) * spm), isX ? X_TOUCH_US : Y_TOUCH_US, true);
    bool hit = isX ? xSwitch() : ySwitch();
    float off = made / spm - BACKOFF_MM;
    Serial.print(isX ? F("   X off by ") : F("   Y off by "));
    if (made >= 0 && hit) { Serial.print(off, 3); Serial.println(fabs(off) <= 0.25 ? F(" mm  OK") : F(" mm  (steps were lost)")); }
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
void scan() {
  stopNow = false;
  switchGlitches = 0;
  if (!home()) { Serial.println(F(">> Homing failed. Type G to try again.")); return; }

  Serial.println();
  Serial.println(F(">>> Starting scan: 1 x 1 inch from home, 5 x 5 stops, 5.08 mm apart, 30 readings 1 s apart"));
  Serial.print(F(">>> Backlash fix ")); Serial.print(BACKLASH_MM, 2); Serial.print(F(" mm, ramp ")); Serial.print(RAMP_STEPS); Serial.println(F(" steps"));
  int point = 0;

  for (int r = 0; r < ROWS; r++) {
    for (int j = 0; j < COLS; j++) {
      int c = (r % 2 == 0) ? j : COLS - 1 - j;              // zig-zag
      float x = CORNER_X + (c + 1) * X_GAP_MM, y = CORNER_Y + (r + 1) * Y_GAP_MM;
      point++;
      if (point > 1) { Serial.print(F("Moving to point ")); Serial.print(point); Serial.println(F("...")); }
      if (!goTo(x, y)) { motorsOff(); Serial.println(F(">> STOPPED. Type G to start again.")); return; }

      Serial.print(F("--- At point ")); Serial.print(point); Serial.print('/'); Serial.print(ROWS * COLS); Serial.print(F(": X "));
      Serial.print(xPos / X_STEPS_PER_MM, 2); Serial.print(F(" mm, Y ")); Serial.print(yPos / Y_STEPS_PER_MM, 2); Serial.println(F(" mm ---"));

      float vals[READINGS], sum = 0;
      unsigned long t0 = millis();                         // readings exactly 1 s apart (first one 1 s after arriving)
      for (int k = 1; k <= READINGS; k++) {
        if (!waitUntil(t0 + k * READING_MS)) { motorsOff(); Serial.println(F(">> STOPPED. Type G to start again.")); return; }
        float v, t = readTemp(v);
        vals[k - 1] = t; sum += t;
        Serial.print(F("Point ")); Serial.print(point); Serial.print(F(" | Reading ")); Serial.print(k);
        Serial.print(F("/30 | C = ")); Serial.print(t, 2); Serial.print(F(" | F = ")); Serial.println(t * 1.8 + 32, 2);
      }
      float m = sum / READINGS, ss = 0;
      for (int k = 0; k < READINGS; k++) ss += (vals[k] - m) * (vals[k] - m);
      float sd = sqrt(ss / (READINGS - 1));                 // sample standard deviation
      avgC[r][c] = m; sdC[r][c] = sd;
      Serial.println(F("---------------------------"));
      Serial.print(F("AVERAGE at point ")); Serial.print(point); Serial.print(F(" | C = ")); Serial.print(m, 2);
      Serial.print(F(" | F = ")); Serial.print(m * 1.8 + 32, 2); Serial.print(F(" | SD = ")); Serial.println(sd, 2);
      Serial.println(F("---------------------------"));
    }
  }

  Serial.println(F(">>> Scan finished. Going home..."));
  if (goTo(0, 0)) accuracyCheck();
  motorsOff();

  Serial.println();
  Serial.println(F("=== AVERAGE TEMPERATURE (C), laid out like the stage (row 1 on top, home on the right) ==="));
  for (int r = 0; r < ROWS; r++)
    for (int c = COLS - 1; c >= 0; c--) { Serial.print(avgC[r][c], 2); Serial.print(c ? F("\t") : F("\n")); }

  Serial.println();
  Serial.println(F("=== CSV (copy into Excel) ==="));
  Serial.println(F("row,col,x_mm,y_mm,avg_C,avg_F,sd_C"));
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      Serial.print(r + 1); Serial.print(','); Serial.print(c + 1); Serial.print(',');
      Serial.print(lround((CORNER_X + (c + 1) * X_GAP_MM) * X_STEPS_PER_MM) / X_STEPS_PER_MM, 2); Serial.print(',');
      Serial.print(lround((CORNER_Y + (r + 1) * Y_GAP_MM) * Y_STEPS_PER_MM) / Y_STEPS_PER_MM, 2); Serial.print(',');
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
  Serial.println(F("  PLASMA SCAN v2 1 x 1 INCH (5 x 5 stops, 5.08 mm apart) - READY"));
  Serial.println(F("  25 stops, 30 readings each (~15 min)"));
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
