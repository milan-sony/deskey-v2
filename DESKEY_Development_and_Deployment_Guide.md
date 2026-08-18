# DESKEY — Development & Deployment Guide

> A complete reference for how DESKEY works, how the PC Agent communicates with the ESP32 over Bluetooth LE, how the Windows `.exe` is generated, how the application is packaged, and how to safely add future functionality.

---

## 1. What is DESKEY?

DESKEY is a physical desktop buddy built around an ESP32 and OLED display.

It has two main parts:

```text
┌─────────────────────────────┐
│        Windows PC           │
│                             │
│   DESKEY.exe                │
│      Python PC Agent        │
│            │                │
│            │ Bluetooth LE   │
└────────────┼────────────────┘
             │
             ▼
┌─────────────────────────────┐
│           ESP32             │
│                             │
│  BLE Communication          │
│  State / Behavior Engine    │
│  Sleep / Wake                │
│  Heartbeat / PC Awareness   │
│  OLED + RoboEyes            │
└─────────────────────────────┘
```

### Responsibilities

| Component | Responsibility |
|---|---|
| Python PC Agent | Understands what is happening on Windows |
| BLE | Carries PC state from the PC to the ESP32 |
| ESP32 | Runs DESKEY's behavior and hardware logic |
| OLED | Displays DESKEY's face/animations |
| RoboEyes | Provides expressive eye animations |
| Optional Wi-Fi | Provides the web dashboard; it is not required for core PC awareness |

---

# 2. Why Python is used

Python was chosen specifically for the **Windows PC Agent**.

The PC Agent needs to detect:

- Foreground application
- Keyboard activity
- Mouse/keyboard idle time
- Audio/music
- Browsing
- Coding
- Gaming
- Video/YouTube
- Other PC context

Python is a good fit because it provides a simple development environment and libraries for Windows/system monitoring, audio detection and BLE.

The important distinction is:

> Python gives DESKEY awareness of the Windows PC. The ESP32 turns that information into physical behavior.

Python is **not required because the ESP32 needs Python**. The Python program exists because the ESP32 cannot directly know which Windows application is active or what the user is doing on the PC.

---

# 3. Why BLE is used

The current architecture is **BLE-first**.

The PC and ESP32 do **not** need to be on the same Wi-Fi network for core DESKEY communication.

```text
Windows
   │
   │ Python
   ▼
Bluetooth LE
   │
   ▼
ESP32
```

Wi-Fi is optional and is only useful for the ESP32 web dashboard.

Therefore:

- No ESP32 IP address is required for Python communication.
- No Wi-Fi connection is required for the core PC-awareness path.
- The Python Agent discovers DESKEY over BLE.
- The Python Agent reconnects when the BLE connection is lost.

---

# 4. BLE architecture

DESKEY uses three important BLE concepts:

```text
BLE Device
   │
   └── Service
          │
          └── Characteristic
```

## 4.1 BLE device name

The ESP32 advertises itself as:

```text
DESKEY
```

Python searches for this device.

---

## 4.2 Service UUID

```text
7f2d0001-6b4a-4f43-9b9a-9f5b1c2e0001
```

This identifies the DESKEY BLE service.

Think of it as the BLE container/folder for DESKEY communication.

---

## 4.3 State Characteristic UUID

```text
7f2d0002-6b4a-4f43-9b9a-9f5b1c2e0001
```

This identifies the writable characteristic used to send the current PC state.

For example:

```json
{
  "state": "coding",
  "idle_seconds": 0
}
```

The ESP32 receives the packet and updates its state system.

---

# 5. UUIDs must match

The UUID values must be the same in the Python Agent and ESP32 firmware.

```text
ESP32                         Python
────────────────────────────────────────
Service UUID  ────────────── Service UUID
State UUID    ────────────── State UUID
```

If one side is changed without changing the other, communication will fail.

The UUID identifies the **DESKEY communication protocol**, not a specific physical ESP32.

This means another ESP32 can use the same firmware and the same UUIDs.

---

# 6. What happens when DESKEY runs

