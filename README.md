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
- 📡 **Wi-Fi** - PC ↔ ESP32 communication
- 💓 **Heartbeat** - detects Python connection status
- 😴 **Sleep Engine** - reacts to PC inactivity
- 🤖 **Autonomous Mode** - operates independently when Python disconnects
- 🌐 **Web Dashboard** - monitor and control DESKEY
- 📶 **Wi-Fi Provisioning** - configure Wi-Fi without editing firmware
- 🔎 **mDNS + UDP Discovery** - find DESKEY without manually entering its IP

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
           │ HTTP / Wi-Fi
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

# 📡 Wi-Fi Provisioning

Wi-Fi credentials do not need to be hard-coded into the firmware.

On first setup, DESKEY can create:

```text
DESKEY-SETUP
```

Connect to it from a phone/computer and configure your Wi-Fi.

```text
DESKEY boots
     ↓
No saved Wi-Fi
     ↓
DESKEY-SETUP
     ↓
Open setup page
     ↓
Select Wi-Fi
     ↓
Enter password
     ↓
Save
     ↓
ESP32 reconnects
```

Credentials are stored in ESP32 non-volatile storage.

After setup:

```text
Power ON
   ↓
Load saved credentials
   ↓
Connect automatically
   ↓
DESKEY starts
```

# 🔎 Automatic Discovery

You do not need to manually find the ESP32 IP every time.

DESKEY supports:

### mDNS

The ESP32 can advertise:

```text
deskey.local
```

Python can then connect using the hostname instead of a changing DHCP address.

There is also a UDP discovery fallback.

```text
Python starts
      ↓
Try deskey.local
      ↓
Found?
   ↙      ↘
 YES      NO
  ↓        ↓
Connect   UDP discovery
           ↓
       Find DESKEY
           ↓
       Get IP address
           ↓
         Connect
```

A manual `--ip` option can remain available as a fallback.

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

When mDNS is available:

```text
http://deskey.local
```

can be used to open the dashboard.

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
                  Start mDNS
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
WiFi
WebServer
Preferences
mDNS
UDP Discovery
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

# 📁 Project Structure

```text
DESKEY/
│
├── deskey-firmware/
│   └── deskey.ino
│
├── deskey-desktop-agent/
│   └── deskey.py
│   └── requirements.txt
│
├── deskey-website/
│   ├── index.html
│   ├── styles.css
│   └── script.js
│
├── README.md
└── LICENSE
```

# 🚀 Getting Started

## 1. Flash the ESP32

Open the DESKEY firmware in Arduino IDE.

Install/configure the ESP32 board package and required libraries, then upload the firmware.

## 2. Configure Wi-Fi

On first boot, connect to:

```text
DESKEY-SETUP
```

Open the setup page and enter your Wi-Fi credentials.

## 3. Find DESKEY

Normally the Python agent can discover:

```text
deskey.local
```

automatically.

If discovery isn't available on your network, use the ESP32 IP as a fallback.

## 4. Start Python

Normally:

```bash
python DESKEY_PC_Agent.py
```

Manual fallback:

```bash
python DESKEY_PC_Agent.py --ip 192.168.x.x
```

## 5. Open the Dashboard

```text
http://deskey.local
```

or use the ESP32 IP address.

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

The PC agent analyzes activity locally and communicates with the ESP32 over the local network.

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

### DESKEY doesn't connect to Wi-Fi

Check:

- Wi-Fi credentials
- 2.4 GHz network availability
- ESP32 Wi-Fi signal
- Router/client isolation settings

Use the `DESKEY-SETUP` portal to reconfigure credentials.

### Python can't find DESKEY

Try:

```bash
python DESKEY_PC_Agent.py --ip <ESP32_IP>
```

If this works, the problem is likely local-network discovery/mDNS.

Make sure the PC and ESP32 are on the same LAN.

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
