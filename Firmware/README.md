# PomoDesk - Physical Pomodoro Timer
**ESP32-based Pomodoro timer with API options**

## Getting Started

### Requirements

Before compiling and uploading the firmware, make sure the following is installed:

* Arduino IDE 2.x
* Adafruit NeoPixel library
* ESP32 Boards Package by Espressif Systems

### Install ESP32 Board Support

In Arduino IDE:

1. Open **File → Preferences**
2. Add the following URL to **Additional Boards Manager URLs**:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

3. Open **Tools → Board → Boards Manager**
4. Search for **ESP32**
5. Install **ESP32 by Espressif Systems**

### Install Required Libraries

Open **Tools → Manage Libraries** and install:

* Adafruit NeoPixel

### Board Configuration

Select the following settings before uploading:

* Board: **ESP32S3 Dev Module**
* USB CDC On Boot: **Enabled**
* Upload Mode: **UART0 / Default**
* Flash Mode: **QIO 80MHz**
* Partition Scheme: **Default 4MB with SPIFFS** (or larger if desired)

### Uploading

1. Connect the ESP32-S3 via USB-C.
2. Select the correct COM port in Arduino IDE.
3. Compile and upload the firmware.
4. Open the Serial Monitor if debugging is required.

After flashing, the device will boot directly into the **Idle** state and is ready to use.




# V1.0:

## Features

- Physical one-button control
- Idle, Focus, Break states
- Multiple LED transition modes
- Settings saved in ESP32 Preferences memory

# V1.1:

**Now with a configurable NeoPixel LED ring and a built-in local configuration webserver.**

## Features

- Config state
- Adjustable state colors
- Adjustable Focus, Break, and Idle times
- Adjustable transition speed
- Brightness control with ECO mode
- RGB cycle mode
- Local Wi-Fi configuration portal
- Deep sleep mode with button wake-up

## Controls

- Short press: step through states
- Hold 1.5 seconds, release: enter or exit config mode
- Hold 3 seconds, release: toggle RGB cycle mode
- Hold 6 seconds, release: go to sleep

## States

PomoDesk follows the Pomodoro technique: alternating focused work sessions with short breaks to maintain concentration and prevent mental fatigue.

The device has three primary states:

* **Idle (Orange)** – Ready to start a new session.
* **Focus (Green)** – Active work period.
* **Break (Red)** – Short recovery period.

By default, PomoDesk uses a **45-minute Focus session** followed by a **5-minute Break**. These timings can be customized through the configuration webserver in V1.1.


## Config Mode

When config mode is active, PomoDesk creates its own Wi-Fi network:

- Wi-Fi: `PomoDesk-Config`
- Password: `pomodesk`
- Open: `http://pomodesk.local`
- Fallback: `http://10.10.10.1`

The web interface lets you customize colors, timers, transitions, speed, brightness, reset defaults, exit config mode, or put the device to sleep.


---

*Feel free to send me constructive criticism or things you would like me to implement, or even if you would like to help me develop this system!*