The current PC application flow is:

```text
User opens DESKEY
        │
        ▼
Python Agent starts
        │
        ▼
BLE discovery
        │
        ▼
Find DESKEY
        │
        ▼
BLE connection
        │
        ▼
PC activity detection
        │
        ▼
Current state sent over BLE
        │
        ▼
ESP32 receives state
        │
        ▼
ESP32 updates DESKEY behavior
        │
        ▼
OLED animation
```

The application is **not configured to start automatically with Windows**.

The intended behavior is:

```text
Open DESKEY
    ↓
Use DESKEY
    ↓
Close DESKEY
    ↓
Agent stops
```

When the application is closed, PC monitoring and the BLE connection are stopped.

---

# 7. PC activity detection

The Python Agent determines the current PC context.

Examples:

```text
VS Code       → coding
Chrome        → browsing
Music player  → music
Keyboard      → typing
Game          → gaming
Video         → watching
No activity   → idle
```

The agent also calculates Windows idle time.

A state packet can contain:

```json
{
  "state": "coding",
  "idle_seconds": 0
}
```

The ESP32 uses both the state and idle information.

---

# 8. Sleep and wake behavior

DESKEY can become sleepy when the PC is inactive.

Conceptually:

```text
PC activity
    │
    ▼
No keyboard/mouse activity
    │
    ▼
Idle threshold reached
    │
    ▼
DESKEY sleeps
    │
    ▼
User moves mouse / presses key
    │
    ▼
DESKEY wakes
    │
    ▼
Current PC state is applied
```

### DESKEY's personality

> **DESKEY is extremely lazy. 😂** If your PC sits quietly for too long, he'll take that as an invitation for a nap. 💤
>
> No keyboard? No mouse? No activity?
>
> **DESKEY:** “Sounds like a perfect time for a nap.” 😴
>
> Move the mouse or press a key and he'll wake up like nothing happened:
>
> **“I wasn't sleeping. I was… conserving energy.”** 👀

---

# 9. BLE reconnection behavior

The Python Agent is designed to keep the PC activity loop separate from BLE connection attempts.

The intended flow is:

```text
BLE connection lost
        │
        ▼
Fresh BLE discovery
        │
        ▼
Find DESKEY
        │
        ▼
Reconnect
        │
        ▼
Resume state updates
```

If DESKEY is temporarily unavailable:

```text
Attempt 1
   ↓
Not found
   ↓
Attempt 2
   ↓
Not found
   ↓
Attempt 3
   ↓
...
   ↓
DESKEY becomes available
   ↓
Connect
```

This is useful if:

- ESP32 is restarted
- ESP32 temporarily goes out of range
- Bluetooth is temporarily disabled
- The BLE connection drops

The Python Agent should not depend permanently on an old ESP32 address.

---

# 10. Heartbeat and autonomous behavior

The PC Agent continuously sends current state information.

The ESP32 uses valid packets as an indication that Python is alive.

The project uses a heartbeat timeout so that a short BLE reconnect does not immediately force autonomous mode.

Conceptually:

```text
Python connected
       │
       ▼
Heartbeat packets
       │
       ▼
ESP32 knows PC Agent is alive
```

If communication stops for long enough:

```text
No heartbeat
     │
     ▼
Timeout
     │
     ▼
Autonomous Mode
```

This allows DESKEY to continue behaving independently instead of becoming completely frozen whenever the PC Agent temporarily disappears.

---

# 11. ESP32 responsibilities

The ESP32 firmware handles the physical DESKEY.

It is responsible for things such as:

- BLE server
- BLE state reception
- State changes
- OLED display
- RoboEyes animations
- Sleep behavior
- Wake behavior
- Heartbeat handling
- Autonomous behavior
- Optional Wi-Fi dashboard
- Hardware-specific logic

The ESP32 receives the current PC context rather than trying to understand Windows itself.

---

# 12. OLED and RoboEyes

The OLED is DESKEY's face.

RoboEyes provides the expressive eye system.

PC state can therefore be translated into visual behavior:

