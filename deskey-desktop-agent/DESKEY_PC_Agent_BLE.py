
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

# IMPORTANT:
# Bleak's Windows/WinRT backend needs the asyncio thread to be MTA.
# Some Windows/COM packages used by PC activity/audio detection can
# initialize the thread as STA. Set this BEFORE importing packages
# that may initialize COM.
sys.coinit_flags = 0  # 0 = MTA

import threading
import time

from bleak import BleakClient, BleakScanner

# Bleak provides this helper for undoing an unwanted STA initialization.
# We use it defensively immediately before starting BLE.
try:
    from bleak.backends.winrt.util import uninitialize_sta
except ImportError:
    uninitialize_sta = None


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

BLE_SCAN_TIMEOUT = 5.0
BLE_CONNECT_TIMEOUT = 5.0
BLE_RETRY_DELAY = 2.0


class ESP32BLEClient:
    """
    BLE transport for DESKEY.

    Important design rule:
    BLE reconnects are performed by a dedicated worker task.
    The PC activity loop never waits for BLE discovery/connection.

    Therefore a Windows/Bleak connection operation that becomes
    slow or stuck cannot freeze activity detection.
    """

    def __init__(self, device_name=BLE_DEVICE_NAME):
        self.device_name = device_name

        self.device = None
        self.client = None

        self.connected = False
        self.last_error = None

        self._stop_event = asyncio.Event()
        self._connection_event = asyncio.Event()
        self._worker_task = None
        self._delivery_task = None

        # Latest state waiting to be delivered.
        self._latest_state = None
        self._state_lock = asyncio.Lock()

        # Prevent multiple GATT writes at once.
        self._write_lock = asyncio.Lock()


    def is_connected(self):
        return (
            self.client is not None
            and self.client.is_connected
        )


    async def start(self):
        self._stop_event.clear()

        if self._worker_task is None:
            self._worker_task = asyncio.create_task(
                self._connection_worker()
            )

        if self._delivery_task is None:
            self._delivery_task = asyncio.create_task(
                self._deliver_latest_state_loop()
            )


    async def stop(self):
        self._stop_event.set()

        for task_name in ("_worker_task", "_delivery_task"):

            task = getattr(self, task_name)

            if task:

                task.cancel()

                try:
                    await task
                except BaseException:
                    pass

                setattr(self, task_name, None)

        await self._disconnect()


    async def _disconnect(self):
        client = self.client

        self.client = None
        self.connected = False
        self._connection_event.clear()

        if client:
            try:
                await asyncio.wait_for(
                    client.disconnect(),
                    timeout=1.5
                )
            except Exception:
                pass


    async def _scan(self):
        """
        Always perform a fresh discovery when recovery is needed.

        We deliberately do not rely on the old Windows BLE address.
        """

        print(
            f"[BLE] Scanning for {self.device_name}..."
        )

        try:
            devices = await asyncio.wait_for(
                BleakScanner.discover(
                    timeout=BLE_SCAN_TIMEOUT
                ),
                timeout=BLE_SCAN_TIMEOUT + 2.0
            )
        except Exception as error:
            self.last_error = str(error)

            print(
                f"[BLE] Scan failed: {error}"
            )

            if "Thread is configured for Windows GUI" in str(error):
                print(
                    "[BLE] Windows COM apartment is STA. "
                    "BLE needs MTA; restart this agent after "
                    "the COM initialization fix."
                )

            return None


        for device in devices:
            name = device.name or ""

            local_name = ""

            try:
                local_name = (
                    device.metadata.get("local_name")
                    or ""
                )
            except Exception:
                pass


            if (
                name == self.device_name
                or local_name == self.device_name
            ):
                print(
                    f"[BLE] Found {self.device_name}: "
                    f"{device.address}"
                )

                return device


        print(
            f"[BLE] {self.device_name} not found."
        )

        return None


    async def _connect_fresh(self):
        """
        Discover and connect to the current BLE device.

        There is intentionally no 'try the old address first' path.
        After an ESP32 restart, Windows can keep a stale BLE object
        and the old connect call can block.
        """

        device = await self._scan()

        if device is None:
            return False


        client = None

        try:
            print(
                f"[BLE] Connecting to "
                f"{device.address}..."
            )

            client = BleakClient(
                device,
                timeout=BLE_CONNECT_TIMEOUT
            )

            await asyncio.wait_for(
                client.connect(),
                timeout=BLE_CONNECT_TIMEOUT + 2.0
            )

            if not client.is_connected:
                raise RuntimeError(
                    "Bleak connect returned without "
                    "an active connection."
                )


            self.device = device
            self.client = client
            self.connected = True
            self.last_error = None

            print(
                "[BLE] Connected to DESKEY."
            )

            self._connection_event.set()

            return True


        except Exception as error:
            self.last_error = str(error)

            print(
                f"[BLE] Fresh connection failed: {error}"
            )

            if client:
                try:
                    await asyncio.wait_for(
                        client.disconnect(),
                        timeout=1.0
                    )
                except Exception:
                    pass

            return False


    async def _connection_worker(self):
        """
        Runs independently from the PC activity detector.

        If DESKEY is restarted, this worker keeps scanning/reconnecting
        while the main activity loop continues normally.
        """

        while not self._stop_event.is_set():

            if not self.is_connected():

                self.connected = False
                self._connection_event.clear()

                connected = await self._connect_fresh()

                if not connected:
                    try:
                        await asyncio.wait_for(
                            self._stop_event.wait(),
                            timeout=BLE_RETRY_DELAY
                        )
                    except asyncio.TimeoutError:
                        pass

                    continue


            # Connected. Wait until the connection disappears or
            # the worker is stopped.
            try:
                while (
                    not self._stop_event.is_set()
                    and self.is_connected()
                ):
                    await asyncio.sleep(0.5)

            except asyncio.CancelledError:
                raise


            if not self._stop_event.is_set():
                print(
                    "[BLE] Connection lost. "
                    "Starting fresh discovery."
                )

                await self._disconnect()


    async def send_state(self, state, idle_seconds=0):
        """
        Non-blocking from the activity detector's perspective.

        If BLE is not connected, the state is stored as the latest
        desired state and the reconnect worker will deliver it once
        the connection is restored.
        """

        payload = (
            '{"state":"'
            + str(state)
            + '","idle_seconds":'
            + str(round(float(idle_seconds), 1))
            + '}'
        )


        async with self._state_lock:
            self._latest_state = payload


        # Never wait for BLE discovery or GATT I/O here.
        # The delivery worker will send this state as soon as BLE
        # is connected. This keeps the PC activity loop responsive.
        return self.is_connected()


    async def _write_latest_state(self):
        async with self._write_lock:

            if not self.is_connected():
                return False


            async with self._state_lock:
                payload = self._latest_state


            if not payload:
                return False


            try:
                await asyncio.wait_for(
                    self.client.write_gatt_char(
                        BLE_STATE_UUID,
                        payload.encode("utf-8"),
                        response=False
                    ),
                    timeout=4.0
                )

                return True


            except Exception as error:
                self.last_error = str(error)

                print(
                    f"[BLE] Write failed: {error}"
                )

                await self._disconnect()

                return False


    async def _deliver_latest_state_loop(self):
        """
        Sends the latest PC state after every successful reconnect.

        This is important after an ESP32 restart: if the user is already
        coding/browsing/music/etc., DESKEY immediately receives that
        current state without waiting for a new activity transition.
        """

        last_payload = None

        while not self._stop_event.is_set():

            if self.is_connected():

                async with self._state_lock:
                    payload = self._latest_state


                if payload and payload != last_payload:

                    success = await self._write_latest_state()

                    if success:
                        last_payload = payload


            try:
                await asyncio.wait_for(
                    self._stop_event.wait(),
                    timeout=0.25
                )
            except asyncio.TimeoutError:
                pass


    async def run(self):
        await self.start()

        try:
            await self._deliver_latest_state_loop()
        finally:
            await self.stop()


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
# MAIN
# ============================================================

