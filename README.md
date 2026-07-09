# PomoDesk - Physical Pomodoro Timer

<img width="1983" height="793" alt="pomo-desk-banner-image" src="https://github.com/user-attachments/assets/f1bfd086-d3e4-4fbc-a294-61849d2e73e7" />

**ESP32-based physical Pomodoro timer with local configuration options**

Designing PCB's, creating or adjusting schematics, doing layout, all these tasks put me in this trance, this FLOW, as some would say.
I've learned it's a gift, not available for everyone, to reach this kind of state. But, there is a small downside to this. This flow-state you have to manage with caution.

To not forget about life, you need something to wake you from this flow every now and then.
The perfect way to do this, for me personally, is to have a Pomodoro timer.


**Focus for 45 minutes, break for 5, then continue.**

<img width="1983" height="793" alt="WhatsApp Image 2026-05-29 at 22 05 28" src="https://github.com/user-attachments/assets/db712639-d5b2-423e-b772-f0439ffff4b0" />

I wanted to create a Pomodoro timer that's not some app counting down on my screen.
It needs to be physical, elegant and as unobtrusive as possible, only showing itself when it's time to break.

**States & Workflow**

PomoDesk follows the Pomodoro technique: a simple productivity method that alternates periods of focused work with short breaks to help maintain concentration and prevent burnout.

The device has three states:

* Idle (Orange) – Ready to start a new session.
* Focus (Green) – A dedicated work period where distractions are minimized and attention stays on a single task.
* Break (Red) – A short recovery period to step away, stretch, grab a drink, or simply reset before the next focus session.

<img width="1774" height="887" alt="WhatsApp Image 2026-05-30 at 12 02 46" src="https://github.com/user-attachments/assets/5a40e451-dc39-4aef-9b0e-190097d0fef9" />

By default, PomoDesk uses a 45-minute focus period followed by a 5-minute break, after which it returns to Idle and waits for the next session. All timings can be customized through the built-in configuration webserver.
 

**Power**

PomoDesk is powered through a standard USB-C connection and is designed to run continuously on any 5V USB power source.

<img width="1983" height="793" alt="WhatsApp Image 2026-05-29 at 22 11 27" src="https://github.com/user-attachments/assets/c3e7b955-128f-4d6b-a01e-8a0cf5e147a6" /> 

It can be powered from:

* A USB wall adapter
* A laptop or desktop USB port
* A USB hub or monitor with USB output
* A power bank for portable use

The ESP32-S3 and LED ring have been optimized for low power consumption, and the built-in ECO mode and deep sleep functionality help minimize energy usage when the device is idle.

This first iteration is far from perfect, but it's a start.

<img width="600" height="499" alt="pomo-desk-orange" src="https://github.com/user-attachments/assets/9fd43c3e-4881-455e-bc60-1d625a76d8db" />

---

# V1.1 - Local Configuration Webserver

Version 1.1 adds a local configuration mode directly on the ESP32-S3.

By pressing and holding the button, PomoDesk can start its own Wi-Fi configuration portal. From there, the timer can be customized without needing to reflash the firmware.

### Button controls

<img width="1774" height="887" alt="WhatsApp Image 2026-05-31 at 11 31 27" src="https://github.com/user-attachments/assets/a8e5a4d2-66d4-4e88-baff-9ad13a10f955" />

* Short press: step through states
* Hold 1.5 seconds, release: enter or exit config mode
* Hold 3 seconds, release: toggle RGB cycle mode
* Hold 6 seconds, release: go to sleep

### Config mode

When config mode is active, PomoDesk creates its own Wi-Fi network:

```text
Wi-Fi: PomoDesk-Config
Password: pomodesk
Open: http://pomodesk.local
Fallback: http://10.10.10.1
```

### Added in v1.1 
* Local Wi-Fi configuration webserver
* Adjustable state colors
* Adjustable Focus, Break and Idle timings
* Multiple LED transition modes
* Adjustable transition speed
* Brightness control
* RGB cycle mode
* ECO brightness mode
* Reset to default settings
* Config mode with blue LED indication
* Deep sleep mode
* Wake-up by button press
* Settings saved to ESP32 memory

---

*Feel free to send me constructive criticism or things you would like me to implement, or even if you would like to help me develop this system!*
