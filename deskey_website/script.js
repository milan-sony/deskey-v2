const faceStates = [
  ["CODING", "coding"], ["BROWSING", "browsing"], ["MUSIC", "music"], ["WATCHING", "watching"],
  ["GAMING", "gaming"], ["CURIOUS", "curious"], ["HAPPY", "happy"], ["THINKING", "thinking"]
];
let faceIndex = 0;
const eyes = document.querySelector("#heroEyes");
const label = document.querySelector("#faceLabel");
const dashState = document.querySelector("#dashState");
const dashIdle = document.querySelector("#dashIdle");
const dashHeart = document.querySelector("#dashHeart");

function animateFace(state) {
  label.textContent = state[0];
  eyes.className = "eyes " + state[1];
  document.querySelectorAll(".eye").forEach((eye, i) => {
    const pupil = eye.querySelector(".pupil");
    const moves = {
      coding: ["translate(5px,4px)", "translate(-5px,4px)"],
      browsing: ["translate(10px,0)", "translate(10px,0)"],
      music: ["translate(0,7px)", "translate(0,7px)"],
      watching: ["translate(0,0)", "translate(0,0)"],
      gaming: ["translate(-5px,6px)", "translate(5px,6px)"],
      curious: ["translate(9px,-5px)", "translate(-9px,-5px)"],
      happy: ["translate(0,4px)", "translate(0,4px)"],
      thinking: ["translate(-7px,-6px)", "translate(7px,-6px)"]
    };
    pupil.style.transform = (moves[state[1]] || moves.coding)[i];
  });
}
setInterval(() => {
  faceIndex = (faceIndex + 1) % faceStates.length;
  animateFace(faceStates[faceIndex]);
}, 2600);
animateFace(faceStates[0]);

const featureData = {
  context: {
    title: "PC Context Engine",
    text: "The Python agent monitors the Windows foreground application, keyboard activity, audio and idle time, then sends the current PC context to DESKEY over Bluetooth Low Energy.",
    code: "Python → BLE → ESP32"
  },
  sleep: {
    title: "Smart Sleep",
    text: "DESKEY uses the PC idle time reported by Python: 30 seconds becomes Sleepy, 2 minutes becomes Sleeping, and 5 minutes reaches the Deep Sleep animation.",
    code: "idle → sleepy → sleeping → deep sleep"
  },
  wake: {
    title: "Context-Aware Wake",
    text: "When activity returns, DESKEY performs its wake animation and Happy reaction, then applies the current PC action reported by Python rather than restoring an old state.",
    code: "wake → happy → current PC state"
  },
  heartbeat: {
    title: "Python Heartbeat",
    text: "BLE state packets also act as the application heartbeat. If Python stops communicating for the configured timeout, the ESP32 can enter Autonomous Mode.",
    code: "BLE packet → heartbeat → ESP32"
  },
  auto: {
    title: "Autonomous Mode",
    text: "When Python is unavailable, DESKEY continues independently with randomized personality behavior instead of becoming completely inactive.",
    code: "Python unavailable → autonomous"
  },
  ble: {
    title: "Bluetooth LE Communication",
    text: "Bluetooth Low Energy is the primary PC-to-ESP32 communication channel. Python performs fresh DESKEY discovery after disconnects and reconnects in the background without blocking PC activity detection.",
    code: "Python → BLE → DESKEY"
  },
  wifi: {
    title: "Optional Wi-Fi Dashboard",
    text: "Wi-Fi is not required for DESKEY's core operation. When enabled, it provides access to the ESP32 web dashboard and optional network features.",
    code: "Wi-Fi → optional dashboard"
  }
};

function updateFeaturePanel(item) {
  const key = item.dataset.feature;
  const data = featureData[key];
  if (!data) return;

  const title = document.getElementById("panelTitle");
  const text = document.getElementById("panelText");
  const code = document.getElementById("panelCode");
  const panel = document.getElementById("featurePanel");

  if (title) title.textContent = data.title;
  if (text) text.textContent = data.text;
  if (code) code.textContent = data.code;

  document.querySelectorAll(".feature-item").forEach(button => {
    button.classList.toggle("active", button === item);
  });

  if (panel) {
    panel.classList.remove("panel-refresh");
    void panel.offsetWidth;
    panel.classList.add("panel-refresh");
  }
}

document.querySelectorAll(".feature-item").forEach(item => {
  item.addEventListener("click", () => updateFeaturePanel(item));
});

const activeFeature = document.querySelector(".feature-item.active") || document.querySelector(".feature-item");
if (activeFeature) updateFeaturePanel(activeFeature);

const emotions = ["RELAXED", "HAPPY", "SAD", "ANGRY", "SLEEPY", "THINKING", "CURIOUS", "EXCITED", "SURPRISED", "CONFUSED", "SCARED", "LAUGHING", "MUSIC", "TYPING", "CODING", "BROWSING", "IDLE", "GAMING", "ERROR", "WATCHING", "SLEEPING", "DEEP_SLEEP"];
document.querySelector("#emotionCloud").innerHTML = emotions.map((x, i) => `<span style="--i:${i}">${x}</span>`).join("");

let idle = 3, heart = .8;
setInterval(() => {
  idle = (idle + 1) % 18;
  heart = (0.45 + Math.random() * .65).toFixed(1);
  dashIdle.textContent = String(idle).padStart(2, "0") + "s";
  dashHeart.textContent = heart + "s";
}, 1000);

const backToTopButton = document.querySelector("#backToTop");
const backToTopTargets = document.querySelectorAll('a[href="#top"]');

function scrollToTop() {
  window.scrollTo({ top: 0, behavior: "smooth" });
}

const updateBackToTopVisibility = () => {
  if (!backToTopButton) return;
  const shouldShow = window.scrollY > 400;
  backToTopButton.classList.toggle("visible", shouldShow);
};

window.addEventListener("scroll", updateBackToTopVisibility);
updateBackToTopVisibility();
backToTopButton?.addEventListener("click", scrollToTop);
backToTopTargets.forEach(link => {
  link.addEventListener("click", (event) => {
    event.preventDefault();
    scrollToTop();
  });
});

const nav = document.querySelector("#mainNav");
const navLinks = document.querySelectorAll(".nav a");
const menu = document.querySelector(".menu-btn");

function closeMobileMenu() {
  if (!nav || !menu) return;
  nav.classList.remove("open");
  menu.setAttribute("aria-expanded", "false");
  menu.setAttribute("aria-label", "Open menu");
  menu.textContent = "☰";
}

function toggleMobileMenu() {
  if (!nav || !menu) return;
  const isOpen = nav.classList.toggle("open");
  menu.setAttribute("aria-expanded", String(isOpen));
  menu.setAttribute("aria-label", isOpen ? "Close menu" : "Open menu");
  menu.textContent = isOpen ? "✕" : "☰";
}

navLinks.forEach((a) => {
  a.addEventListener("click", () => {
    if (window.innerWidth <= 900) {
      closeMobileMenu();
    }
  });
});

menu.addEventListener("click", toggleMobileMenu);