```text
coding
  ↓
Coding expression

browsing
  ↓
Browsing expression

music
  ↓
Music expression + movement

sleepy
  ↓
Sleepy expression
```

The ESP32 remains responsible for the animation itself.

Python only needs to communicate the context.

---

# 13. Music mode

Music is detected by the PC Agent.

The Python side sends:

```text
music
```

The ESP32 then runs the music behavior.

The current DESKEY firmware includes a music animation with smooth left-to-right/right-to-left movement rather than a simple shake.

---

# 14. Wi-Fi's current role

Wi-Fi is **optional**.

The core path is:

```text
Python
   │
   │ BLE
   ▼
ESP32
```

Optional dashboard:

```text
Browser
   │
   │ Wi-Fi
   ▼
ESP32 Web Dashboard
```

Therefore, if Wi-Fi is disconnected:

- Core BLE PC awareness can continue.
- OLED behavior can continue.
- Sleep/wake can continue.
- Python can continue communicating with the ESP32.
- The web dashboard will not be available through the network.

An ESP32 IP address is not required for the Python Agent.

---

# 15. Arduino libraries

For the current ESP32 firmware, the external Arduino libraries used for the OLED/animation side include:

```text
Adafruit GFX Library
Adafruit SSD1306
RoboEyes
```

The ESP32 Arduino board package provides platform functionality such as:

```text
BLE
WiFi
WebServer
Preferences
Wire
```

### Important BLE library issue

Do not use the old third-party:

```text
ESP32_BLE_Arduino
```

with the current ESP32 Arduino board package if it causes a conflict.

The ESP32 board package already provides its BLE library.

If Arduino reports:

```text
Multiple libraries were found for "BLEDevice.h"
```

and selects:

```text
Documents\Arduino\libraries\ESP32_BLE_Arduino
```

instead of the BLE library bundled with the ESP32 board package, remove or rename the old conflicting library.

---

# 16. Python dependencies

The PC Agent currently uses libraries including:

```text
bleak
pynput
pycaw
psutil
```

The system-tray version additionally uses:

```text
pystray
Pillow
```

A typical development setup is:

```text
DESKEY project
│
├── venv/
├── DESKEY_PC_Agent_BLE.py
├── DESKEY.ico
└── ...
```

Create the environment:

```bash
python -m venv venv
```

Activate it on Windows:

```bat
venv\Scripts\activate
```

Install dependencies:

```bat
python -m pip install bleak pynput pycaw psutil pystray pillow
```

PyInstaller:

```bat
python -m pip install pyinstaller
```

---

# 17. Why use a virtual environment?

A virtual environment keeps DESKEY's Python packages separate from other projects.

Instead of:

```text
Global Python
 ├── DESKEY packages
 ├── Project A packages
 ├── Project B packages
 └── ...
```

use:

```text
DESKEY
└── venv
     ├── bleak
     ├── pynput
     ├── pycaw
     ├── psutil
     ├── pystray
     ├── Pillow
     └── PyInstaller
```

This reduces dependency conflicts.

---

# 18. How the EXE is generated

The Python source is converted into a Windows executable using **PyInstaller**.

Development flow:

```text
DESKEY_PC_Agent_BLE.py
          │
          ▼
       PyInstaller
          │
          ▼
      DESKEY.exe
```

The `--onefile` option packages the application into a single executable.

Example:

```bat
python -m PyInstaller --onefile --name DESKEY DESKEY_PC_Agent_BLE.py
```

The output is:

```text
dist\
└── DESKEY.exe
```

---

# 19. Adding the DESKEY icon

The Windows executable uses:

```text
DESKEY.ico
```

Build with:

```bat
python -m PyInstaller --onefile --name DESKEY --icon=DESKEY.ico DESKEY_PC_Agent_BLE.py
```

The icon is embedded into the EXE.

The Python source file can still display a Python icon in Explorer. That is normal.

The important file is:

```text
dist\DESKEY.exe
```

---

# 20. System-tray version

DESKEY was changed from a terminal-only Python agent into a desktop-style application.

The tray version uses:

