# DESKEY

> **Your desktop. With a personality.** 👀

DESKEY is a PC-aware physical desktop companion built around an **ESP32**, **OLED display**, and **Windows Python agent**.

Instead of being another application hidden on your screen, DESKEY lives on your desk and reacts to your computer activity through expressive animated eyes.

When you're coding, DESKEY codes with you. When you're browsing, it browses with you. When you're listening to music, it reacts to music. When you walk away, it gets sleepy. When you return, it wakes and displays what you're doing **right now**.

## ✨ What is DESKEY?

DESKEY combines:

- 🧠 **ESP32** - device brain and behavior engine
- 👀 **OLED + RoboEyes** - expressive animated face
- 🐍 **Python PC Agent** - Windows activity/context detection
- 📶 **Bluetooth LE** - primary PC ↔ ESP32 communication
- 💓 **Heartbeat** - detects Python connection status
- 😴 **Sleep Engine** - reacts to PC inactivity
- 🤖 **Autonomous Mode** - operates independently when Python disconnects
- 🌐 **Optional Wi-Fi Dashboard** - web dashboard when Wi-Fi is enabled

The goal is simple: **make the desktop feel a little more alive.**

## 🧩 Architecture

```text
Windows PC
    │
    │ Python detects
    ▼
┌──────────────────────┐
│    Python Agent      │
│                      │
│ Foreground App       │
│ Keyboard Activity    │
│ Audio                │
│ PC Idle Time         │
└──────────┬───────────┘
           │ Bluetooth LE
           ▼
┌──────────────────────┐
│        ESP32         │
│                      │
│ State Engine         │
│ Sleep / Wake         │
│ Heartbeat            │
│ Autonomous Mode      │
│ Web Dashboard        │
│ Wi-Fi Manager        │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│     OLED + RoboEyes  │
└──────────────────────┘
```

### Responsibilities

**Python asks:** *"What is happening on the PC right now?"*

**ESP32 asks:** *"How should DESKEY behave?"*

This separation keeps the system modular.

# 🖥️ PC Activity Detection

DESKEY can recognize contexts such as:

| Activity | Examples |
|---|---|
| 💻 Coding | VS Code, Cursor, JetBrains IDEs |
| 🌐 Browsing | Chrome, Edge, Firefox, Brave |
| 🎵 Music | Audio playback |
| 🎮 Gaming | Recognized games |
| 📺 Watching | YouTube, Netflix, Twitch, video players |
| ⌨️ Typing | Active keyboard interaction |
| 😴 Idle | Extended mouse/keyboard inactivity |

### Activity priority

Foreground context has priority over background audio.

For example:

```text
Spotify playing
       +
VS Code is foreground
       ↓
💻 CODING
```

rather than incorrectly showing Music just because Spotify is playing.

# 😴 Smart Inactivity

The ESP32 uses the reported PC idle time to progressively change DESKEY's behavior:

```text
PC Active
    │
    ▼
Normal Activity
    │
    │ 30 seconds
    ▼
😴 SLEEPY
    │
    │ 2 minutes
    ▼
💤 SLEEPING
    │
    │ 5 minutes
    ▼
🌙 DEEP SLEEP
```

> **Note:** `DEEP_SLEEP` is currently a visual DESKEY state, not actual ESP32 hardware `esp_deep_sleep()`. The ESP32 remains active so networking, heartbeat, dashboard, and wake behavior continue working.

## 😂 DESKEY has a personality

> **Warning: DESKEY is extremely lazy. 😂** If your PC sits quietly for too long, he'll take that as an invitation for a nap. 💤

No keyboard? No mouse? No activity?

**DESKEY:** *"Sounds like a perfect time for a nap."* 😴

Just move your mouse or press a key and he'll wake up like nothing happened:

**"I wasn't sleeping. I was… conserving energy."** 👀

Then he'll check your current activity and get back to work.

# ☀️ Context-Aware Wake

DESKEY does **not** restore an old PC state after waking.

Example:

```text
Before sleep:
🎵 Music

DESKEY sleeps...

You open VS Code.

You move the mouse.
      ↓
🌙 Deep Sleep
      ↓
☀️ Wake
      ↓
😄 Happy
      ↓
💻 Coding
```

