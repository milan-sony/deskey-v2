"""
============================================================
DESKTOP BUDDY - PC AWARENESS ENGINE
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

Manual (optional IP override):

    python pc_agent.py --state coding
============================================================
"""

import argparse
import ctypes
import ctypes.wintypes
import sys
import socket
import threading
import time

import requests


# ============================================================
# CONFIGURATION
# ============================================================

IDLE_THRESHOLD_SECONDS = 120

TYPING_KPS_THRESHOLD = 3

POLL_INTERVAL = 2.0

ESP_TIMEOUT = 3

# Automatic ESP32 discovery
ESP32_HOSTNAME = "deskey.local"
DISCOVERY_PORT = 4210
DISCOVERY_MESSAGE = "DESKEY_DISCOVER"
DISCOVERY_TIMEOUT = 1.5
DISCOVERY_ATTEMPTS = 3


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
# ESP32 DISCOVERY
# ============================================================

def resolve_mdns_host():

    try:
        ip = socket.gethostbyname(ESP32_HOSTNAME)

        print(
            f"[OK] Found DESKEY via mDNS: {ESP32_HOSTNAME} -> {ip}"
        )

        return ip

    except socket.gaierror:

        print(
            f"[INFO] {ESP32_HOSTNAME} was not resolved."
        )

        return None


def discover_esp32_udp():

    message = DISCOVERY_MESSAGE.encode("utf-8")

    for attempt in range(1, DISCOVERY_ATTEMPTS + 1):

        sock = socket.socket(
            socket.AF_INET,
            socket.SOCK_DGRAM
        )

        try:

            sock.setsockopt(
                socket.SOL_SOCKET,
                socket.SO_BROADCAST,
                1
            )

            sock.settimeout(
                DISCOVERY_TIMEOUT
            )

            sock.sendto(
                message,
                ("255.255.255.255", DISCOVERY_PORT)
            )

            deadline = (
                time.time() +
                DISCOVERY_TIMEOUT
            )

            while time.time() < deadline:

                try:
                    data, address = sock.recvfrom(128)

                except socket.timeout:
                    break

                response = (
                    data.decode(
                        "utf-8",
                        errors="ignore"
                    ).strip()
                )

                if response.startswith("DESKEY|"):

                    advertised_ip = (
                        response.split("|", 1)[1].strip()
                    )

                    ip = advertised_ip or address[0]

                    print(
                        f"[OK] Found DESKEY via UDP discovery: {ip}"
                    )

                    return ip

        except OSError as error:

            print(
                f"[WARN] UDP discovery attempt {attempt} failed: {error}"
            )

        finally:
            sock.close()

    return None


def discover_esp32():

    print(
        "[INFO] Searching for DESKEY automatically..."
    )

    ip = resolve_mdns_host()

    if ip:
        return ip

    return discover_esp32_udp()


# ============================================================
# ESP32 CLIENT
# ============================================================

class ESP32Client:

    def __init__(
        self,
        ip
    ):

        self.ip = ip

        self.base_url = (
            f"http://{ip}"
        )


    def send_state(
        self,
        state,
        idle_seconds=0
    ):
        url = (
            f"{self.base_url}/state"
        )

        payload = {
            "state": state,
            "idle_seconds": round(
                float(idle_seconds),
                1
            )
        }

        try:
            response = requests.post(
                url,
                json=payload,
                timeout=ESP_TIMEOUT
            )

            if (
                response.status_code ==
                200
            ):
                return True

            print(
                "[WARN] ESP32 returned:",
                response.status_code
            )

            return False

        except requests.exceptions.ConnectionError:
            print(
                "[ERROR] Cannot connect to ESP32"
            )
            return False

        except requests.exceptions.Timeout:
            print(
                "[ERROR] ESP32 request timed out"
            )
            return False

        except Exception as error:
            print(
                "[ERROR]",
                error
            )
            return False


    def status(self):

        try:

            response = requests.get(

                f"{self.base_url}/status",

                timeout=ESP_TIMEOUT
            )


            if (
                response.status_code ==
                200
            ):

                return response.json()


        except Exception:

            pass


        return None


