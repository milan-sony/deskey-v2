
"""
============================================================
DESKTOP BUDDY PC AWARENESS ENGINE
============================================================

Detects:

    - Music / audio
    - Keyboard activity
    - Mouse / keyboard idle
    - Foreground application
    - Browser
    - Coding
    - Gaming
    - YouTube
    - Video
    - General browsing
    - Idle

Sends contextual states to ESP32.

Usage:

    python pc_agent.py

Manual:

    python pc_agent.py --state coding
============================================================
"""

import argparse
import asyncio
import ctypes
import ctypes.wintypes
import sys
import threading
import time
from pathlib import Path

try:
    import pystray
    from PIL import Image
except ImportError:
    pystray = None
    Image = None

from bleak import BleakClient, BleakScanner


# ============================================================
# CONFIGURATION
# ============================================================

IDLE_THRESHOLD_SECONDS = 120

TYPING_KPS_THRESHOLD = 3

POLL_INTERVAL = 2.0

ESP_TIMEOUT = 3


# ============================================================
# STATES
# ============================================================

VALID_STATES = [

    "relaxed",

    "happy",
    "sad",
    "angry",
    "sleepy",
    "thinking",
    "curious",
    "excited",
    "surprised",
    "confused",
    "scared",
    "laughing",

    "music",
    "typing",
    "browsing",
    "idle",
    "gaming",
    "error",
    "watching",

    "coding",
]


# ============================================================
# WINDOWS IDLE DETECTION
# ============================================================

class LASTINPUTINFO(ctypes.Structure):

    _fields_ = [
        (
            "cbSize",
            ctypes.c_uint
        ),

        (
            "dwTime",
            ctypes.c_uint
        ),
    ]


def get_idle_seconds():

    try:

        info = LASTINPUTINFO()

        info.cbSize = ctypes.sizeof(
            LASTINPUTINFO
        )

        ctypes.windll.user32.GetLastInputInfo(
            ctypes.byref(info)
        )

        millis = (
            ctypes.windll.kernel32.GetTickCount()
            - info.dwTime
        )

        return millis / 1000.0

    except Exception:

        return 0


# ============================================================
# KEYBOARD MONITOR
# ============================================================

class KeyboardMonitor:

    def __init__(self):

        self.key_count = 0

        self.keys_per_second = 0.0

        self.lock = threading.Lock()

        self.listener = None


    def start(self):

        try:

            from pynput import keyboard

        except ImportError:

            print(
                "[WARN] pynput not installed."
            )

            return


        def on_press(key):

            with self.lock:

                self.key_count += 1


        self.listener = (
            keyboard.Listener(
                on_press=on_press
            )
        )

        self.listener.daemon = True

        self.listener.start()


        thread = threading.Thread(
            target=self._calculate_kps,
            daemon=True
        )

        thread.start()


        print(
            "[OK] Keyboard monitor started"
        )


    def _calculate_kps(self):

        while True:

            time.sleep(1)

            with self.lock:

                self.keys_per_second = (
                    self.key_count
                )

                self.key_count = 0


    def get_kps(self):

        with self.lock:

            return self.keys_per_second


# ============================================================
# AUDIO DETECTION
# ============================================================

def is_audio_playing():

    try:

        from pycaw.pycaw import (
            AudioUtilities,
            IAudioMeterInformation
        )

        sessions = (
            AudioUtilities.GetAllSessions()
        )


        for session in sessions:

            if not session.Process:

                continue


            try:

                meter = (
                    session._ctl.QueryInterface(
                        IAudioMeterInformation
                    )
                )

                peak = meter.GetPeakValue()


                if peak > 0.01:

                    return True


            except Exception:

                continue


        return False


    except ImportError:

        print(
            "[WARN] pycaw not installed."
        )

        return False


    except Exception:

        return False


# ============================================================
# FOREGROUND WINDOW
# ============================================================

def get_foreground_window():

    try:

        user32 = ctypes.windll.user32

        hwnd = (
            user32.GetForegroundWindow()
        )

        if not hwnd:

            return ""


        length = (
            user32.GetWindowTextLengthW(
                hwnd
            )
        )


        buffer = ctypes.create_unicode_buffer(
            length + 1
        )


        user32.GetWindowTextW(
            hwnd,
            buffer,
            length + 1
        )


        return buffer.value


    except Exception:

        return ""