It does **not** restore:

```text
🎵 Music ❌
```

Python continuously reports the current PC context so DESKEY can react to what you're doing **now**.

# 💓 Python Heartbeat

The Python agent periodically communicates with the ESP32.

The ESP32 measures the age of the last communication.

For example:

```text
Heartbeat: 800 ms
```

means Python communicated roughly **0.8 seconds ago**.

The current heartbeat timeout is:

```text
15 seconds
```

If Python stops communicating:

```text
Python running
      ↓
Heartbeat updates
      ↓
Python closes / crashes
      ↓
Heartbeat stops
      ↓
15 seconds
      ↓
🤖 Autonomous Mode
```

# 🤖 Autonomous Mode

DESKEY can continue operating without Python.

After the heartbeat timeout, the ESP32 can switch to:

```text
AUTONOMOUS MODE
```

In autonomous mode, DESKEY can generate its own personality/emotion behavior.

The current autonomous event interval is randomized between:

```text
12 – 30 seconds
```

# 📡 Optional Wi-Fi

Wi-Fi is **not required for DESKEY's core functionality**.

Bluetooth Low Energy handles the PC ↔ ESP32 communication. Wi-Fi is optional and is used for the ESP32 web dashboard.

```text
DESKEY boots
     ↓
BLE starts
     ↓
Python discovers DESKEY
     ↓
Core DESKEY operation
     │
     └── Optional Wi-Fi
             ↓
       Web Dashboard
```

If Wi-Fi is configured, the ESP32 can provide its dashboard on the local network.

# 🌐 Web Dashboard

DESKEY includes a web dashboard hosted directly by the ESP32.

It can expose:

- Current State
- PC State
- Mode
- Python Connection
- Wi-Fi Connection
- IP Address
- Heartbeat Age
- PC Idle Time
- Uptime
- Free Heap
- Wi-Fi RSSI
- Sleep Status

When Wi-Fi is enabled, the dashboard can be opened using the ESP32's local network address.

# 👀 Expressions

DESKEY supports expressive states such as:

```text
RELAXED
HAPPY
SAD
ANGRY
SLEEPY
THINKING
CURIOUS
EXCITED
SURPRISED
CONFUSED
SCARED
LAUGHING
MUSIC
TYPING
CODING
BROWSING
IDLE
GAMING
WATCHING
SLEEPING
DEEP_SLEEP
```

The exact visual behavior is controlled by the RoboEyes configuration in the ESP32 firmware.

# 🔄 Runtime Flow

```text
                     POWER ON
                         │
                         ▼
                  ESP32 initializes
                         │
                         ▼
                   Connect Wi-Fi
                         │
                         ▼
                  Start DESKEY
                         │
                         ▼
                  Start dashboard
                         │
                         ▼
                 Python starts
                         │
                         ▼
              Discover DESKEY
                         │
                         ▼
              Detect PC activity
                         │
                         ▼
              Send state + idle time
                         │
                         ▼
                      ESP32
                         │
                         ▼
                 OLED animation
                         │
              ┌──────────┴──────────┐
              │                     │
           Active                  Idle
                                    │
                              30s → 😴
                              2m  → 💤
                              5m  → 🌙
                                    │
                              User activity
                                    │
                                    ▼
                              ☀️ Wake
                                    │
                                    ▼
                                😄 Happy
                                    │
                                    ▼
                         Current PC context
```

# 🛠️ Hardware

Core hardware:

- ESP32 development board
- SSD1306 OLED display
- I²C connection
- USB power
- Optional enclosure/case
- Optional future sensors/peripherals

# 💻 Software Stack

### Firmware

```text
C++
Arduino Framework
ESP32
Adafruit GFX
Adafruit SSD1306
RoboEyes
Bluetooth LE
WebServer
Preferences

```

### PC Agent

```text
Python
Requests
Windows APIs
Foreground Window Detection
Keyboard Activity Detection
Audio Detection
```

### Website

```text
HTML
CSS
JavaScript
```

# 🚀 Getting Started

## 1. Flash the ESP32

Open the DESKEY firmware in Arduino IDE.

