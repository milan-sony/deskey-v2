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
  context: ["PC Context Engine", "The Python agent monitors Windows foreground applications, keyboard activity, audio and idle time. Foreground context wins over background audio.", "python → /state → ESP32"],
  sleep: ["Smart Sleep", "ESP32 inactivity logic transitions from Sleepy at 30 seconds to Sleeping at 2 minutes and the Deep Sleep animation at 5 minutes.", "idle_seconds → sleep engine"],
  wake: ["Context-Aware Wake", "When PC activity returns, DESKEY performs its wake animation, shows Happy, and then applies the current PC state reported by Python.", "sleep → wake → happy → current"],
  heartbeat: ["Python Heartbeat", "The ESP32 tracks the age of the last Python communication. After the 15-second heartbeat timeout, it can switch to Autonomous Mode.", "15s timeout → autonomous"],
  auto: ["Autonomous Mode", "Without Python, DESKEY can continue independently and choose randomized personality events on a 12–30 second interval.", "random event: 12–30s"],
  wifi: ["Wi-Fi Provisioning + Discovery", "Wi-Fi credentials can be configured without firmware edits. mDNS exposes deskey.local and UDP discovery provides a local-network fallback.", "DESKEY-SETUP → deskey.local"]
};
document.querySelectorAll(".feature-item").forEach(item => {
  item.addEventListener("click", () => {
    document.querySelectorAll(".feature-item").forEach(x => x.classList.remove("active"));
    item.classList.add("active");
    const d = featureData[item.dataset.feature];
    document.querySelector("#panelTitle").textContent = d[0];
    document.querySelector("#panelText").textContent = d[1];
    document.querySelector("#panelCode").textContent = d[2];
  });
});

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
