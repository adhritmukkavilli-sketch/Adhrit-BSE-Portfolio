# Gesture Controlled Robot 
Two-wheeled robot controlled entirely by hand gestures using Bluetooth. An MPU6050 on the controller detects hand tilt and sends directional commands wirelessly via HC-05 Bluetooth modules to the robot. The biggest challenge was calibrating the gyroscope thresholds and getting the two Bluetooth modules to pair correctly."

Engineer | School | Area of Interest | Grade
:--: | :--: | :--: | :--:
Adhrit M | Fallon Middle School | Bioengineering | Incoming 9th Grader

**Replace the BlueStamp logo below with an image of yourself and your completed project. Follow the guide [here](https://tomcam.github.io/least-github-pages/adding-images-github-pages-site.html) if you need help.**

![Headstone Image](logo.svg)
  
# Final Milestone

<iframe width="560" height="315" src="https://www.youtube.com/embed/8tovYcNzq1s?si=dPka2gW4jlUlIRyM" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

For my final milestone I completed all 3 modifications I planned — a direct 9V battery connection to the motor driver for more speed, an LCD screen that displays the current direction in real time, and a DFPlayer Mini that plays different sound effects for each movement using MP3 files on a micro SD card.

**Biggest Challenges and Triumphs:**
The hardest part of the whole project was debugging hardware — things like a missing GND wire or a loose breadboard connection would break everything and take forever to find. My biggest triumph was getting the full robot working with all 3 modifications together — motors, Bluetooth, screen, and sound all running at the same time.

**Key Topics I Learned:**
- Bluetooth communication with HC-05 modules
- gesture sensor data with MPU6050
- Motor control with L298N motor driver
- I2C communication for the LCD screen
- Serial communication for DFPlayer Mini
- Arduino SoftwareSerial library

**What I Hope to Learn Next:**
I want to learn more about PCB design so I can make my own custom circuit boards instead of using jumper wires everywhere. I also want to explore more advanced sensors and eventually build something that can do way more things at the same time 

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


# Schematics 
Here's where you'll put images of your schematics. [Tinkercad](https://www.tinkercad.com/blog/official-guide-to-tinkercad-circuits) and [Fritzing](https://fritzing.org/learning/) are both great resoruces to create professional schematic diagrams, though BSE recommends Tinkercad becuase it can be done easily and for free in the browser. 

## Code

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
# Bill of Materials

| **Part** | **Note** | **Price** | **Link** |
|:--:|:--:|:--:|:--:|
| Car Chassis Kit | Robot base with motors and wheels | $39.99 | <a href="https://www.amazon.com"> Link </a> |
| Screwdriver Kit | For assembly | $5.94 | <a href="https://www.amazon.com"> Link </a> |
| Arduino Uno Clone x2 | Main controllers for robot and glove | $14.98 | <a href="https://www.amazon.com"> Link </a> |
| Electronics Kit | Jumper wires, resistors, components | $14.00 | <a href="https://www.amazon.com"> Link </a> |
| Breadboard Kit | For prototyping connections | $8.79 | <a href="https://www.amazon.com"> Link </a> |
| Arduino Nano 33 | Glove controller | $39.70 | <a href="https://www.amazon.com"> Link </a> |
| Micro USB Cable | For programming Arduinos | $5.00 | <a href="https://www.amazon.com"> Link </a> |
| MPU6050 Accelerometer | Detects hand tilt gestures | $9.00 | <a href="https://www.amazon.com"> Link </a> |
| HC-05 Bluetooth x2 | Wireless communication between glove and robot | $9.00 | <a href="https://www.amazon.com"> Link </a> |
| Breadboard Power Supply | Powers breadboard components | $8.00 | <a href="https://www.amazon.com"> Link </a> |
| 9V Batteries | Powers robot and glove | $8.69 | <a href="https://www.amazon.com"> Link </a> |
| Velcro Tape | Mounts components to glove and robot | $8.00 | <a href="https://www.amazon.com"> Link </a> |
| DMM | Multimeter for debugging | $9.99 | <a href="https://www.amazon.com"> Link </a> |
| LCD Screen 16x2 with I2C | Displays current direction on robot | $4.00 | <a href="https://www.amazon.com"> Link </a> |
| DFPlayer Mini | Plays sound effects for each movement | $3.00 | <a href="https://www.amazon.com"> Link </a> |
| Jumper Wires | Connects all components together | $3.00 | <a href="https://www.amazon.com"> Link </a> |
| 8GB Micro SD Card | Stores MP3 sound files for DFPlayer | $5.00 | <a href="https://www.amazon.com"> Link </a> |
| L298N Mini Motor Driver | Controls the two DC motors | $3.00 | <a href="https://www.amazon.com"> Link </a> |
| Speaker | Outputs sound from DFPlayer Mini | $2.00 | <a href="https://www.amazon.com"> Link </a> |
- [Hand Gesture Control Robot via Bluetooth - Hackster.io](https://www.hackster.io/embeddedlab786/hand-gesture-control-robot-via-bluetooth-94b13d)
- [Hand Gesture Controlled Robot - YouTube](https://www.youtube.com/watch?v=BXXAcFOTnBo)
