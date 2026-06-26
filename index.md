# Gesture Controlled Robot
Two-wheeled robot controlled entirely by hand gestures using Bluetooth. An MPU6050 on the controller detects hand tilt and sends directional commands wirelessly via HC-05 Bluetooth modules to the robot. The biggest challenge was calibrating the gyroscope thresholds and getting the two Bluetooth modules to pair correctly.

Engineer | School | Area of Interest | Grade
:--: | :--: | :--: | :--:
Adhrit M | Fallon Middle School | Bioengineering | Incoming 9th Grader

<img width="370" height="497" alt="Screenshot 2026-06-26 at 11 54 13 AM" src="https://github.com/user-attachments/assets/867ce68b-711f-483f-ab02-a532c474ed99" />

---

# Final Milestone

<iframe width="560" height="315" src="https://www.youtube.com/embed/8tovYcNzq1s?si=dPka2gW4jlUlIRyM" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

For my final milestone I completed all 3 modifications I planned — a direct 9V battery connection to the motor driver for more speed, an LCD screen that displays the current direction in real time, and a DFPlayer Mini that plays different sound effects for each movement using MP3 files on a micro SD card.

**Biggest Challenges and Triumphs:**
The hardest part of the whole project was debugging hardware — things like a missing GND wire or a loose breadboard connection would break everything and take forever to find. My biggest triumph was getting the full robot working with all 3 modifications together — motors, Bluetooth, screen, and sound all running at the same time.

**Key Topics I Learned:**
- Bluetooth communication with HC-05 modules
- Gesture sensor data with MPU6050
- Motor control with L298N motor driver
- I2C communication for the LCD screen
- Serial communication for DFPlayer Mini
- Arduino SoftwareSerial library

**What I Hope to Learn Next:**
I want to learn more about PCB design so I can make my own custom circuit boards instead of using jumper wires everywhere. I also want to explore more advanced sensors and eventually build something that can do way more things at the same time.

---

# First Milestone

<iframe width="560" height="315" src="https://www.youtube.com/embed/FbHnvSsPRkI?si=nOQB-oQah-txIDwg" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

For my first milestone I finished the base project — all the wiring and code for the gesture controlled robot.

**How It Works:**
- You tilt your hand
- The MPU6050 detects that and sends the tilt coordinates to the hand Arduino
- The hand Arduino reads the tilt values and sends commands over Bluetooth via HC-05 to the robot's Arduino
- The robot Arduino receives the commands and controls the L298N motor driver
- The L298N drives the two DC motors according to the gesture you chose

**Hardest Parts:**
- Getting the two HC-05 modules to pair
- MPU6050 not being detected due to a breadboard connection issue
- Calibrating the tilt thresholds for each direction
- Motor directions being reversed

**Plans After First Milestone:**
- Connecting a battery directly to the motor driver for more speed
- Adding a screen that displays the current direction
- Adding sound effects with DFPlayer Mini

---

# Schematics

## 📐 Wiring Schematic

<img width="1157" height="683" alt="Gesture Robot Wiring Schematic" src="https://github.com/user-attachments/assets/f5bb9bee-ff7b-4c71-93fe-8ed7543931b7" />

## 🔄 Pin Substitution Chart

### HC-05 Bluetooth Module → HC-SR04 (substitute)
> Used twice — once on the transmitter Arduino, once on the receiver Arduino.

| Real HC-05 Pin | Function | HC-SR04 Substitute Pin | Wire Color |
|:-:|:-|:-:|:-:|
| `VCC` | Power 3.3V–5V | `VCC` | 🔴 Red |
| `GND` | Ground | `GND` | ⚫ Black |
| `TXD` | Transmit data → Arduino RX | `TRIG` | 🔵 Blue |
| `RXD` | Receive data ← Arduino TX | `ECHO` | 🟢 Green |

### MPU6050 Gyroscope → LM393 Comparator IC (substitute)

