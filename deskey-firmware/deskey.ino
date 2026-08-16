/*
============================================================
DESKEY - Desktop Buddy
============================================================

ESP32 + SSD1306 OLED + FluxGarage RoboEyes

CORE BEHAVIOUR
------------------------------------------------------------

Python connected:
    Python controls the current PC context.

Python disconnected:
    ESP32 switches to Autonomous Mode.

PC inactivity:
    30 sec  -> Sleepy
    2 min   -> Sleeping
    5 min   -> Deep Sleep

PC activity returns:
    Wake animation
        ↓
    Happy
        ↓
    CURRENT PC STATE FROM PYTHON

Example:

    Browsing
       ↓
    Sleep
       ↓
    Wake
       ↓
    Happy
       ↓
    Browsing


IMPORTANT STATE SEPARATION
------------------------------------------------------------

currentState
    = What DESKEY is displaying right now.

lastPcState
    = What Python says the PC is currently doing.

This allows DESKEY to sleep without losing its
current PC context.


============================================================
*/


#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <FluxGarage_RoboEyes.h>


// Forward declaration: bootScreen is used by the Wi-Fi provisioning code
// before its full implementation appears later in the sketch.
void bootScreen(
  const String& line1,
  const String& line2 = "",
  const String& line3 = "");


// ============================================================
// WIFI
// ============================================================

const unsigned long WIFI_TIMEOUT = 20000;

// Wi-Fi provisioning
const char* WIFI_SETUP_AP = "DESKEY-SETUP";
const char* WIFI_SETUP_PASSWORD = "deskey123";
const byte DNS_PORT = 53;

Preferences wifiPreferences;
DNSServer dnsServer;
WiFiUDP discoveryUdp;
const uint16_t DISCOVERY_PORT = 4210;
const char* MDNS_HOSTNAME = "deskey";
bool mdnsStarted = false;
bool discoveryStarted = false;
bool wifiSetupMode = false;


// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define SDA_PIN 21
#define SCL_PIN 22


Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET);


RoboEyes<Adafruit_SSD1306> roboEyes(display);


// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);


// ============================================================
// CONTROL MODE
// ============================================================

enum ControlMode {

  MODE_AUTONOMOUS,

  MODE_EXTERNAL

};


ControlMode controlMode =
  MODE_AUTONOMOUS;


// ============================================================
// BUDDY STATES
// ============================================================

enum BuddyState {

  // ----------------------------------------------------------
  // Personality
  // ----------------------------------------------------------

  STATE_RELAXED,
  STATE_HAPPY,
  STATE_SAD,
  STATE_ANGRY,
  STATE_SLEEPY,
  STATE_THINKING,
  STATE_CURIOUS,
  STATE_EXCITED,
  STATE_SURPRISED,
  STATE_CONFUSED,
  STATE_SCARED,
  STATE_LAUGHING,

  // ----------------------------------------------------------
  // PC Context
  // ----------------------------------------------------------

  STATE_MUSIC,
  STATE_TYPING,
  STATE_CODING,
  STATE_BROWSING,
  STATE_IDLE,
  STATE_GAMING,
  STATE_ERROR_STATE,
  STATE_WATCHING,

  // ----------------------------------------------------------
  // Sleep
  // ----------------------------------------------------------

  STATE_SLEEPING,
  STATE_DEEP_SLEEP

};


// ============================================================
// CURRENT DISPLAY STATE
// ============================================================

BuddyState currentState =
  STATE_RELAXED;


// ============================================================
// LAST PC STATE
// ============================================================

/*
IMPORTANT:

This is the state that Python last reported.

It is NOT necessarily what the OLED is currently showing.

Example:

    lastPcState = BROWSING
    currentState = DEEP_SLEEP

When the user becomes active:

    currentState -> HAPPY
    then
    currentState -> BROWSING
*/

BuddyState lastPcState =
  STATE_RELAXED;


// ============================================================
// PREVIOUS STATE
// ============================================================

BuddyState previousState =
  STATE_RELAXED;


// ============================================================
// PYTHON HEARTBEAT
// ============================================================

const unsigned long HEARTBEAT_TIMEOUT =
  15000;

unsigned long lastHeartbeat =
  0;

bool pythonConnected =
  false;


// ============================================================
// PC SLEEP TIMERS
// ============================================================

const unsigned long PC_SLEEPY_TIME =
  30;

const unsigned long PC_SLEEP_TIME =
  120;

const unsigned long PC_DEEP_SLEEP_TIME =
  300;


unsigned long lastPcIdleSeconds =
  0;

unsigned long lastPcActivity =
  0;


// ============================================================
// SLEEP SYSTEM
// ============================================================

bool sleepSystemActive =
  false;

unsigned long sleepStateStarted =
  0;


// ============================================================
// AUTONOMOUS MODE
// ============================================================

const unsigned long AUTONOMOUS_MIN_EVENT =
  12000;

const unsigned long AUTONOMOUS_MAX_EVENT =
  30000;


unsigned long lastAutonomousDecision =
  0;

unsigned long nextAutonomousEvent =
  15000;


// ============================================================
// MICRO ANIMATION
// ============================================================

unsigned long lastBlinkTime =
  0;

unsigned long nextBlinkTime =
  10000;

unsigned long lastLookTime =
  0;

unsigned long nextLookTime =
  8000;


// ============================================================
// BOOT
// ============================================================

bool bootAnimationDone =
  false;


// ============================================================
// WIFI
// ============================================================

bool wifiConnected =
  false;


// ============================================================
// STATE → STRING
// ============================================================

const char* stateToString(
  BuddyState state) {

  switch (state) {

    case STATE_RELAXED:
      return "relaxed";

    case STATE_HAPPY:
      return "happy";

    case STATE_SAD:
      return "sad";

    case STATE_ANGRY:
      return "angry";

    case STATE_SLEEPY:
      return "sleepy";

    case STATE_THINKING:
      return "thinking";

    case STATE_CURIOUS:
      return "curious";

    case STATE_EXCITED:
      return "excited";

    case STATE_SURPRISED:
      return "surprised";

    case STATE_CONFUSED:
      return "confused";

    case STATE_SCARED:
      return "scared";

    case STATE_LAUGHING:
      return "laughing";

    case STATE_MUSIC:
      return "music";

    case STATE_TYPING:
      return "typing";

    case STATE_CODING:
      return "coding";

    case STATE_BROWSING:
      return "browsing";

    case STATE_IDLE:
      return "idle";

    case STATE_GAMING:
      return "gaming";

    case STATE_ERROR_STATE:
      return "error";

    case STATE_WATCHING:
      return "watching";

    case STATE_SLEEPING:
      return "sleeping";

    case STATE_DEEP_SLEEP:
      return "deep_sleep";

    default:
      return "unknown";
  }
}


// ============================================================
// STRING → STATE
// ============================================================