```text
pystray
Pillow
```

The application can show a DESKEY icon in the Windows notification area.

The user can:

```text
Open DESKEY
     ↓
BLE connects
     ↓
PC awareness starts
     ↓
DESKEY remains available in the tray
     ↓
Exit
     ↓
BLE disconnects
     ↓
PC agent stops
```

The application is intentionally **not automatically started with Windows**.

---

# 21. Building the current tray EXE

Because the tray needs access to the DESKEY icon at runtime, package the icon as application data too.

Use:

```bat
python -m PyInstaller --clean --onefile --noconsole --name DESKEY --icon=DESKEY.ico --add-data "DESKEY.ico;." DESKEY_PC_Agent_BLE_Tray.py
```

Important options:

| Option | Purpose |
|---|---|
| `--clean` | Clears previous build cache |
| `--onefile` | Creates one EXE |
| `--noconsole` | Hides the terminal window |
| `--name DESKEY` | Names the EXE `DESKEY.exe` |
| `--icon=DESKEY.ico` | Sets the Windows EXE icon |
| `--add-data "DESKEY.ico;."` | Makes the icon available to the tray application |

Output:

```text
dist\
└── DESKEY.exe
```

---

# 22. Development vs final build

During development, it is better to keep the console visible:

```bat
python -m PyInstaller --clean --onefile --name DESKEY --icon=DESKEY.ico --add-data "DESKEY.ico;." DESKEY_PC_Agent_BLE_Tray.py
```

Why?

If something crashes, you can see:

```text
[BLE] ...
[TRAY] ...
[ERROR] ...
```

Once everything is stable, use:

```bat
python -m PyInstaller --clean --onefile --noconsole --name DESKEY --icon=DESKEY.ico --add-data "DESKEY.ico;." DESKEY_PC_Agent_BLE_Tray.py
```

---

# 23. Testing the EXE

Before distributing DESKEY, test:

### Application

- [ ] EXE starts
- [ ] No Python installation is required
- [ ] DESKEY icon appears
- [ ] System tray icon appears
- [ ] Exit works

### BLE

- [ ] ESP32 advertises as `DESKEY`
- [ ] Python discovers it
- [ ] Python connects
- [ ] State updates reach the ESP32
- [ ] ESP32 restart is handled
- [ ] BLE disconnect/reconnect works
- [ ] Bluetooth temporarily unavailable is handled

### PC awareness

- [ ] Coding
- [ ] Browsing
- [ ] Music
- [ ] Typing
- [ ] Gaming
- [ ] Watching/video
- [ ] Idle

### DESKEY behavior

- [ ] Sleep
- [ ] Wake
- [ ] Current PC state after wake
- [ ] Music animation
- [ ] Autonomous mode
- [ ] OLED animations

---

# 24. Running DESKEY on another computer

The PyInstaller `--onefile` EXE can be copied to another compatible Windows PC.

The other PC does not normally need:

```text
Python
PyInstaller
bleak
pynput
pycaw
psutil
venv
```

The other PC does need:

- Windows
- Bluetooth LE support
- DESKEY ESP32 powered on

Basic process:

```text
Copy DESKEY.exe
      ↓
Enable Bluetooth
      ↓
Power ESP32
      ↓
Open DESKEY.exe
      ↓
BLE discovery
      ↓
Connect
```

For a proper public/distributable version, create an installer instead of distributing the raw EXE.

---

# 25. Using another ESP32 + OLED

A second ESP32 can use the same firmware if it is a compatible board and the OLED/hardware wiring matches.

The important configuration is:

```text
Device name:
DESKEY

Service UUID:
7f2d0001-6b4a-4f43-9b9a-9f5b1c2e0001

State UUID:
7f2d0002-6b4a-4f43-9b9a-9f5b1c2e0001
```

Flash the same firmware to the second ESP32.

For a simple test, turn off the first ESP32 so Python has only one `DESKEY` device to discover.

If multiple DESKEY devices are eventually required, give each device a unique identity, such as:

```text
DESKEY-01
DESKEY-02
DESKEY-03
```