# ============================================================
# ACTIVE PROCESS
# ============================================================

def get_foreground_process():

    try:

        import psutil


        user32 = ctypes.windll.user32

        hwnd = (
            user32.GetForegroundWindow()
        )


        if not hwnd:

            return None


        pid = ctypes.c_ulong()

        user32.GetWindowThreadProcessId(
            hwnd,
            ctypes.byref(pid)
        )


        process_id = pid.value


        if not process_id:

            return None


        process = (
            psutil.Process(
                process_id
            )
        )


        return process.name().lower()


    except Exception:

        return None


# ============================================================
# APPLICATION CATEGORIES
# ============================================================

CODING_APPS = {

    "code.exe",
    "code-insiders.exe",

    "devenv.exe",

    "idea64.exe",
    "pycharm64.exe",

    "webstorm64.exe",

    "clion64.exe",

    "androidstudio.exe",

    "sublime_text.exe",

    "notepad++.exe",

    "cursor.exe",

    "windsurf.exe",
}


GAMING_APPS = {

    "steam.exe",

    "steamwebhelper.exe",

    "epicgameslauncher.exe",

    "riotclientservices.exe",

    "valorant.exe",

    "cs2.exe",

    "eldenring.exe",

    "minecraft.exe",
}


BROWSER_APPS = {

    "chrome.exe",

    "msedge.exe",

    "firefox.exe",

    "brave.exe",

    "opera.exe",

    "vivaldi.exe",
}


VIDEO_APPS = {

    "vlc.exe",

    "potplayer.exe",

    "mpc-hc64.exe",

    "wmplayer.exe",
}


# ============================================================
# WINDOW KEYWORDS
# ============================================================

CODING_KEYWORDS = [

    "visual studio code",

    "cursor",

    "pycharm",

    "intellij",

    "webstorm",

    "android studio",

    "visual studio",

    "sublime",
]


VIDEO_KEYWORDS = [

    "youtube",

    "netflix",

    "prime video",

    "disney+",

    "twitch",

    "video",
]


# ============================================================
# CLASSIFY APPLICATION
# ============================================================

def classify_application(
    process_name,
    window_title
):

    process = (
        process_name or ""
    ).lower()


    title = (
        window_title or ""
    ).lower()


    # --------------------------------------------------------
    # Gaming
    # --------------------------------------------------------

    if process in GAMING_APPS:

        return "gaming"


    # --------------------------------------------------------
    # Coding
    # --------------------------------------------------------

    if process in CODING_APPS:

        return "coding"


    for keyword in CODING_KEYWORDS:

        if keyword in title:

            return "coding"


    # --------------------------------------------------------
    # Video players
    # --------------------------------------------------------

    if process in VIDEO_APPS:

        return "watching"


    # --------------------------------------------------------
    # Browser
    # --------------------------------------------------------

    if process in BROWSER_APPS:

        for keyword in VIDEO_KEYWORDS:

            if keyword in title:

                return "watching"


        return "browsing"


    return "browsing"


# ============================================================
# CONTEXT DETECTOR
# ============================================================

class ContextDetector:

    def __init__(self):

        self.keyboard = KeyboardMonitor()

        self.last_context = None

        self.context_since = time.time()


    def start(self):

        self.keyboard.start()


    def detect(self):

        # ----------------------------------------------------
        # Idle
        # ----------------------------------------------------

        idle_seconds = (
            get_idle_seconds()
        )

        # Keep the existing idle behavior unchanged.
        if (
            idle_seconds >
            IDLE_THRESHOLD_SECONDS
        ):
            return "idle"

        # ----------------------------------------------------
        # Foreground application
        #
        # The foreground app is checked BEFORE background
        # audio. This prevents Spotify/music playing in the
        # background from overriding VS Code, a browser, game,
        # etc.
        # ----------------------------------------------------

        process = (
            get_foreground_process()
        )

        title = (
            get_foreground_window()
        )

        app_state = (
            classify_application(
                process,
                title
            )
        )

        # ----------------------------------------------------
        # Typing
        # ----------------------------------------------------

        kps = (
            self.keyboard.get_kps()
        )

        if (
            kps >=
            TYPING_KPS_THRESHOLD
        ):
            if app_state == "coding":
                return "coding"

            return "typing"

        # ----------------------------------------------------
        # Foreground app priority
        #
        # Any useful foreground context wins over background
        # audio.
        #
        # Examples:
        #   Spotify + VS Code       -> coding
        #   Spotify + Chrome        -> browsing/watching
        #   Game + Spotify          -> gaming
        # ----------------------------------------------------

        if app_state != "browsing":
            return app_state

        # Browser state is already classified by
        # classify_application(), including video keywords.
        if process in BROWSER_APPS:
            return app_state

        # ----------------------------------------------------
        # Audio fallback
        #
        # Only use music when no useful foreground context
        # was detected.
        # ----------------------------------------------------

        if is_audio_playing():
            return "music"

        return app_state