bool stringToState(
  const String& state,
  BuddyState& result) {

  if (state == "relaxed") {
    result = STATE_RELAXED;
    return true;
  }

  if (state == "happy") {
    result = STATE_HAPPY;
    return true;
  }

  if (state == "sad") {
    result = STATE_SAD;
    return true;
  }

  if (state == "angry") {
    result = STATE_ANGRY;
    return true;
  }

  if (state == "sleepy") {
    result = STATE_SLEEPY;
    return true;
  }

  if (state == "thinking") {
    result = STATE_THINKING;
    return true;
  }

  if (state == "curious") {
    result = STATE_CURIOUS;
    return true;
  }

  if (state == "excited") {
    result = STATE_EXCITED;
    return true;
  }

  if (state == "surprised") {
    result = STATE_SURPRISED;
    return true;
  }

  if (state == "confused") {
    result = STATE_CONFUSED;
    return true;
  }

  if (state == "scared") {
    result = STATE_SCARED;
    return true;
  }

  if (state == "laughing") {
    result = STATE_LAUGHING;
    return true;
  }

  if (state == "music") {
    result = STATE_MUSIC;
    return true;
  }

  if (state == "typing") {
    result = STATE_TYPING;
    return true;
  }

  if (state == "coding") {
    result = STATE_CODING;
    return true;
  }

  if (state == "browsing") {
    result = STATE_BROWSING;
    return true;
  }

  if (state == "idle") {
    result = STATE_IDLE;
    return true;
  }

  if (state == "gaming") {
    result = STATE_GAMING;
    return true;
  }

  if (state == "error") {
    result = STATE_ERROR_STATE;
    return true;
  }

  if (state == "watching") {
    result = STATE_WATCHING;
    return true;
  }

  if (state == "sleeping") {
    result = STATE_SLEEPING;
    return true;
  }

  if (state == "deep_sleep") {
    result = STATE_DEEP_SLEEP;
    return true;
  }

  return false;
}


// ============================================================
// JSON STRING
// ============================================================

String getJsonString(
  const String& json,
  const String& key) {

  String search =
    "\"" + key + "\"";


  int index =
    json.indexOf(search);


  if (index == -1) {
    return "";
  }


  index =
    json.indexOf(
      ":",
      index);


  if (index == -1) {
    return "";
  }


  int start =
    json.indexOf(
      "\"",
      index + 1);


  if (start == -1) {
    return "";
  }


  int end =
    json.indexOf(
      "\"",
      start + 1);


  if (end == -1) {
    return "";
  }


  return json.substring(
    start + 1,
    end);
}


// ============================================================
// JSON NUMBER
// ============================================================

long getJsonNumber(
  const String& json,
  const String& key,
  long defaultValue) {

  String search =
    "\"" + key + "\"";


  int index =
    json.indexOf(search);


  if (index == -1) {
    return defaultValue;
  }


  index =
    json.indexOf(
      ":",
      index);


  if (index == -1) {
    return defaultValue;
  }


  index++;


  while (
    index < json.length() && (json[index] == ' ' || json[index] == '\t')) {

    index++;
  }


  int end = index;


  while (
    end < json.length() && (isDigit(json[end]) || json[end] == '-')) {

    end++;
  }


  if (end == index) {
    return defaultValue;
  }


  return json.substring(
               index,
               end)
    .toInt();
}


// ============================================================
// RESET EYE SETTINGS
// ============================================================

void resetEyeSettings() {

  roboEyes.setHFlicker(OFF);

  roboEyes.setVFlicker(OFF);

  roboEyes.setIdleMode(OFF);

  roboEyes.setCuriosity(OFF);

  roboEyes.setCyclops(OFF);

  roboEyes.setSweat(OFF);

  roboEyes.setMood(DEFAULT);

  roboEyes.setWidth(
    36,
    36);

  roboEyes.setHeight(
    36,
    36);

  roboEyes.setBorderradius(
    8,
    8);

  roboEyes.setSpacebetween(
    10);

  roboEyes.setPosition(
    DEFAULT);

  roboEyes.setAutoblinker(
    OFF);
}


// ============================================================
// CONFIGURE STATE
// ============================================================

void configureState() {

  resetEyeSettings();


  switch (currentState) {

    case STATE_RELAXED:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(36, 36);

      roboEyes.setHeight(36, 36);

      roboEyes.setAutoblinker(
        ON,
        4,
        3);

      break;


    case STATE_HAPPY:

      roboEyes.setMood(HAPPY);

      roboEyes.setWidth(40, 40);

      roboEyes.setHeight(38, 38);

      roboEyes.setBorderradius(
        10,
        10);

      roboEyes.setAutoblinker(
        ON,
        3,
        2);

      break;


    case STATE_SAD:

      roboEyes.setMood(TIRED);

      roboEyes.setWidth(38, 38);

      roboEyes.setHeight(22, 22);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(
        ON,
        3,
        2);

      break;


    case STATE_ANGRY:

      roboEyes.setMood(ANGRY);

      roboEyes.setWidth(42, 42);

      roboEyes.setHeight(26, 26);

      roboEyes.setBorderradius(
        4,
        4);

      roboEyes.setHFlicker(
        ON,
        1);

      roboEyes.setAutoblinker(
        ON,
        5,
        2);

      break;


    case STATE_SLEEPY:

      roboEyes.setMood(TIRED);

      roboEyes.setWidth(40, 40);

      roboEyes.setHeight(18, 18);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_THINKING:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(34, 34);

      roboEyes.setHeight(34, 34);

      roboEyes.setCuriosity(ON);

      roboEyes.setPosition(N);

      roboEyes.setAutoblinker(
        ON,
        4,
        2);

      break;


    case STATE_CURIOUS:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(40, 40);

      roboEyes.setHeight(40, 40);

      roboEyes.setCuriosity(ON);

      roboEyes.setPosition(E);

      roboEyes.setAutoblinker(
        ON,
        5,
        2);

      break;


    case STATE_EXCITED:

      roboEyes.setMood(HAPPY);

      roboEyes.setWidth(44, 44);

      roboEyes.setHeight(44, 44);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_SURPRISED:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(46, 46);

      roboEyes.setHeight(46, 46);

      roboEyes.setBorderradius(
        14,
        14);

      roboEyes.setAutoblinker(OFF);

      break;


    case STATE_CONFUSED:

      roboEyes.setMood(DEFAULT);

      roboEyes.setSweat(ON);

      roboEyes.setCuriosity(ON);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_SCARED:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(44, 44);

      roboEyes.setHeight(44, 44);

      roboEyes.setHFlicker(
        ON,
        2);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_LAUGHING:

      roboEyes.setMood(HAPPY);

      roboEyes.setWidth(36, 36);

      roboEyes.setHeight(36, 36);

      roboEyes.setAutoblinker(OFF);

      roboEyes.anim_laugh();

      break;


    case STATE_MUSIC:

      roboEyes.setMood(HAPPY);

      roboEyes.setWidth(38, 38);

      roboEyes.setHeight(38, 38);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_TYPING:

      roboEyes.setMood(DEFAULT);

      roboEyes.setCuriosity(ON);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(
        ON,
        4,
        2);

      break;


    case STATE_CODING:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(34, 34);

      roboEyes.setHeight(32, 32);

      roboEyes.setCuriosity(ON);

      roboEyes.setPosition(N);

      roboEyes.setAutoblinker(
        ON,
        5,
        2);

      break;


    case STATE_BROWSING:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(36, 36);

      roboEyes.setHeight(36, 36);

      roboEyes.setIdleMode(
        ON,
        3,
        3);

      roboEyes.setAutoblinker(
        ON,
        4,
        3);

      break;


    case STATE_IDLE:

      roboEyes.setMood(TIRED);

      roboEyes.setWidth(38, 38);

      roboEyes.setHeight(24, 24);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_GAMING:

      roboEyes.setMood(ANGRY);

      roboEyes.setWidth(40, 40);

      roboEyes.setHeight(28, 28);

      roboEyes.setHFlicker(
        ON,
        1);

      roboEyes.setAutoblinker(
        ON,
        6,
        3);

      break;


    case STATE_ERROR_STATE:

      roboEyes.setMood(DEFAULT);

      roboEyes.setSweat(ON);

      roboEyes.setHFlicker(
        ON,
        1);

      roboEyes.setAutoblinker(
        ON,
        2,
        1);

      break;


    case STATE_WATCHING:

      roboEyes.setMood(DEFAULT);

      roboEyes.setWidth(42, 42);

      roboEyes.setHeight(42, 42);

      roboEyes.setBorderradius(
        14,
        14);

      roboEyes.setAutoblinker(
        ON,
        6,
        4);

      break;


    case STATE_SLEEPING:

      roboEyes.setMood(TIRED);

      roboEyes.setWidth(40, 40);

      roboEyes.setHeight(12, 12);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(
        ON,
        8,
        5);

      break;


    case STATE_DEEP_SLEEP:

      roboEyes.setMood(TIRED);

      roboEyes.setWidth(34, 34);

      roboEyes.setHeight(7, 7);

      roboEyes.setPosition(S);

      roboEyes.setAutoblinker(OFF);

      break;
  }
}


