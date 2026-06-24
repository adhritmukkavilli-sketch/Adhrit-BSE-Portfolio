# Gesture Controlled Robot 
Two-wheeled robot controlled entirely by hand gestures using Bluetooth. An MPU6050 on the controller detects hand tilt and sends directional commands wirelessly via HC-05 Bluetooth modules to the robot. The biggest challenge was calibrating the gyroscope thresholds and getting the two Bluetooth modules to pair correctly."

Engineer | School | Area of Interest | Grade
:--: | :--: | :--: | :--:
Adhrit M | Fallon Middle School | Bioengineering | Incoming 9th Grader

**Replace the BlueStamp logo below with an image of yourself and your completed project. Follow the guide [here](https://tomcam.github.io/least-github-pages/adding-images-github-pages-site.html) if you need help.**

![Headstone Image](logo.svg)
  
# Final Milestone

<iframe width="560" height="315" src="https://www.youtube.com/embed/8tovYcNzq1s?si=dPka2gW4jlUlIRyM" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

For your final milestone, explain the outcome of your project. Key details to include are:
- What you've accomplished since your previous milestone
- What your biggest challenges and triumphs were at BSE
- A summary of key topics you learned about
- What you hope to learn in the future after everything you've learned at BSE

# First Milestone

 <iframe width="560" height="315" src="https://www.youtube.com/embed/FbHnvSsPRkI?si=nOQB-oQah-txIDwg" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

First Milestone
- I Finished base progect. (Wiring + Code)

How It Works 
- You tilt your hand. 
- the MPU6050 detects that and sends the tilt coordinates over to the hand Arduino
- Hand Arduino reads the tilt values and sends commands over Bluetooth via HC-05 to the robots arduino 
- Robot Arduino receives the commands and controls the L298N motor driver
- L298N drives the two DC motors according to the gesture you chose


Hardest Parts
- Getting the two HC-05 modules to pair
- MPU6050 not being detected (breadboard connection issue)
- Calibrating the tilt thresholds for each direction
- Motor directions being reversed

Plans after First Milestone
- Connecting a battery directly to the motor driver for more speed
- Adding a screen that displays the way its moving or like a message
- Adding Sound effects with DFPlayer Mini


# Schematics 
Here's where you'll put images of your schematics. [Tinkercad](https://www.tinkercad.com/blog/official-guide-to-tinkercad-circuits) and [Fritzing](https://fritzing.org/learning/) are both great resoruces to create professional schematic diagrams, though BSE recommends Tinkercad becuase it can be done easily and for free in the browser. 

# Code
Here's where you'll put your code. The syntax below places it into a block of code. Follow the guide [here]([url](https://www.markdownguide.org/extended-syntax/)) to learn how to customize it to your project needs. 

Glove code:

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


Robot Code:

#define IN1 5
#define IN2 6
#define IN3 10
#define IN4 11

void setup() {
  Serial.begin(9600);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotors();
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'R') stopMotors();
    else if (cmd == 'F') turnRight();
    else if (cmd == 'B') turnLeft();
    else if (cmd == 'L') forward();
    else if (cmd == 'S') backward();
  }
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
# Bill of Materials
Here's where you'll list the parts in your project. To add more rows, just copy and paste the example rows below.
Don't forget to place the link of where to buy each component inside the quotation marks in the corresponding row after href =. Follow the guide [here]([url](https://www.markdownguide.org/extended-syntax/)) to learn how to customize this to your project needs. 

| **Part** | **Note** | **Price** | **Link** |
|:--:|:--:|:--:|:--:|
| Item Name | What the item is used for | $Price | <a href="https://www.amazon.com/Arduino-A000066-ARDUINO-UNO-R3/dp/B008GRTSV6/"> Link </a> |
| Item Name | What the item is used for | $Price | <a href="https://www.amazon.com/Arduino-A000066-ARDUINO-UNO-R3/dp/B008GRTSV6/"> Link </a> |
| Item Name | What the item is used for | $Price | <a href="https://www.amazon.com/Arduino-A000066-ARDUINO-UNO-R3/dp/B008GRTSV6/"> Link </a> |

# Other Resources/Examples
One of the best parts about Github is that you can view how other people set up their own work. Here are some past BSE portfolios that are awesome examples. You can view how they set up their portfolio, and you can view their index.md files to understand how they implemented different portfolio components.
- [Example 1](https://trashytuber.github.io/Yimin
- gJiaBlueStamp/)
- [Example 2](https://sviatil0.github.io/Sviatoslav_BSE/)
- [Example 3](https://arneshkumar.github.io/arneshbluestamp/)

To watch the BSE tutorial on how to create a portfolio, click here.