async def main_async():

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
    # PREPARE WINDOWS BLE / COM APARTMENT
    #
    # The PC agent also uses pycaw for audio detection. pycaw/comtypes
    # can initialize COM as STA on Windows. Bleak's console/asyncio
    # WinRT backend needs MTA unless a GUI message loop is integrated.
    # See Bleak's Windows troubleshooting documentation.
    # ========================================================

    if uninitialize_sta is not None:
        try:
            uninitialize_sta()
        except Exception:
            pass


    # ========================================================
    # START BLE WORKERS
    #
    # The activity detector is never blocked by BLE discovery or
    # connection attempts. The worker scans/reconnects in the
    # background until DESKEY is available.
    # ========================================================

    await client.start()

    print(
        "[BLE] Background connection worker started."
    )


    # ========================================================
    # MANUAL MODE
    # ========================================================

    if args.state:

        print(
            f"Sending: {args.state}"
        )

        # Give the background worker a short opportunity to connect.
        deadline = time.monotonic() + 10.0

        while (
            not client.is_connected()
            and time.monotonic() < deadline
        ):
            await asyncio.sleep(0.2)

        success = await client.send_state(
            args.state,
            0
        )

        if success:
            # Allow the delivery worker to perform the GATT write.
            await asyncio.sleep(0.5)
            print("[OK] State queued/sent")
            await client.stop()
            return 0

        print("[ERROR] DESKEY not connected.")
        await client.stop()
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
    print("Press Ctrl+C to stop.")
    print()

    last_state = None
    connection_errors = 0

    try:

        while True:

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
                    f"[BLE] DESKEY unavailable "
                    f"(attempt {connection_errors}). "
                    "Background reconnect will keep scanning."
                )

            await asyncio.sleep(
                args.interval
            )


    except KeyboardInterrupt:

        print()
        print("DESKEY Bluetooth agent stopped.")

    finally:

        await client.stop()

    return 0


def main():
    try:
        return asyncio.run(main_async())
    except KeyboardInterrupt:
        print("\n[DESKEY] Stopped by user.")
        return 0


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":

    main()