# ============================================================
# TERMINAL UI
# ============================================================

def print_banner():

    print()

    print("DESKEY - DESKTOP BUDDY - PC AWARENESS ENGINE")

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
# MAIN
# ============================================================

def main():

    parser = argparse.ArgumentParser(

        description=
        "Desktop Buddy PC Awareness Agent"
    )


    parser.add_argument(

        "--ip",

        default=None,

        help=
        "ESP32 IP address (optional; DESKEY is discovered automatically if omitted)"
    )


    parser.add_argument(

        "--state",

        default=None,

        choices=VALID_STATES,

        help=
        "Manually send a state"
    )


    parser.add_argument(

        "--interval",

        type=float,

        default=POLL_INTERVAL,

        help=
        "Detection interval"
    )


    args = parser.parse_args()


    print_banner()


    # ------------------------------------------------------------
    # ESP32 connection
    # ------------------------------------------------------------

    if args.ip:

        esp_ip = args.ip

        print(
            f"[INFO] Using manually supplied ESP32 IP: {esp_ip}"
        )

    else:

        esp_ip = discover_esp32()

        if not esp_ip:

            print()
            print(
                "[ERROR] Could not find DESKEY automatically."
            )
            print(
                "[INFO] Make sure the PC and DESKEY are on the same Wi-Fi network."
            )
            print(
                "[INFO] You can also specify the IP manually with --ip."
            )
            sys.exit(1)

    print(
        f"ESP32: {esp_ip}"
    )

    client = ESP32Client(
        esp_ip
    )


    # ========================================================
    # MANUAL MODE
    # ========================================================

    if args.state:

        print(
            f"Sending: {args.state}"
        )


        success = (
            client.send_state(
                args.state,
                0
            )
        )


        if success:

            print(
                "[OK] State sent"
            )

        else:

            print(
                "[ERROR] Failed"
            )


        return


    # ========================================================
    # AUTOMATIC MODE
    # ========================================================

    detector = (
        ContextDetector()
    )


    detector.start()


    print()

    print(
        "[OK] PC awareness started"
    )

    print(
        "[INFO] Detecting:"
    )

    print(
        "       • Audio"
    )

    print(
        "       • Typing"
    )

    print(
        "       • Idle"
    )

    print(
        "       • Coding"
    )

    print(
        "       • Gaming"
    )

    print(
        "       • Browsing"
    )

    print(
        "       • Video"
    )

    print()

    print(
        "Press Ctrl+C to stop."
    )

    print()


    last_state = None

    connection_errors = 0


    try:
        while True:

            state = (
                detector.detect()
            )

            # Get the actual current Windows input idle time.
            idle_seconds = (
                get_idle_seconds()
            )

            # ------------------------------------------------
            # Print only when the detected context changes.
            # ------------------------------------------------

            if state != last_state:

                timestamp = time.strftime(
                    "%H:%M:%S"
                )

                icon = (
                    STATE_ICONS.get(
                        state,
                        "❓"
                    )
                )

                print(
                    f"[{timestamp}] "
                    f"{icon} "
                    f"{state} "
                    f"(idle={idle_seconds:.1f}s)"
                )

            # ------------------------------------------------
            # IMPORTANT:
            #
            # Always send the current state and idle time.
            #
            # This is what allows the ESP32 to receive the
            # CURRENT action after the user wakes the PC,
            # instead of restoring an old action.
            # ------------------------------------------------

            success = (
                client.send_state(
                    state,
                    idle_seconds
                )
            )

            if success:

                last_state = state

                connection_errors = 0

            else:

                connection_errors += 1

                if (
                    connection_errors >= 10
                ):

                    print(
                        "[ERROR] Too many "
                        "connection failures."
                    )

                    sys.exit(1)

            time.sleep(
                args.interval
            )


    except KeyboardInterrupt:

        print()

        print(
            "Desktop Buddy agent stopped."
        )


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":

    main()