// ============================================================
// CHANGE STATE
// ============================================================

void changeState(
  BuddyState newState) {

  if (
    currentState == newState) {

    return;
  }


  previousState =
    currentState;


  currentState =
    newState;


  configureState();


  Serial.print(
    "[STATE] ");

  Serial.print(
    stateToString(previousState));

  Serial.print(
    " -> ");

  Serial.println(
    stateToString(currentState));
}


// ============================================================
// WAKE
// ============================================================

void wakeUp() {

  Serial.println(
    "[SLEEP] Wake animation.");


  // ----------------------------------------------------------
  // Leave the sleep system immediately.
  // ----------------------------------------------------------

  sleepSystemActive =
    false;


  // ----------------------------------------------------------
  // Wake animation ONLY.
  //
  // IMPORTANT:
  // Do NOT restore lastPcState here.
  //
  // Python will report the CURRENT PC state immediately
  // after activity resumes, and the caller will apply it.
  // ----------------------------------------------------------

  roboEyes.close();

  delay(250);


  roboEyes.open();

  delay(250);


  roboEyes.setPosition(E);

  delay(200);


  roboEyes.setPosition(W);

  delay(200);


  roboEyes.setPosition(DEFAULT);

  delay(200);


  // ----------------------------------------------------------
  // Temporary wake reaction.
  //
  // This is NOT the final PC state.
  // ----------------------------------------------------------

  changeState(
    STATE_HAPPY);

  delay(700);


  Serial.println(
    "[WAKE] Waiting for current PC state from Python.");
}


// ============================================================
// SLEEPY
// ============================================================

void enterSleepyMode() {

  if (
    currentState == STATE_SLEEPY) {

    return;
  }


  Serial.println(
    "[SLEEP] Sleepy.");


  sleepSystemActive =
    true;


  sleepStateStarted =
    millis();


  changeState(
    STATE_SLEEPY);
}


// ============================================================
// SLEEPING
// ============================================================

void enterSleepingMode() {

  if (
    currentState == STATE_SLEEPING) {

    return;
  }


  Serial.println(
    "[SLEEP] Sleeping.");


  sleepSystemActive =
    true;


  sleepStateStarted =
    millis();


  changeState(
    STATE_SLEEPING);
}


// ============================================================
// DEEP SLEEP
// ============================================================

void enterDeepSleepMode() {

  if (
    currentState == STATE_DEEP_SLEEP) {

    return;
  }


  Serial.println(
    "[SLEEP] Deep sleep.");


  sleepSystemActive =
    true;


  sleepStateStarted =
    millis();


  changeState(
    STATE_DEEP_SLEEP);
}


// ============================================================
// PC ACTIVITY
// ============================================================

void processPcActivity(
  unsigned long idleSeconds) {

  lastPcIdleSeconds =
    idleSeconds;


  // ----------------------------------------------------------
  // ACTIVE USER
  // ----------------------------------------------------------

  if (
    idleSeconds < 5) {

    lastPcActivity =
      millis();


    // --------------------------------------------------------
    // If DESKEY was sleeping, wake it.
    // --------------------------------------------------------

    if (
      sleepSystemActive || currentState == STATE_SLEEPY || currentState == STATE_SLEEPING || currentState == STATE_DEEP_SLEEP) {

      wakeUp();
    }


    return;
  }


  // ----------------------------------------------------------
  // SLEEP SYSTEM
  // ----------------------------------------------------------

  if (
    idleSeconds >= PC_SLEEPY_TIME) {

    if (
      idleSeconds >= PC_DEEP_SLEEP_TIME) {

      enterDeepSleepMode();
    }

    else if (
      idleSeconds >= PC_SLEEP_TIME) {

      enterSleepingMode();
    }

    else {

      enterSleepyMode();
    }
  }
}


// ============================================================
// HEARTBEAT
// ============================================================

void handleHeartbeat() {

  lastHeartbeat =
    millis();


  pythonConnected =
    true;


  if (
    server.hasArg("plain")) {

    String body =
      server.arg("plain");


    // --------------------------------------------------------
    // Python heartbeat contains the CURRENT PC state.
    // This is the source of truth.
    // --------------------------------------------------------

    String stateString =
      getJsonString(
        body,
        "state");


    BuddyState reportedState;

    bool validState =
      stringToState(
        stateString,
        reportedState);


    unsigned long idleSeconds =
      getJsonNumber(
        body,
        "idle_seconds",
        0);


    // --------------------------------------------------------
    // Always remember what Python says the PC is doing NOW.
    // --------------------------------------------------------

    if (
      validState) {

      lastPcState =
        reportedState;
    }


    // --------------------------------------------------------
    // Process inactivity / wake first.
    //
    // If DESKEY was sleeping and activity has returned,
    // wakeUp() only performs the wake animation.
    // It does NOT restore lastPcState.
    // --------------------------------------------------------

    processPcActivity(
      idleSeconds);


    // --------------------------------------------------------
    // IMPORTANT:
    //
    // Apply the CURRENT state reported by Python AFTER
    // sleep/wake processing.
    //
    // This guarantees:
    //
    // Sleep
    //   ↓
    // User becomes active
    //   ↓
    // Python detects CURRENT action
    //   ↓
    // Wake animation
    //   ↓
    // CURRENT action animation
    // --------------------------------------------------------

    if (
      validState && !sleepSystemActive && idleSeconds < 5) {

      controlMode =
        MODE_EXTERNAL;


      changeState(
        reportedState);


      Serial.print(
        "[PC] Current action after wake: ");


      Serial.println(
        stateToString(
          reportedState));
    }
  }


  server.send(
    200,
    "application/json",
    "{\"status\":\"ok\"}");
}


// ============================================================
// SET STATE
// ============================================================

void handleSetState() {

  if (
    !server.hasArg("plain")) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"no body\"}");

    return;
  }


  String body =
    server.arg("plain");


  String stateString =
    getJsonString(
      body,
      "state");


  BuddyState newState;


  if (
    !stringToState(
      stateString,
      newState)) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"unknown state\"}");

    return;
  }


  // ----------------------------------------------------------
  // Python is alive.
  // ----------------------------------------------------------

  lastHeartbeat =
    millis();


  pythonConnected =
    true;


  // ----------------------------------------------------------
  // This is the CURRENT state reported by Python.
  // ----------------------------------------------------------

  lastPcState =
    newState;


  unsigned long idleSeconds =
    getJsonNumber(
      body,
      "idle_seconds",
      0);


  // ----------------------------------------------------------
  // Process sleep/wake BEFORE applying the PC state.
  //
  // If the buddy was asleep, wakeUp() shows only the wake
  // animation. The current PC state is applied below.
  // ----------------------------------------------------------

  processPcActivity(
    idleSeconds);


  // ----------------------------------------------------------
  // If the PC is currently idle enough to be sleeping,
  // keep the sleep animation.
  // ----------------------------------------------------------

  if (
    sleepSystemActive) {

    server.send(
      200,
      "application/json",

      String(
        "{\"state\":\"")
        +

        stateToString(
          currentState)
        +

        "\",\"pc_state\":\"" +

        stateToString(
          lastPcState)
        +

        "\",\"sleep\":true}");


    return;
  }


  // ----------------------------------------------------------
  // External control / current PC state.
  // ----------------------------------------------------------

  controlMode =
    MODE_EXTERNAL;


  changeState(
    newState);


  Serial.print(
    "[PC] Current action: ");


  Serial.println(
    stateToString(
      newState));


  server.send(
    200,
    "application/json",

    String(
      "{\"state\":\"")
      +

      stateToString(
        currentState)
      +

      "\",\"pc_state\":\"" +

      stateToString(
        lastPcState)
      +

      "\",\"mode\":\"external\"}");
}