and add device selection to the PC Agent.

---

# 26. Adding new functionality

The most important development rule is:

> Do not modify the EXE directly.

Always modify the Python source, test it, and rebuild the EXE.

The normal workflow is:

```text
Modify Python
     ↓
Test Python
     ↓
Fix bugs
     ↓
Build EXE
     ↓
Test EXE
     ↓
Release new version
```

For example:

```text
DESKEY v1.0
     ↓
Add calendar awareness
     ↓
Update Python
     ↓
Test
     ↓
Build
     ↓
DESKEY v1.1
```

---

# 27. Adding a new PC activity

Suppose you want:

```text
Figma → designing
```

The general process is:

```text
1. Add "designing" to valid states
2. Update the activity detector
3. Detect Figma
4. Return "designing"
5. Send it over BLE
6. Add "designing" behavior in ESP32
7. Add OLED animation
8. Test
9. Rebuild EXE
```

The ESP32 and Python must agree on the state name.

---

# 28. Adding a new ESP32 state

Suppose you add:

```text
STATE_DESIGNING
```

The ESP32 needs to understand the state and decide how DESKEY should behave.

Conceptually:

```text
Python
   │
   │ "designing"
   ▼
BLE
   │
   ▼
ESP32
   │
   ▼
STATE_DESIGNING
   │
   ▼
Designing animation
```

Python does not need to know how the OLED animation works.

---

# 29. Adding a new sensor

If you add a sensor to the ESP32:

```text
Temperature sensor
        ↓
ESP32
        ↓
Behavior engine
        ↓
OLED
```

This can usually be implemented entirely in the ESP32 firmware.

If the sensor information also needs to be sent to the PC:

```text
ESP32
  ↓
BLE characteristic
  ↓
Python
```

you may need an additional BLE characteristic/communication direction.

---

# 30. Adding a new BLE feature

Current architecture:

```text
DESKEY BLE
│
└── State Characteristic
```

If future features require more data, you can add characteristics, for example:

```text
DESKEY BLE Service
│
├── State Characteristic
├── Configuration Characteristic
├── Device Status Characteristic
└── Commands Characteristic
```

Do this carefully because both the ESP32 and Python Agent need to understand the new UUIDs and data format.

---

# 31. Adding a web dashboard

The optional Wi-Fi dashboard can expose things such as:

```text
Current state
BLE status
Heartbeat
PC idle information
Wi-Fi status
Device configuration
```

The architecture is:

```text
Python ──BLE──► ESP32
                  │
                  │ Wi-Fi
                  ▼
               Browser
```

The dashboard is a separate network layer and does not replace BLE.

---

# 32. Adding a new Python dependency

If a future feature needs another Python package:

```bat
python -m pip install package-name
```

Then update your dependency list.

For example:

```text
requirements.txt
```

could contain:

```text
bleak
pynput
pycaw
psutil
pystray
Pillow
```

Then another developer can recreate the environment with:

```bat
python -m pip install -r requirements.txt
```

After adding a dependency, rebuild the EXE.

---

# 33. Adding an application icon

Keep:

```text
DESKEY.ico
```

in the project.

Build using:

```bat
--icon=DESKEY.ico
```

If the application also needs the icon at runtime:

```bat
--add-data "DESKEY.ico;."
```

---

# 34. Recommended future project structure

As DESKEY grows, avoid keeping everything in one very large Python file.

A cleaner architecture would eventually be:

```text
DESKEY/
│
├── pc_agent/
│   ├── main.py
│   ├── activity_detector.py
│   ├── idle_detector.py
│   ├── audio_detector.py
│   ├── ble_manager.py
│   ├── state_manager.py
│   └── tray.py
│
├── assets/
│   └── DESKEY.ico
│
├── requirements.txt
├── build.bat
└── README.md
```

The ESP32 firmware can remain a separate Arduino project.

---

# 35. Building after future changes

If the Python entry point is:

```text
pc_agent/main.py
```

the build command can eventually become:

```bat
python -m PyInstaller --clean --onefile --noconsole --name DESKEY --icon=assets\DESKEY.ico --add-data "assets\DESKEY.ico;assets" pc_agent\main.py
```

Always test the Python version first.

---

# 36. What NOT to change casually

Avoid changing these without updating both sides:

```text
BLE device name
BLE service UUID
BLE state UUID
BLE packet format
```

For example:

```text
Python:
state = "coding"

ESP32:
expects "coding"
```

Changing only one side can break communication.

Similarly, changing:

```text
7f2d0001-...
```

on the ESP32 but not Python will prevent Python from finding the expected BLE service.

---

# 37. Troubleshooting guide

## EXE does not open

Build without `--noconsole`:

```bat
python -m PyInstaller --clean --onefile --name DESKEY --icon=DESKEY.ico --add-data "DESKEY.ico;." DESKEY_PC_Agent_BLE_Tray.py
```

Then run:

```bat
dist\DESKEY.exe
```

Read the error shown in the console.

---

## Tray icon does not appear

Test the Python source first:

```bat
python DESKEY_PC_Agent_BLE_Tray.py
```

Confirm that `pystray` and `Pillow` are installed in the active virtual environment.

---

## BLE does not connect

Check:

- ESP32 is powered
- ESP32 is advertising
- Windows Bluetooth is enabled
- ESP32 is within range
- Device name is `DESKEY`
- Service/characteristic UUIDs match
- No conflicting BLE implementation is being used

---

## BLE disconnects after ESP32 restart

The intended behavior is:

```text
Connection lost
      ↓
Fresh discovery
      ↓
Find DESKEY
      ↓
Reconnect
```

If it becomes stuck, test the Python version with the console visible so the BLE discovery error can be seen.

---

## Wi-Fi does not connect

Remember:

> Wi-Fi is optional for core DESKEY operation.

BLE PC awareness can continue even when Wi-Fi is unavailable.

The web dashboard will not be available through the network without Wi-Fi.

---

# 38. Release checklist

Before releasing a new DESKEY version:

```text
[ ] Test Python source
[ ] Test BLE discovery
[ ] Test BLE reconnect
[ ] Test ESP32 restart
[ ] Test PC activity detection
[ ] Test sleep/wake
[ ] Test music animation
[ ] Test autonomous mode
[ ] Test tray
[ ] Test Exit
[ ] Build EXE
[ ] Test EXE
[ ] Check EXE icon
[ ] Test on another Windows PC
[ ] Update version number
[ ] Create release package/installer
```

---

# 39. Current DESKEY architecture summary

The current system can be summarized as:

```text
                    DESKEY
                       │
          ┌────────────┴────────────┐
          │                         │
       Windows                    ESP32
          │                         │
     DESKEY.exe                 Firmware
          │                         │
     Python Agent              State Engine
          │                         │
   ┌──────┼────────┐          ┌─────┼──────┐
   │      │        │          │     │      │
 Activity Audio  Idle       OLED  BLE   Sleep/Wake
 Detection       Detection
   │
   └──────────────┐
                  │
              Bluetooth LE
                  │
                  ▼
                ESP32
```

### Core communication

```text
Windows
   ↓
Python
   ↓
BLE
   ↓
DESKEY ESP32
   ↓
OLED / Behavior
```

### Optional network layer

```text
ESP32
   ↓
Wi-Fi
   ↓
Web Dashboard
```

---

# 40. Final development principle

DESKEY is intentionally split into two responsibilities:

> **Python understands the PC. ESP32 understands DESKEY.**

Keep that separation as the project grows.

If a new feature is about:

```text
Windows applications
keyboard
mouse
audio
PC activity
Windows notifications
```

it probably belongs in the **Python Agent**.

If it is about:

```text
OLED
eyes
animations
sleep
wake
physical sensors
LEDs
buzzers
hardware behavior
```

it probably belongs in the **ESP32 firmware**.

If it is about:

```text
communication between the two
```

it belongs in the **BLE protocol layer** and should be designed on both sides.

That separation will make DESKEY much easier to maintain and extend.