# ============================================================
# ESP32 BLE CLIENT
# ============================================================

BLE_DEVICE_NAME = "DESKEY"

BLE_SERVICE_UUID = "7f2d0001-6b4a-4f43-9b9a-9f5b1c2e0001"
BLE_STATE_UUID = "7f2d0002-6b4a-4f43-9b9a-9f5b1c2e0001"

BLE_SCAN_TIMEOUT = 6.0
BLE_CONNECT_TIMEOUT = 8.0


class ESP32BLEClient:

    def __init__(self, device_name=BLE_DEVICE_NAME):
        self.device_name = device_name
        self.client = None
        self.address = None


    async def discover(self):

        print(
            f"[BLE] Scanning for {self.device_name}..."
        )

        devices = await BleakScanner.discover(
            timeout=BLE_SCAN_TIMEOUT
        )

        for device in devices:

            name = device.name or ""

            if name == self.device_name:

                self.address = device.address

                print(
                    f"[BLE] Found {self.device_name}: "
                    f"{self.address}"
                )

                return True

        print(
            f"[ERROR] BLE device '{self.device_name}' not found."
        )

        return False


    async def connect(self):

        if not self.address:

            found = await self.discover()

            if not found:
                return False

        try:

            if self.client and self.client.is_connected:
                return True

            self.client = BleakClient(
                self.address,
                timeout=BLE_CONNECT_TIMEOUT
            )

            await self.client.connect()

            print(
                f"[BLE] Connected to {self.device_name}."
            )

            return True

        except Exception as error:

            print(
                f"[BLE] Connection failed: {error}"
            )

            self.client = None

            return False


    async def disconnect(self):

        if self.client:

            try:
                await self.client.disconnect()
            except Exception:
                pass

        self.client = None


    async def send_state(self, state, idle_seconds=0):

        if not self.client or not self.client.is_connected:

            connected = await self.connect()

            if not connected:
                return False

        payload = (
            '{"state":"'
            + state
            + '","idle_seconds":'
            + str(round(float(idle_seconds), 1))
            + '}'
        )

        try:

            await self.client.write_gatt_char(
                BLE_STATE_UUID,
                payload.encode("utf-8"),
                response=False
            )

            return True

        except Exception as error:

            print(
                f"[BLE] Send failed: {error}"
            )

            await self.disconnect()

            return False


# ============================================================
# TERMINAL UI
# ============================================================

def print_banner():

    print()

    print(
        "DESKTOP BUDDY - PC AWARENESS ENGINE"
    )

    print()


# ============================================================
# STATE ICON
# ============================================================

STATE_ICONS = {

    "coding": "💻",

    "music": "🎵",

    "typing": "⌨️",

    "browsing": "🌐",

    "gaming": "🎮",

    "watching": "📺",

    "idle": "😴",

    "happy": "😄",

    "sad": "😢",

    "angry": "😡",

    "sleepy": "😴",

    "thinking": "🤔",

    "curious": "👀",

    "excited": "🤩",

    "surprised": "😲",

    "confused": "😵",

    "scared": "😨",

    "laughing": "😂",

    "relaxed": "😌",

    "error": "❌",
}


# ============================================================
# SYSTEM TRAY
# ============================================================

shutdown_event = threading.Event()
tray_icon = None

def resource_path(filename):
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS) / filename
    return Path(__file__).resolve().parent / filename

def create_tray_image():
    if Image is None:
        return None
    path = resource_path("DESKEY.ico")
    if path.exists():
        try:
            return Image.open(path).convert("RGBA")
        except Exception:
            pass
    return Image.new("RGBA", (64, 64), (20, 20, 24, 255))