// ============================================================
// AUTONOMOUS MODE
// ============================================================

void handleAutonomousMode() {

  Serial.println(
    "[MODE] Autonomous.");


  controlMode =
    MODE_AUTONOMOUS;


  pythonConnected =
    false;


  sleepSystemActive =
    false;


  lastAutonomousDecision =
    millis();


  nextAutonomousEvent =
    random(
      AUTONOMOUS_MIN_EVENT,
      AUTONOMOUS_MAX_EVENT);


  chooseAutonomousState();


  server.send(
    200,
    "application/json",
    "{\"mode\":\"autonomous\"}");
}


// ============================================================
// MANUAL SLEEP
// ============================================================

void handleSleep() {

  String state =
    "";


  if (
    server.hasArg("plain")) {

    state =
      getJsonString(
        server.arg("plain"),
        "state");
  }


  controlMode =
    MODE_EXTERNAL;


  sleepSystemActive =
    true;


  if (
    state == "sleepy") {

    enterSleepyMode();
  }

  else if (
    state == "sleeping") {

    enterSleepingMode();
  }

  else {

    enterDeepSleepMode();
  }


  server.send(
    200,
    "application/json",
    "{\"status\":\"ok\"}");
}


// ============================================================
// MANUAL WAKE
// ============================================================

void handleWake() {

  wakeUp();


  server.send(
    200,
    "application/json",
    "{\"status\":\"awake\"}");
}


// ============================================================
// STATUS
// ============================================================

void handleStatus() {

  unsigned long now =
    millis();


  unsigned long heartbeatAge =
    now - lastHeartbeat;


  String mode =
    controlMode == MODE_AUTONOMOUS
      ? "autonomous"
      : "external";


  String ip =
    WiFi.status() == WL_CONNECTED
      ? WiFi.localIP().toString()
      : "Not connected";


  String response =
    String("{") +

    "\"state\":\"" + stateToString(currentState) +

    "\","

    "\"pc_state\":\""
    + stateToString(
      lastPcState)
    +

    "\","

    "\"mode\":\""
    + mode +

    "\","

    "\"python_connected\":"
    +

    (pythonConnected
       ? "true"
       : "false")
    +

    ","

    "\"wifi_connected\":"
    +

    (WiFi.status() == WL_CONNECTED
       ? "true"
       : "false")
    +

    ","

    "\"ip\":\""
    + ip + "\","

           "\"heartbeat_age_ms\":"
    + String(
      heartbeatAge)
    +

    ","

    "\"pc_idle_seconds\":"
    + String(
      lastPcIdleSeconds)
    +

    ","

    "\"uptime_seconds\":"
    + String(
      now / 1000)
    +

    ","

    "\"free_heap\":"
    + String(
      ESP.getFreeHeap())
    +

    ","

    "\"wifi_rssi\":"
    + String(
      WiFi.status() == WL_CONNECTED
        ? WiFi.RSSI()
        : 0)
    +

    ","

    "\"sleep_active\":"
    +

    (sleepSystemActive
       ? "true"
       : "false")
    +

    "}";


  server.send(
    200,
    "application/json",
    response);
}


// ============================================================
// WIFI PROVISIONING
// ============================================================

String getSavedWiFiSSID() {

  wifiPreferences.begin("wifi", true);

  String ssid =
    wifiPreferences.getString("ssid", "");

  wifiPreferences.end();

  return ssid;
}


String getSavedWiFiPassword() {

  wifiPreferences.begin("wifi", true);

  String password =
    wifiPreferences.getString("password", "");

  wifiPreferences.end();

  return password;
}


void saveWiFiCredentials(
  const String& ssid,
  const String& password) {

  wifiPreferences.begin("wifi", false);

  wifiPreferences.putString(
    "ssid",
    ssid);

  wifiPreferences.putString(
    "password",
    password);

  wifiPreferences.end();
}


void forgetWiFiCredentials() {

  wifiPreferences.begin("wifi", false);

  wifiPreferences.clear();

  wifiPreferences.end();
}


String htmlEscape(
  const String& value) {

  String result = value;

  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("\"", "&quot;");
  result.replace("'", "&#39;");

  return result;
}


String buildWiFiSetupPage(
  const String& message = "") {

  int networkCount =
    WiFi.scanNetworks();

  String options =
    "<option value=\"\">Select a Wi-Fi network</option>";

  for (
    int i = 0;
    i < networkCount;
    i++) {

    String ssid =
      WiFi.SSID(i);

    if (ssid.length() == 0) {
      continue;
    }

    options +=
      "<option value=\"" + htmlEscape(ssid) + "\">" + htmlEscape(ssid) + " (" + String(WiFi.RSSI(i)) + " dBm)" + "</option>";
  }

  WiFi.scanDelete();

  String page = R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta charset="utf-8">
<title>DESKEY Wi-Fi Setup</title>
<style>
:root{--bg:#09090b;--card:#111113;--card2:#18181b;--border:#27272a;--text:#fafafa;--muted:#a1a1aa;--accent:#8b5cf6}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,sans-serif}
.wrap{width:min(460px,calc(100% - 28px));margin:40px auto}.logo{width:56px;height:56px;border:1px solid var(--border);background:var(--card2);border-radius:16px;display:grid;place-items:center;font-size:28px;margin-bottom:18px}
h1{font-size:25px;margin:0 0 6px}.sub{color:var(--muted);font-size:14px;margin-bottom:22px}.card{background:var(--card);border:1px solid var(--border);border-radius:20px;padding:20px}
label{display:block;font-size:12px;color:var(--muted);margin:0 0 7px}select,input{width:100%;padding:13px 14px;background:var(--card2);border:1px solid var(--border);border-radius:12px;color:var(--text);font-size:15px;outline:none;margin-bottom:16px}button{width:100%;padding:13px;border:0;border-radius:12px;background:var(--accent);color:white;font-size:15px;font-weight:600;cursor:pointer}.note{margin-top:15px;color:var(--muted);font-size:12px;line-height:1.5}.msg{padding:11px 12px;border:1px solid var(--border);background:var(--card2);border-radius:11px;margin-bottom:16px;font-size:13px}
</style>
</head>
<body>
<div class="wrap">
<div class="logo">🤖</div>
<h1>DESKEY Wi-Fi Setup</h1>
<div class="sub">Connect DESKEY to your Wi-Fi network. Credentials are saved on the ESP32.</div>
<div class="card">
)rawliteral";

  if (message.length() > 0) {

    page +=
      "<div class=\"msg\">" + htmlEscape(message) + "</div>";
  }

  page +=
    "<form method=\"POST\" action=\"/wifi/save\">"
    "<label>Wi-Fi Network</label>"
    "<select name=\"ssid\" required>"
    + options + "</select>"
                "<label>Password</label>"
                "<input name=\"password\" type=\"password\" placeholder=\"Wi-Fi password\" autocomplete=\"off\">"
                "<button type=\"submit\">Save &amp; Connect</button>"
                "</form>"
                "<div class=\"note\">"
                "Setup network: <b>"
    + htmlEscape(String(WIFI_SETUP_AP)) + "</b><br>"
                                          "Setup password: <b>"
    + htmlEscape(String(WIFI_SETUP_PASSWORD)) + "</b>"
                                                "</div>"
                                                "</div></div></body></html>";

  return page;
}