| Real MPU6050 Pin | Function | LM393 Substitute Pin | Wire Color |
|:-:|:-|:-:|:-:|
| `VCC` | Power 3.3V | `Pin 8 (V+)` | 🔴 Red |
| `GND` | Ground | `Pin 4 (GND)` | ⚫ Black |
| `SDA` | I2C data → Arduino A4 | `Pin 2 (IN-)` | 🟡 Yellow |
| `SCL` | I2C clock → Arduino A5 | `Pin 3 (IN+)` | 🟠 Orange |
| `INT` | Interrupt → Arduino D2 (optional) | `Pin 1 (OUT)` | 🟣 Purple |

### DFPlayer Mini → Passive Buzzer (substitute)
> The buzzer only beeps in Fritzing — the real DFPlayer plays audio files via Serial.

| Real DFPlayer Pin | Function | Buzzer Substitute Pin | Wire Color |
|:-:|:-|:-:|:-:|
| `VCC` | Power 5V | `+ (positive)` | 🔴 Red |
| `GND` | Ground | `- (negative)` | ⚫ Black |
| `RX` | Serial from Arduino TX | `+ (signal)` | 🔵 Blue |

### ✅ No Substitution Needed

| Component | Fritzing Part | Status |
|:-|:-|:-:|
| Arduino Uno × 2 | Arduino Uno | ✅ Exact match |
| L298N Motor Driver | L298N | ✅ Exact match |
| LCD 16×2 (I2C) | LCD 16×2 | ✅ Exact match |
| DC Motors × 2 | DC Motor (yellow gearbox) | ✅ Exact match |
| 9V Battery × 2 | 9V Battery | ✅ Exact match |
| Breadboard | Breadboard | ✅ Exact match |

---

# Code

**Glove Code:**
```cpp
#include <SoftwareSerial.h>
#include <Wire.h>
#include <MPU6050.h>

SoftwareSerial BTSerial(2, 3);
MPU6050 mpu;

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Wire.begin();
  mpu.initialize();
}

void loop() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  char cmd;
  if (ay > 5000) cmd = 'F';
  else if (ay < -8000) cmd = 'B';
  else if (ax < 9000) cmd = 'L';
  else if (ax > 14000) cmd = 'R';
  else cmd = 'S';

  Serial.println(cmd);
  BTSerial.print(cmd);
  delay(100);
}
```

**Robot Code:**
```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mySoftwareSerial(7, 8);
DFRobotDFPlayerMini myDFPlayer;

#define IN1 5
#define IN2 6
#define IN3 10
#define IN4 11

char lastCmd = ' ';

void setup() {
  Serial.begin(9600);
  mySoftwareSerial.begin(9600);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Robot Ready!");
  myDFPlayer.begin(mySoftwareSerial);
  myDFPlayer.volume(30);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.play(1);
  stopMotors();
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd != lastCmd) {
      lastCmd = cmd;
      if (cmd == 'R') { stopMotors(); showDirection("STOPPED"); myDFPlayer.stop(); myDFPlayer.play(5); }
      else if (cmd == 'B') { forward(); showDirection("GOING BACKWARD"); myDFPlayer.stop(); myDFPlayer.play(3); }
      else if (cmd == 'F') { backward(); showDirection("GOING FORWARD"); myDFPlayer.stop(); myDFPlayer.play(2); }
      else if (cmd == 'L') { turnLeft(); showDirection("GOING RIGHT"); myDFPlayer.stop(); myDFPlayer.play(4); }
      else if (cmd == 'S') { turnRight(); showDirection("GOING LEFT"); myDFPlayer.stop(); myDFPlayer.play(4); }
    } else {
      if (cmd == 'R') stopMotors();
      else if (cmd == 'B') forward();
      else if (cmd == 'F') backward();
      else if (cmd == 'L') turnLeft();
      else if (cmd == 'S') turnRight();
    }
  }
}

void showDirection(String dir) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Direction:");
  lcd.setCursor(0, 1);
  lcd.print(dir);
}

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
```

---

# Bill of Materials