def tray_exit(icon, item):
    print("[TRAY] Exit requested.")
    shutdown_event.set()
    icon.stop()

def tray_reconnect(icon, item):
    print("[TRAY] Reconnect requested. The BLE loop will rediscover DESKEY.")

def run_tray():
    global tray_icon
    if pystray is None or Image is None:
        print("[TRAY] pystray/Pillow not installed; tray disabled.")
        return
    try:
        tray_icon = pystray.Icon(
            "DESKEY",
            create_tray_image(),
            "DESKEY",
            pystray.Menu(
                pystray.MenuItem("DESKEY", lambda icon, item: None, enabled=False),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Reconnect", tray_reconnect),
                pystray.MenuItem("Exit", tray_exit),
            ),
        )
        print("[TRAY] DESKEY tray started.")
        tray_icon.run()
    except Exception as exc:
        print(f"[TRAY] Failed to start: {exc}")

def start_tray():
    thread = threading.Thread(target=run_tray, name="DESKEY-Tray", daemon=True)
    thread.start()
    return thread


# ============================================================
# MAIN
# ============================================================

async def main_async():

    start_tray()

    parser = argparse.ArgumentParser(
        description=
        "DESKEY PC Awareness Agent - Bluetooth BLE"
    )

    parser.add_argument(
        "--state",
        default=None,
        choices=VALID_STATES,
        help="Manually send a state"
    )

    parser.add_argument(
        "--interval",
        type=float,
        default=POLL_INTERVAL,
        help="Detection interval"
    )

    parser.add_argument(
        "--device",
        default=BLE_DEVICE_NAME,
        help="BLE device name (default: DESKEY)"
    )

    args = parser.parse_args()

    print_banner()

    print(
        f"BLE device: {args.device}"
    )

    client = ESP32BLEClient(
        args.device
    )

    # ========================================================
    # CONNECT
    # ========================================================

    if not await client.connect():

        print(
            "[ERROR] Could not connect to DESKEY over Bluetooth."
        )

        return 1


    # ========================================================
    # MANUAL MODE
    # ========================================================

    if args.state:

        print(
            f"Sending: {args.state}"
        )

        success = await client.send_state(
            args.state,
            0
        )

        if success:
            print("[OK] State sent")
            return 0

        print("[ERROR] Failed")
        return 1


    # ========================================================
    # AUTOMATIC MODE
    # ========================================================

    detector = ContextDetector()
    detector.start()

    print()
    print("[OK] PC awareness started")
    print("[OK] Bluetooth transport active")
    print("[INFO] Detecting:")
    print("       • Audio")
    print("       • Typing")
    print("       • Idle")
    print("       • Coding")
    print("       • Gaming")
    print("       • Browsing")
    print("       • Video")
    print()
    print("Use the DESKEY tray icon → Exit to stop.")
    print()

    last_state = None
    connection_errors = 0

    try:

        while not shutdown_event.is_set():

            state = detector.detect()

            # Get the actual current Windows input idle time.
            idle_seconds = get_idle_seconds()

            # Print only when context changes.
            if state != last_state:

                timestamp = time.strftime("%H:%M:%S")

                icon = STATE_ICONS.get(
                    state,
                    "❓"
                )

                print(
                    f"[{timestamp}] "
                    f"{icon} "
                    f"{state} "
                    f"(idle={idle_seconds:.1f}s)"
                )

            # IMPORTANT:
            # Always send the CURRENT state and idle time.
            # This preserves current-action wake behavior.
            success = await client.send_state(
                state,
                idle_seconds
            )

            if success:

                last_state = state
                connection_errors = 0

            else:

                connection_errors += 1

                print(
                    f"[BLE] Reconnect attempt "
                    f"{connection_errors}"
                )

                await asyncio.sleep(1)

            await asyncio.sleep(
                args.interval
            )


    except KeyboardInterrupt:

        print()
        print("DESKEY Bluetooth agent stopped.")

    finally:

        await client.disconnect()

        if tray_icon is not None:
            try:
                tray_icon.stop()
            except Exception:
                pass

    return 0


def main():
    try:
        return asyncio.run(main_async())
    except Exception as exc:
        print(f"[DESKEY] Fatal error: {exc}")
        raise


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":

    main()