void handleWiFiSetupRoot() {

  server.send(
    200,
    "text/html",
    buildWiFiSetupPage());
}


void handleWiFiSave() {

  String ssid =
    server.arg("ssid");

  String password =
    server.arg("password");

  ssid.trim();

  if (ssid.length() == 0) {

    server.send(
      400,
      "text/html",
      buildWiFiSetupPage(
        "Please select a Wi-Fi network."));

    return;
  }

  bootScreen(
    "WiFi",
    "Connecting...",
    ssid);

  Serial.print(
    "[WiFi] Trying network: ");

  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(250);
  WiFi.begin(
    ssid.c_str(),
    password.c_str());

  unsigned long start =
    millis();

  while (
    WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {

    delay(250);
  }

  if (
    WiFi.status() == WL_CONNECTED) {

    saveWiFiCredentials(
      ssid,
      password);

    String ip =
      WiFi.localIP().toString();

    Serial.println(
      "[WiFi] Connected.");

    Serial.print(
      "[WiFi] IP: ");

    Serial.println(ip);

    startNetworkDiscovery();

    server.send(
      200,
      "text/html",
      String(R"rawliteral(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><meta charset="utf-8"><title>DESKEY Connected</title><style>body{margin:0;background:#09090b;color:#fafafa;font-family:system-ui;text-align:center}.wrap{width:min(460px,calc(100% - 28px));margin:70px auto}.card{background:#111113;border:1px solid #27272a;border-radius:20px;padding:30px}h1{font-size:25px}p{color:#a1a1aa;line-height:1.6}.ip{display:inline-block;padding:12px 16px;background:#18181b;border:1px solid #27272a;border-radius:12px;font-weight:700}</style></head><body><div class="wrap"><div class="card"><div style="font-size:42px">✓</div><h1>DESKEY Connected</h1><p>Wi-Fi credentials were saved successfully.</p><p>IP address</p><div class="ip">)rawliteral")
        + ip + R"rawliteral(</div><p>DESKEY will restart and connect automatically.</p></div></div></body></html>)rawliteral");

    delay(1800);

    ESP.restart();

    return;
  }

  WiFi.disconnect(true);

  Serial.println(
    "[WiFi] Connection failed.");

  server.send(
    200,
    "text/html",
    buildWiFiSetupPage(
      "Could not connect. Check the password and try again."));
}


void handleWiFiSetupNotFound() {

  server.sendHeader(
    "Location",
    "/",
    true);

  server.send(
    302,
    "text/plain",
    "");
}


bool startWiFiSetupPortal() {

  wifiSetupMode = true;

  WiFi.mode(WIFI_AP_STA);

  bool apStarted =
    WiFi.softAP(
      WIFI_SETUP_AP,
      WIFI_SETUP_PASSWORD);

  if (!apStarted) {

    Serial.println(
      "[WiFi] Failed to start setup AP.");

    return false;
  }

  IPAddress apIP =
    WiFi.softAPIP();

  dnsServer.start(
    DNS_PORT,
    "*",
    apIP);

  server.on(
    "/",
    HTTP_GET,
    handleWiFiSetupRoot);

  server.on(
    "/wifi/save",
    HTTP_POST,
    handleWiFiSave);

  server.onNotFound(
    handleWiFiSetupNotFound);

  server.enableCORS(true);
  server.begin();

  Serial.println(
    "[WiFi] Setup AP started.");

  Serial.print(
    "[WiFi] Network: ");

  Serial.println(
    WIFI_SETUP_AP);

  Serial.print(
    "[WiFi] Password: ");

  Serial.println(
    WIFI_SETUP_PASSWORD);

  Serial.print(
    "[WiFi] Setup page: http://");

  Serial.print(apIP);

  Serial.println("/");

  bootScreen(
    "WiFi Setup",
    "Connect to:",
    WIFI_SETUP_AP);

  return true;
}


bool connectToSavedWiFi() {

  String ssid =
    getSavedWiFiSSID();

  String password =
    getSavedWiFiPassword();

  if (ssid.length() == 0) {

    Serial.println(
      "[WiFi] No saved credentials.");

    return false;
  }

  Serial.print(
    "[WiFi] Connecting to saved network: ");

  Serial.println(ssid);

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    ssid.c_str(),
    password.c_str());

  unsigned long start =
    millis();

  while (
    WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {

    delay(300);
  }

  return WiFi.status() == WL_CONNECTED;
}


void setupWiFi() {

  bootScreen(
    "WiFi",
    "Connecting...");

  if (connectToSavedWiFi()) {

    wifiConnected = true;

    String ip =
      WiFi.localIP().toString();

    Serial.print(
      "[WiFi] Connected. IP: ");

    Serial.println(ip);

    bootScreen(
      "WiFi Connected",
      "IP Address:",
      ip);

    delay(2500);

    startNetworkDiscovery();

    return;
  }

  wifiConnected = false;

  Serial.println(
    "[WiFi] Saved network unavailable.");

  if (!startWiFiSetupPortal()) {

    bootScreen(
      "WiFi Error",
      "Restarting...");

    delay(2000);
    ESP.restart();
  }

  // Stay here until the user saves valid Wi-Fi.
  while (wifiSetupMode) {

    dnsServer.processNextRequest();
    server.handleClient();

    delay(2);
  }
}


// ============================================================
// BOOT SCREEN
// ============================================================

void bootScreen(
  const String& line1,
  const String& line2,
  const String& line3) {

  display.clearDisplay();


  display.setTextColor(
    SSD1306_WHITE);


  display.setTextSize(2);


  int16_t x1;
  int16_t y1;

  uint16_t w;
  uint16_t h;


  display.getTextBounds(
    "DESKEY",
    0,
    0,
    &x1,
    &y1,
    &w,
    &h);


  display.setCursor(
    (SCREEN_WIDTH - w) / 2,
    3);


  display.println(
    "DESKEY");


  display.setTextSize(1);


  display.getTextBounds(
    line1,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h);


  display.setCursor(
    (SCREEN_WIDTH - w) / 2,
    31);


  display.println(
    line1);


  if (
    line2.length() > 0) {

    display.getTextBounds(
      line2,
      0,
      0,
      &x1,
      &y1,
      &w,
      &h);


    display.setCursor(
      (SCREEN_WIDTH - w) / 2,
      43);


    display.println(
      line2);
  }


  if (
    line3.length() > 0) {

    display.getTextBounds(
      line3,
      0,
      0,
      &x1,
      &y1,
      &w,
      &h);


    display.setCursor(
      (SCREEN_WIDTH - w) / 2,
      54);


    display.println(
      line3);
  }


  display.display();
}


// ============================================================
// BOOT SEQUENCE
// ============================================================

void runBootSequence() {

  bootScreen(
    "Starting...");

  delay(1000);

  setupWiFi();

  bootScreen(
    "Starting Buddy...");

  delay(1200);
}


// ============================================================
// AUTONOMOUS EMOTION
// ============================================================

void chooseAutonomousState() {

  int value =
    random(100);


  if (
    value < 30) {

    changeState(
      STATE_RELAXED);
  }

  else if (
    value < 45) {

    changeState(
      STATE_CURIOUS);
  }

  else if (
    value < 58) {

    changeState(
      STATE_HAPPY);
  }

  else if (
    value < 68) {

    changeState(
      STATE_THINKING);
  }

  else if (
    value < 76) {

    changeState(
      STATE_EXCITED);
  }

  else if (
    value < 84) {

    changeState(
      STATE_SURPRISED);
  }

  else if (
    value < 90) {

    changeState(
      STATE_CONFUSED);
  }

  else if (
    value < 95) {

    changeState(
      STATE_SLEEPY);
  }

  else {

    changeState(
      STATE_LAUGHING);
  }


  nextAutonomousEvent =
    random(
      AUTONOMOUS_MIN_EVENT,
      AUTONOMOUS_MAX_EVENT);
}


// ============================================================
// MICRO BEHAVIOR
// ============================================================

void updateMicroBehavior() {

  unsigned long now =
    millis();


  if (
    now - lastBlinkTime >= nextBlinkTime) {

    lastBlinkTime =
      now;


    nextBlinkTime =
      random(
        7000,
        18000);


    roboEyes.blink(
      true,
      true);
  }


  if (
    now - lastLookTime >= nextLookTime) {

    lastLookTime =
      now;


    nextLookTime =
      random(
        5000,
        14000);


    int direction =
      random(5);


    switch (direction) {

      case 0:
        roboEyes.setPosition(E);
        break;

      case 1:
        roboEyes.setPosition(W);
        break;

      case 2:
        roboEyes.setPosition(N);
        break;

      case 3:
        roboEyes.setPosition(S);
        break;

      default:
        roboEyes.setPosition(DEFAULT);
        break;
    }
  }
}


// ============================================================
// AUTONOMOUS ENGINE
// ============================================================

void updateAutonomousMode() {

  unsigned long now =
    millis();


  if (
    now - lastAutonomousDecision >= nextAutonomousEvent) {

    lastAutonomousDecision =
      now;


    chooseAutonomousState();
  }


  updateMicroBehavior();
}


// ============================================================
// HEARTBEAT CHECK
// ============================================================

void checkHeartbeat() {

  if (
    !pythonConnected) {

    return;
  }


  unsigned long elapsed =
    millis() - lastHeartbeat;


  if (
    elapsed > HEARTBEAT_TIMEOUT) {

    Serial.println(
      "[HEARTBEAT] Python disconnected.");


    pythonConnected =
      false;


    controlMode =
      MODE_AUTONOMOUS;


    /*
    IMPORTANT:

    Python disappearing does NOT mean
    PC inactivity.

    Cancel sleep and become autonomous.
    */

    sleepSystemActive =
      false;


    lastAutonomousDecision =
      millis();


    nextAutonomousEvent =
      random(
        AUTONOMOUS_MIN_EVENT,
        AUTONOMOUS_MAX_EVENT);


    chooseAutonomousState();


    Serial.print(
      "[AUTONOMOUS] ");


    Serial.println(
      stateToString(
        currentState));
  }
}


// ============================================================
// BOOT EYES
// ============================================================

void bootEyesAnimation() {

  static unsigned long start =
    millis();


  unsigned long elapsed =
    millis() - start;


  if (
    elapsed < 700) {

    roboEyes.close();
  }

  else if (
    elapsed < 1400) {

    roboEyes.open();
  }

  else if (
    elapsed < 1900) {

    roboEyes.setPosition(E);
  }

  else if (
    elapsed < 2400) {

    roboEyes.setPosition(W);
  }

  else if (
    elapsed < 2900) {

    roboEyes.setPosition(DEFAULT);
  }

  else {

    bootAnimationDone =
      true;


    currentState =
      STATE_RELAXED;


    configureState();


    lastAutonomousDecision =
      millis();


    nextAutonomousEvent =
      random(
        AUTONOMOUS_MIN_EVENT,
        AUTONOMOUS_MAX_EVENT);


    Serial.println(
      "[BOOT] DESKEY ready.");
  }
}


// ============================================================
// DASHBOARD HTML
// ============================================================

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta
name="viewport"
content="width=device-width,initial-scale=1"
>

<title>DESKEY</title>


<style>

:root {

--bg:#09090b;
--surface:#111113;
--surface2:#18181b;
--border:#27272a;
--text:#fafafa;
--muted:#a1a1aa;
--accent:#8b5cf6;
--green:#22c55e;
--red:#ef4444;

}


* {

box-sizing:border-box;

}


body {

margin:0;

background:var(--bg);

color:var(--text);

font-family:
Inter,
system-ui,
sans-serif;

}


.container {

width:min(
1100px,
calc(100% - 32px)
);

margin:auto;

padding:
28px 0 50px;

}


header {

display:flex;

justify-content:space-between;

align-items:center;

margin-bottom:28px;

gap:20px;

}


.brand {

display:flex;

align-items:center;

gap:14px;

}


.logo {

width:48px;

height:48px;

display:grid;

place-items:center;

border-radius:14px;

background:var(--surface2);

border:1px solid var(--border);

font-size:24px;

}


h1 {

margin:0;

font-size:24px;

}


.brand p {

margin:3px 0 0;

color:var(--muted);

font-size:13px;

}


.connection {

display:flex;

align-items:center;

gap:8px;

padding:
8px 12px;

border-radius:999px;

background:var(--surface2);

border:1px solid var(--border);

font-size:13px;

}


.dot {

width:8px;

height:8px;

border-radius:50%;

background:var(--red);

}


.dot.online {

background:var(--green);

}


.grid {

display:grid;

grid-template-columns:
repeat(4,1fr);

gap:12px;

}


.card {

background:var(--surface);

border:1px solid var(--border);

border-radius:18px;

padding:18px;

}


.stat {

min-height:110px;

}


.label {

color:var(--muted);

font-size:12px;

margin-bottom:12px;

}


.value {

font-size:20px;

font-weight:650;

word-break:break-word;

}


.small {

color:var(--muted);

font-size:12px;

margin-top:6px;

}


.section {

margin-top:18px;

}


.section-header {

display:flex;

justify-content:space-between;

margin-bottom:12px;

}


.section-header h2 {

font-size:16px;

margin:0;

}


.section-header span {

font-size:12px;

color:var(--muted);

}


.buttons {

display:grid;

grid-template-columns:
repeat(4,1fr);

gap:10px;

}


button {

border:
1px solid var(--border);

background:var(--surface2);

color:var(--text);

padding:12px;

border-radius:12px;

cursor:pointer;

transition:.15s;

}


button:hover {

border-color:#52525b;

transform:translateY(-1px);

}


button.primary {

background:var(--accent);

border-color:var(--accent);

}


.animation {

min-height:78px;

text-align:left;

}


.emoji {

font-size:22px;

margin-bottom:6px;

}


.animation.active {

border-color:var(--accent);

background:
rgba(139,92,246,.12);

}


.info {

display:grid;

grid-template-columns:
repeat(2,1fr);

gap:10px;

}


.info-item {

background:var(--surface2);

padding:12px;

border-radius:12px;

}


.info-item label {

display:block;

color:var(--muted);

font-size:11px;

margin-bottom:5px;

}


.info-item strong {

font-size:13px;

}


.footer {

text-align:center;

color:var(--muted);

font-size:11px;

margin-top:28px;

}


@media(max-width:800px) {

.grid {

grid-template-columns:
repeat(2,1fr);

}

.buttons {

grid-template-columns:
repeat(3,1fr);

}

}


@media(max-width:520px) {

.container {

width:
calc(100% - 20px);

padding-top:18px;

}


header {

align-items:flex-start;

}


.grid {

grid-template-columns:
repeat(2,1fr);

}


.buttons {

grid-template-columns:
repeat(2,1fr);

}


.info {

grid-template-columns:1fr;

}

}

</style>

</head>


<body>


<div class="container">


<header>

<div class="brand">

<div class="logo">
🤖
</div>

<div>

<h1>DESKEY</h1>

<p>Desktop Buddy</p>

</div>

</div>


<div class="connection">

<div
id="wifiDot"
class="dot"
></div>

<span id="wifiStatus">
Checking...
</span>

</div>

</header>


<div class="grid">


<div class="card stat">

<div class="label">
CURRENT DISPLAY
</div>

<div
id="state"
class="value"
>
—
</div>

<div
id="mode"
class="small"
>
—
</div>

</div>


<div class="card stat">

<div class="label">
CURRENT PC STATE
</div>

<div
id="pcState"
class="value"
>
—
</div>

<div class="small">
Remembered context
</div>

</div>


<div class="card stat">

<div class="label">
PYTHON
</div>

<div
id="python"
class="value"
>
—
</div>

<div
id="heartbeat"
class="small"
>
—
</div>

</div>


<div class="card stat">

<div class="label">
PC IDLE
</div>

<div
id="idle"
class="value"
>
—
</div>

<div class="small">
seconds
</div>

</div>


</div>


<div class="section">


<div class="section-header">

<h2>Control</h2>

<span id="sleep">
Normal
</span>

</div>


<div class="card">


<div class="buttons">


<button
class="primary"
onclick="autonomous()"
>
🧠 Autonomous
</button>


<button
onclick="wake()"
>
☀️ Wake
</button>


<button
onclick="sleepState('sleepy')"
>
😴 Sleepy
</button>


<button
onclick="sleepState('sleeping')"
>
💤 Sleeping
</button>


</div>

</div>

</div>


<div class="section">


<div class="section-header">

<h2>Animations</h2>

<span>
Manual
</span>

</div>


<div class="card">


<div class="buttons">


<button
class="animation"
data-state="relaxed"
onclick="sendState('relaxed')"
>
<div class="emoji">😌</div>
Relaxed
</button>


<button
class="animation"
data-state="happy"
onclick="sendState('happy')"
>
<div class="emoji">😄</div>
Happy
</button>


<button
class="animation"
data-state="sad"
onclick="sendState('sad')"
>
<div class="emoji">😢</div>
Sad
</button>


<button
class="animation"
data-state="angry"
onclick="sendState('angry')"
>
<div class="emoji">😠</div>
Angry
</button>


<button
class="animation"
data-state="sleepy"
onclick="sendState('sleepy')"
>
<div class="emoji">😴</div>
Sleepy
</button>


<button
class="animation"
data-state="thinking"
onclick="sendState('thinking')"
>
<div class="emoji">🤔</div>
Thinking
</button>


<button
class="animation"
data-state="curious"
onclick="sendState('curious')"
>
<div class="emoji">👀</div>
Curious
</button>


<button
class="animation"
data-state="excited"
onclick="sendState('excited')"
>
<div class="emoji">🤩</div>
Excited
</button>


<button
class="animation"
data-state="surprised"
onclick="sendState('surprised')"
>
<div class="emoji">😲</div>
Surprised
</button>


<button
class="animation"
data-state="confused"
onclick="sendState('confused')"
>
<div class="emoji">😵</div>
Confused
</button>


<button
class="animation"
data-state="scared"
onclick="sendState('scared')"
>
<div class="emoji">😨</div>
Scared
</button>


<button
class="animation"
data-state="laughing"
onclick="sendState('laughing')"
>
<div class="emoji">😂</div>
Laughing
</button>


<button
class="animation"
data-state="music"
onclick="sendState('music')"
>
<div class="emoji">🎵</div>
Music
</button>


<button
class="animation"
data-state="typing"
onclick="sendState('typing')"
>
<div class="emoji">⌨️</div>
Typing
</button>


<button
class="animation"
data-state="coding"
onclick="sendState('coding')"
>
<div class="emoji">💻</div>
Coding
</button>


<button
class="animation"
data-state="browsing"
onclick="sendState('browsing')"
>
<div class="emoji">🌐</div>
Browsing
</button>


<button
class="animation"
data-state="idle"
onclick="sendState('idle')"
>
<div class="emoji">🧘</div>
Idle
</button>


<button
class="animation"
data-state="gaming"
onclick="sendState('gaming')"
>
<div class="emoji">🎮</div>
Gaming
</button>


<button
class="animation"
data-state="error"
onclick="sendState('error')"
>
<div class="emoji">⚠️</div>
Error
</button>


<button
class="animation"
data-state="watching"
onclick="sendState('watching')"
>
<div class="emoji">🍿</div>
Watching
</button>


</div>

</div>

</div>


<div class="section">


<div class="section-header">

<h2>System</h2>

<span>
Live
</span>

</div>


<div class="card">


<div class="info">


<div class="info-item">

<label>IP ADDRESS</label>

<strong id="ip">
—
</strong>

</div>


<div class="info-item">

<label>UPTIME</label>

<strong id="uptime">
—
</strong>

</div>


<div class="info-item">

<label>FREE HEAP</label>

<strong id="heap">
—
</strong>

</div>


<div class="info-item">

<label>WIFI RSSI</label>

<strong id="rssi">
—
</strong>

</div>


<div class="info-item">

<label>HEARTBEAT AGE</label>

<strong id="heartbeatAge">
—
</strong>

</div>


<div class="info-item">

<label>SLEEP SYSTEM</label>

<strong id="sleepSystem">
—
</strong>

</div>


</div>

</div>

</div>


<div class="footer">

DESKEY · ESP32 Desktop Buddy

</div>


</div>


<script>


async function api(
  url,
  options={}
) {

  try {

    const response =
      await fetch(
        url,
        options
      );

    return await response.json();

  }

  catch(error) {

    return null;

  }

}


async function sendState(
  state
) {

  await api(
    "/state",
    {

      method:"POST",

      headers:{
        "Content-Type":
          "application/json"
      },

      body:JSON.stringify({

        state:state,

        idle_seconds:0

      })

    }
  );


  updateStatus();

}


async function autonomous() {

  await api(
    "/mode",
    {
      method:"POST"
    }
  );


  updateStatus();

}


async function sleepState(
  state
) {

  await api(
    "/sleep",
    {

      method:"POST",

      headers:{
        "Content-Type":
          "application/json"
      },

      body:JSON.stringify({
        state:state
      })

    }
  );


  updateStatus();

}


async function wake() {

  await api(
    "/wake",
    {
      method:"POST"
    }
  );


  updateStatus();

}


function formatUptime(
  seconds
) {

  seconds =
    Number(seconds);


  const days =
    Math.floor(
      seconds / 86400
    );


  seconds %= 86400;


  const hours =
    Math.floor(
      seconds / 3600
    );


  seconds %= 3600;


  const minutes =
    Math.floor(
      seconds / 60
    );


  seconds =
    Math.floor(
      seconds % 60
    );


  return (

    (days
      ? days + "d "
      : "") +

    (hours
      ? hours + "h "
      : "") +

    (minutes
      ? minutes + "m "
      : "") +

    seconds + "s"

  );

}


async function updateStatus() {

  const data =
    await api(
      "/status"
    );


  if (!data) {

    document
      .getElementById(
        "wifiStatus"
      )
      .innerText =
      "Offline";


    document
      .getElementById(
        "wifiDot"
      )
      .classList
      .remove(
        "online"
      );


    return;

  }


  document
    .getElementById(
      "state"
    )
    .innerText =
    data.state;


  document
    .getElementById(
      "pcState"
    )
    .innerText =
    data.pc_state;


  document
    .getElementById(
      "mode"
    )
    .innerText =
    "Mode: " +
    data.mode;


  document
    .getElementById(
      "wifiStatus"
    )
    .innerText =
    data.wifi_connected
      ? "Wi-Fi Connected"
      : "Wi-Fi Offline";


  document
    .getElementById(
      "wifiDot"
    )
    .classList
    .toggle(
      "online",
      data.wifi_connected
    );


  document
    .getElementById(
      "python"
    )
    .innerText =
    data.python_connected
      ? "Connected"
      : "Disconnected";


  document
    .getElementById(
      "heartbeat"
    )
    .innerText =
    "Heartbeat: " +
    data.heartbeat_age_ms +
    " ms";


  document
    .getElementById(
      "idle"
    )
    .innerText =
    data.pc_idle_seconds;


  document
    .getElementById(
      "ip"
    )
    .innerText =
    data.ip;


  document
    .getElementById(
      "uptime"
    )
    .innerText =
    formatUptime(
      data.uptime_seconds
    );


  document
    .getElementById(
      "heap"
    )
    .innerText =
    Math.round(
      data.free_heap / 1024
    ) +
    " KB";


  document
    .getElementById(
      "rssi"
    )
    .innerText =
    data.wifi_connected
      ? data.wifi_rssi + " dBm"
      : "—";


  document
    .getElementById(
      "heartbeatAge"
    )
    .innerText =
    data.heartbeat_age_ms +
    " ms";


  document
    .getElementById(
      "sleepSystem"
    )
    .innerText =
    data.sleep_active
      ? "Active"
      : "Inactive";


  document
    .getElementById(
      "sleep"
    )
    .innerText =
    data.sleep_active
      ? "Sleep active"
      : "Normal";


  document
    .querySelectorAll(
      ".animation"
    )
    .forEach(
      button => {

        button
          .classList
          .toggle(
            "active",
            button.dataset.state ===
              data.state
          );

      }
    );

}


updateStatus();


setInterval(
  updateStatus,
  2000
);


</script>


</body>

</html>

)rawliteral";


