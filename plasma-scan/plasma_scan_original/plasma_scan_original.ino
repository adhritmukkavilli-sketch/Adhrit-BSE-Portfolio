// ====================================================================
//  PLASMA TEMPERATURE SCAN — 1 x 1 INCH: 5 x 5 STOPS, 5.08 mm (0.2 in) APART   (Arduino Mega, Serial Monitor)
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
//  both motors stay locked for the whole scan.
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

// ---- scan ----
const int   ROWS = 5, COLS = 5;            // 5 stops each way, each one 5.08 mm further
const float X_GAP_MM = 5.08;               // left/right gap between stops (0.2 inch)
const float Y_GAP_MM = 5.08;               // up/down gap between rows (0.2 inch)
const float CORNER_X = 0.0, CORNER_Y = 0.0; // corner of the 1-inch square = home
const int   READINGS = 30;
const unsigned long READING_MS = 1000;     // 1 s apart

long xPos = 0, yPos = 0;                   // position in steps from home
bool stopNow = false;
float avgC[ROWS][COLS], sdC[ROWS][COLS];

// ====================================================================
//  basics
// ====================================================================
bool xSwitch() { return digitalRead(xLimitPin) == HIGH; }
bool ySwitch() { return digitalRead(yLimitPin) == HIGH; }

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
    digitalWrite(stepPin, HIGH); delayMicroseconds(us);
    digitalWrite(stepPin, LOW);  delayMicroseconds(us);
    if (isX) xPos += away ? 1 : -1; else yPos += away ? 1 : -1;
  }
  return n;
}

// go to (x, y) in mm: Y first, then X
bool goTo(float xmm, float ymm) {
  if (move(false, lround(ymm * Y_STEPS_PER_MM) - yPos, STEP_US, false) < 0) return false;
  if (move(true,  lround(xmm * X_STEPS_PER_MM) - xPos, STEP_US, false) < 0) return false;
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

// from home: touch each switch slowly and compare with the step count
void accuracyCheck() {
  Serial.println(F(">> Accuracy check (touching both switches):"));
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
}

// ====================================================================
//  the scan
// ====================================================================
void scan() {
  stopNow = false;
  if (!home()) { Serial.println(F(">> Homing failed. Type G to try again.")); return; }

  Serial.println();
  Serial.println(F(">>> Starting scan: 1 x 1 inch from home, 5 x 5 stops, 5.08 mm apart, 30 readings 1 s apart"));
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
  Serial.println(F("  PLASMA SCAN 1 x 1 INCH (5 x 5 stops, 5.08 mm apart) - READY"));
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