| **Part** | **Note** | **Price** | **Link** |
|:--:|:--:|:--:|:--:|
| Car Chassis Kit | Robot base with motors and wheels | $39.99 | [Link](https://www.amazon.com/dp/B0DJ7BT1V5) |
| Screwdriver Kit | For assembly | $5.94 | [Link](https://www.amazon.com/Small-Screwdriver-Set-Mini-Magnetic/dp/B08RYXKJW9) |
| Arduino Uno Clone x2 | Main controllers for robot and glove | $14.98 | [Link](https://www.amazon.com/ELEGOO-Board-ATmega328P-ATMEGA16U2-Compliant/dp/B01EWOE0UU) |
| Electronics Kit | Jumper wires, resistors, components | $14.00 | [Link](https://www.amazon.com/Smraza-Electronics-Potentiometer-tie-Points-Breadboard/dp/B0B62RL725) |
| Breadboard Kit | For prototyping connections | $8.79 | [Link](https://www.amazon.com/Breadboards-Solderless-Breadboard-Distribution-Connecting/dp/B07DL13RZH) |
| Micro USB Cable | For programming Arduinos | $5.00 | [Link](https://www.amazon.com/Charging-Transfer-Android-Trustable-MYFON/dp/B098DW7485) |
| MPU6050 Accelerometer | Detects hand tilt gestures | $9.00 | [Link](https://www.amazon.com/dp/B0D2TJVMNY) |
| HC-05 Bluetooth x2 | Wireless communication between glove and robot | $9.00 | [Link](https://www.amazon.com/DSD-TECH-HC-05-Pass-through-Communication/dp/B01G9KSAF6) |
| Breadboard Power Supply | Powers breadboard components | $8.00 | [Link](https://www.amazon.com/ALAMSCN-Solderless-Breadboard-Battery-Arduino/dp/B08JYPMCZY) |
| 9V Batteries | Powers robot and glove | $8.69 | [Link](https://www.amazon.com/Amazon-Basics-Performance-All-Purpose-Batteries/dp/B00MH4QM1S) |
| Velcro Tape | Mounts components to glove and robot | $8.00 | [Link](https://www.amazon.com/Art3d-Sticky-Double-Sided-Command-Adhesive/dp/B0B58FGF8H) |
| DMM | Multimeter for debugging | $9.99 | [Link](https://www.amazon.com/dp/B0CXM242J1) |
| LCD Screen 16x2 with I2C | Displays current direction on robot | $9.00 | [Link](https://www.amazon.com/GeeekPi-Character-Backlight-Raspberry-Electrical/dp/B07S7PJYM6) |
| DFPlayer Mini | Plays sound effects for each movement | $9.00 | [Link](https://www.amazon.com/DFPlayer-A-Mini-MP3-Player/dp/B089D5NLW1) |
| Jumper Wires | Connects all components together | $6.00 | [Link](https://www.amazon.com/ELEGOO-Solderless-Flexible-Breadboard-Compatible/dp/B09ZQP9LB6) |
| 8GB Micro SD Card | Stores MP3 sound files for DFPlayer | $20.00 | [Link](https://www.amazon.com/SanDisk-microSD-High-Capacity-microSDHC/dp/B00488G6P8) |
| L298N Mini Motor Driver | Controls the two DC motors | $7.00 | [Link](https://www.amazon.com/WWZMDiB-Channel-Bridge-Electric-Projects/dp/B0BD53Q7TT) |
| Speaker 8 Ohm | Outputs sound from DFPlayer Mini | $10.00 | [Link](https://www.amazon.com/MakerHawk-Full-Range-Advertising-Separating-JST-PH2-0mm-2/dp/B07FTB281F) |

---

## Other Resources/Examples

- [Hand Gesture Control Robot via Bluetooth - Hackster.io](https://www.hackster.io/embeddedlab786/hand-gesture-control-robot-via-bluetooth-94b13d)
- [Hand Gesture Controlled Robot - YouTube](https://www.youtube.com/watch?v=BXXAcFOTnBo)