// ============================================================
// NETWORK DISCOVERY
// ============================================================

void startNetworkDiscovery() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mdnsStarted) {

    if (MDNS.begin(MDNS_HOSTNAME)) {

      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;

      Serial.println("[mDNS] DESKEY available at http://deskey.local");

    } else {

      Serial.println("[mDNS] Failed to start.");
    }
  }

  if (!discoveryStarted) {

    if (discoveryUdp.begin(DISCOVERY_PORT)) {
      discoveryStarted = true;
      Serial.print("[DISCOVERY] UDP port: ");
      Serial.println(DISCOVERY_PORT);
    } else {
      Serial.println("[DISCOVERY] Failed to start UDP discovery.");
    }
  }
}


void handleNetworkDiscovery() {

  if (!discoveryStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  int packetSize = discoveryUdp.parsePacket();

  if (packetSize <= 0) {
    return;
  }

  char buffer[64];
  int length = discoveryUdp.read(buffer, sizeof(buffer) - 1);

  if (length <= 0) {
    return;
  }

  buffer[length] = '\0';

  String request = String(buffer);
  request.trim();

  if (request == "DESKEY_DISCOVER") {

    String response =
      "DESKEY|" + WiFi.localIP().toString();

    IPAddress senderIP = discoveryUdp.remoteIP();
    uint16_t senderPort = discoveryUdp.remotePort();

    discoveryUdp.beginPacket(
      senderIP,
      senderPort);

    discoveryUdp.print(response);
    discoveryUdp.endPacket();

    Serial.print("[DISCOVERY] Responded to ");
    Serial.println(discoveryUdp.remoteIP());
  }
}


// ============================================================
// WIFI MANAGEMENT FROM NORMAL DASHBOARD
// ============================================================

void handleWiFiChange() {

  // Reuse the setup page while keeping the current ESP32 alive.
  // The normal dashboard can link to this endpoint if desired.

  wifiSetupMode = true;

  WiFi.disconnect();
  delay(250);

  if (!startWiFiSetupPortal()) {

    wifiSetupMode = false;

    server.send(
      500,
      "text/plain",
      "Could not start Wi-Fi setup mode.");

    return;
  }

  // This endpoint cannot safely continue serving the old STA
  // dashboard while the setup AP is active. The setup portal
  // takes over until credentials are saved.
}


void handleWiFiForget() {

  forgetWiFiCredentials();

  server.send(
    200,
    "application/json",
    "{\"status\":\"credentials_deleted\"}");

  delay(500);

  ESP.restart();
}


// ============================================================
// ROOT
// ============================================================

void handleRoot() {

  server.send_P(
    200,
    "text/html",
    DASHBOARD_HTML);
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200);


  delay(300);


  Serial.println();
  Serial.println(
    "======================================");
  Serial.println(
    "              DESKEY");
  Serial.println(
    "        Desktop AI Buddy");
  Serial.println(
    "======================================");


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  Wire.begin(
    SDA_PIN,
    SCL_PIN);


  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR)) {

    Serial.println(
      "[ERROR] OLED initialization failed.");


    while (true) {

      delay(1000);
    }
  }


  Serial.println(
    "[OK] OLED initialized.");


  // ----------------------------------------------------------
  // Random
  // ----------------------------------------------------------

  randomSeed(
    micros());


  // ----------------------------------------------------------
  // RoboEyes
  // ----------------------------------------------------------

  roboEyes.begin(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    100);


  resetEyeSettings();


  // ----------------------------------------------------------
  // Boot
  // ----------------------------------------------------------

  runBootSequence();


  // ----------------------------------------------------------
  // HTTP routes
  // ----------------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED) {

    server.on(
      "/",
      HTTP_GET,
      handleRoot);


    server.on(
      "/state",
      HTTP_POST,
      handleSetState);


    server.on(
      "/heartbeat",
      HTTP_POST,
      handleHeartbeat);


    server.on(
      "/status",
      HTTP_GET,
      handleStatus);


    server.on(
      "/mode",
      HTTP_POST,
      handleAutonomousMode);


    server.on(
      "/sleep",
      HTTP_POST,
      handleSleep);


    server.on(
      "/wake",
      HTTP_POST,
      handleWake);


    server.on(
      "/wifi/forget",
      HTTP_POST,
      handleWiFiForget);


    server.enableCORS(
      true);


    server.begin();


    Serial.println(
      "[HTTP] Dashboard started.");


    Serial.print(
      "[HTTP] http://");


    Serial.print(
      WiFi.localIP());


    Serial.println(
      "/");

    startNetworkDiscovery();
  }


  // ----------------------------------------------------------
  // Timers
  // ----------------------------------------------------------

  lastHeartbeat =
    millis();


  lastPcActivity =
    millis();


  lastAutonomousDecision =
    millis();


  nextAutonomousEvent =
    random(
      AUTONOMOUS_MIN_EVENT,
      AUTONOMOUS_MAX_EVENT);


  lastBlinkTime =
    millis();


  lastLookTime =
    millis();


  nextBlinkTime =
    random(
      7000,
      18000);


  nextLookTime =
    random(
      5000,
      14000);


  // ----------------------------------------------------------
  // Initial state
  // ----------------------------------------------------------

  controlMode =
    MODE_AUTONOMOUS;


  pythonConnected =
    false;


  sleepSystemActive =
    false;


  currentState =
    STATE_RELAXED;


  lastPcState =
    STATE_RELAXED;


  configureState();


  Serial.println(
    "[DESKEY] Ready.");
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // Web server
  // ----------------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED) {

    handleNetworkDiscovery();
    server.handleClient();
  }


  // ----------------------------------------------------------
  // Boot animation
  // ----------------------------------------------------------

  if (
    !bootAnimationDone) {

    bootEyesAnimation();

    roboEyes.update();

    return;
  }


  // ----------------------------------------------------------
  // Heartbeat
  // ----------------------------------------------------------

  checkHeartbeat();


  // ----------------------------------------------------------
  // Autonomous
  // ----------------------------------------------------------

  if (
    controlMode == MODE_AUTONOMOUS &&

    !sleepSystemActive) {

    updateAutonomousMode();
  }


  // ----------------------------------------------------------
  // Sleep
  // ----------------------------------------------------------

  if (
    sleepSystemActive) {

    if (
      currentState == STATE_SLEEPING) {

      if (
        millis() - lastBlinkTime > 12000) {

        lastBlinkTime =
          millis();


        roboEyes.blink(
          true,
          true);
      }
    }
  }


  // ----------------------------------------------------------
  // RoboEyes
  // ----------------------------------------------------------

  roboEyes.update();
}