Install/configure the ESP32 board package and required libraries, then upload the firmware.

```text
Arduino IDE
│
├── ESP32 board package 3.3.11
│   ├── WiFi
│   ├── WebServer
│   ├── Preferences
│   ├── BLE
│   └── Wire
│
└── Libraries
    ├── Adafruit GFX Library
    ├── Adafruit SSD1306
    └── RoboEyes
```

## 2. Start the Python Agent

Run:

```bash
python DESKEY_PC_Agent_BLE.py
```

The agent discovers **DESKEY over Bluetooth Low Energy** and connects automatically.

## 3. Use DESKEY

Once connected, Python detects the current PC activity and sends the current context to the ESP32.

```text
PC activity
    ↓
Python Agent
    ↓
Bluetooth LE
    ↓
ESP32
    ↓
OLED animation
```

## 4. Optional: Enable Wi-Fi Dashboard

Wi-Fi can be configured if you want to use the ESP32 web dashboard. It is not required for PC awareness, BLE communication, animations, sleep/wake or autonomous behavior.

# ⚙️ Main Configuration

Current inactivity thresholds:

```cpp
PC_SLEEPY_TIME = 30
PC_SLEEP_TIME = 120
PC_DEEP_SLEEP_TIME = 300
```

Heartbeat timeout:

```cpp
HEARTBEAT_TIMEOUT = 15000
```

Autonomous event interval:

```text
12 – 30 seconds
```

Python polling interval:

```text
2 seconds
```

These values can be adjusted to change DESKEY's personality.

# 🔐 Privacy

DESKEY is designed around local PC-to-device communication.

The PC agent analyzes activity locally and communicates with the ESP32 over Bluetooth Low Energy.

The core project does not require a cloud service.

# 💡 Future Possibilities

Potential future features include:

- 🎙️ Voice interaction
- 🤖 Local AI assistant
- 🧠 Long-term personality and memory
- 📱 Mobile companion app
- 🔔 Desktop notifications
- 📅 Calendar awareness
- 🎧 Advanced media detection
- 🏷️ NFC support
- 🌡️ Environmental sensors
- 💡 RGB lighting
- 🔊 Speaker/audio reactions
- 💤 Actual ESP32 hardware deep sleep
- 👋 Presence/proximity detection
- 📊 Activity analytics
- 🔌 More PC automation
- 🧩 Plugin system
- 🌐 Remote dashboard
- 🧠 LLM-powered contextual emotions

# 🧪 Development Philosophy

DESKEY is intentionally modular:

```text
PC Awareness
     ↓
Context
     ↓
State
     ↓
Behavior
     ↓
Expression
```

This makes it possible to improve one layer without rewriting the entire system.

For example, a future AI model could replace the current activity classifier while the ESP32 sleep and animation system remains unchanged.

# 🐛 Troubleshooting

### Python can't find DESKEY

Check that:

- Bluetooth is enabled on Windows
- DESKEY is powered on
- DESKEY is advertising over BLE
- The ESP32 is within Bluetooth range

Python will retry discovery automatically after a lost connection.

Wi-Fi and an ESP32 IP address are not required for core DESKEY communication.

### DESKEY stays in Music

Use the updated Python context detector.

Foreground applications should take priority over background audio:

```text
Spotify playing
+
VS Code foreground
        ↓
Coding
```

### DESKEY doesn't wake into the current activity

Make sure the Python agent sends the current state continuously rather than only when the state changes.

The intended flow is:

```text
Sleep
 ↓
User activity
 ↓
Python detects current state
 ↓
ESP32 wakes
 ↓
Wake animation
 ↓
Current PC state
```

# 📜 License

```text
MIT License
```

# ❤️ Why DESKEY?

Most desktop software lives inside the screen.

DESKEY lives **beside it**.

It doesn't need to replace your computer, automate everything, or become another notification panel.

It is deliberately small:

```text
You work.
   ↓
DESKEY notices.
   ↓
DESKEY reacts.
   ↓
You walk away.
   ↓
DESKEY sleeps.
   ↓
You return.
   ↓
DESKEY wakes.
```

A tiny physical companion for the machine you spend your day with.
