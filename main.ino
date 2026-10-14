
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== L298N: ESP8266 GPIO NUMBERS =====
#define ENA 13  // GPIO13 / D7
#define IN1 5   // GPIO5  / D1
#define IN2 4   // GPIO4  / D2

#define ENB 16  // GPIO16 / D0
#define IN3 14  // GPIO14 / D5
#define IN4 12  // GPIO12 / D6

// ===== WIFI ACCESS POINT =====
const char* ssid = "OLL_RoboRacer";
const char* password = "robostudios@121321##@@";

ESP8266WebServer server(80);

// ===== MOTOR SETTINGS =====
int motorSpeed = 170;
char currentCommand = 'S';

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 1200;

bool moving = false;

// ===== STOP MOTORS =====
void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  moving = false;
}

// ===== SET MOTOR DIRECTIONS AND SPEED =====
void setMotors(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // Remove PWM before changing direction.
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, left > 0 ? HIGH : LOW);
  digitalWrite(IN2, left < 0 ? HIGH : LOW);

  digitalWrite(IN3, right > 0 ? HIGH : LOW);
  digitalWrite(IN4, right < 0 ? HIGH : LOW);

  analogWrite(ENA, abs(left));
  analogWrite(ENB, abs(right));

  moving = (left != 0 || right != 0);
}

// ===== APPLY SELECTED COMMAND =====
void applyCommand() {
  int s = motorSpeed;

  if (s == 0 || currentCommand == 'S') {
    stopMotors();
    return;
  }

  switch (currentCommand) {
    case 'F':
      setMotors(s, s);
      break;

    case 'B':
      setMotors(-s, -s);
      break;

    case 'L':
      setMotors(-s, s);
      break;

    case 'R':
      setMotors(s, -s);
      break;

    default:
      currentCommand = 'S';
      stopMotors();
      break;
  }
}

// ===== DIRECTION ENDPOINT =====
void handleCommand() {
  if (!server.hasArg("c")) {
    server.send(400, "text/plain", "Missing command");
    return;
  }

  String value = server.arg("c");

  if (value.length() != 1) {
    server.send(400, "text/plain", "Invalid command");
    return;
  }

  char c = value.charAt(0);

  if (c != 'F' && c != 'B' &&
      c != 'L' && c != 'R' && c != 'S') {
    server.send(400, "text/plain", "Unknown command");
    return;
  }

  lastCommandTime = millis();
  currentCommand = c;

  if (c == 'S') {
    stopMotors();
  } else {
    applyCommand();
  }

  server.send(200, "text/plain", "OK");
}

// ===== SPEED ENDPOINT =====
void handleSpeed() {
  if (!server.hasArg("v")) {
    server.send(400, "text/plain", "Missing speed");
    return;
  }

  String value = server.arg("v");

  if (value.length() == 0 || value.length() > 3) {
    server.send(400, "text/plain", "Invalid speed");
    return;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    if (value.charAt(i) < '0' ||
        value.charAt(i) > '9') {
      server.send(400, "text/plain", "Invalid speed");
      return;
    }
  }

  int newSpeed = value.toInt();

  if (newSpeed < 0 || newSpeed > 255) {
    server.send(400, "text/plain",
                "Speed must be 0-255");
    return;
  }

  motorSpeed = newSpeed;

  // Do not refresh the movement safety timer here.
  // Only direction commands/heartbeats refresh it.
  applyCommand();

  server.send(200, "text/plain", "Speed updated");
}

// ===== WEBPAGE =====
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
 content="width=device-width, initial-scale=1,
 maximum-scale=1, user-scalable=no">
<title>RoboRacer</title>

<style>
* {
  box-sizing: border-box;
  -webkit-tap-highlight-color: transparent;
  user-select: none;
}

:root {
  --bg: #101116;
  --panel: #1b1c24;
  --blue: #287cff;
  --red: #e53b49;
  --text: #f6f6fb;
  --muted: #a7a8b9;
}

html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: var(--bg);
  color: var(--text);
  font-family: Arial, sans-serif;
  touch-action: manipulation;
}

.app {
  width: 100%;
  height: 100%;
  padding: 8px 14px;
  display: flex;
  flex-direction: column;
}

header {
  min-height: 38px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

h1 {
  margin: 0;
  font-size: clamp(17px, 3vh, 24px);
}

.status {
  color: #76e5a8;
  font-size: 12px;
}

main {
  flex: 1;
  min-height: 0;
  display: grid;
  grid-template-columns: 1.1fr 0.9fr;
  gap: 12px;
}

.panel {
  min-height: 0;
  min-width: 0;
  background: var(--panel);
  border: 1px solid #2b2c39;
  border-radius: 18px;
  padding: 10px 14px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: space-evenly;
}

.label {
  font-size: clamp(11px, 2vh, 15px);
  color: var(--muted);
  font-weight: bold;
  letter-spacing: 1px;
}

.dpad {
  --pad-size: min(48vh, 40vw, 300px);
  position: relative;
  width: var(--pad-size);
  height: var(--pad-size);
  flex-shrink: 1;
  aspect-ratio: 1;
  border-radius: 50%;
  background: #e8e8f0;
  box-shadow: inset 0 2px 5px #fff,
              0 4px 0 #aaaab8;
  overflow: hidden;
  touch-action: none;
}

.dpad button {
  position: absolute;
  padding: 0;
  border: 0;
  color: #8588ad;
  background: transparent;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: clamp(22px, 5vh, 39px);
  font-weight: bold;
  touch-action: none;
}

.dpad button.active {
  background: #c9cadd;
  color: #454b7d;
}

.arrow-up {
  left: 29%;
  top: 0;
  width: 42%;
  height: 36%;
  clip-path: polygon(0 0,100% 0,100% 100%,
    72% 100%,50% 70%,28% 100%,0 100%);
}

.arrow-down {
  left: 29%;
  bottom: 0;
  width: 42%;
  height: 36%;
  clip-path: polygon(0 0,28% 0,50% 30%,
    72% 0,100% 0,100% 100%,0 100%);
}

.arrow-left {
  left: 0;
  top: 29%;
  width: 36%;
  height: 42%;
  clip-path: polygon(0 0,100% 0,100% 28%,
    70% 50%,100% 72%,100% 100%,0 100%);
}

.arrow-right {
  right: 0;
  top: 29%;
  width: 36%;
  height: 42%;
  clip-path: polygon(0 0,100% 0,100% 100%,
    0 100%,0 72%,30% 50%,0 28%);
}

.center-stop {
  z-index: 5;
  left: 31%;
  top: 31%;
  width: 38%;
  height: 38%;
  border-radius: 50%;
  border: 5px solid #4a4b5e !important;
  background: radial-gradient(circle at 35% 28%,
    #999ab9,#686a8c 72%) !important;
  box-shadow: 0 0 0 4px #c5c6d5,
              inset 0 3px 6px #ffffff55;
}

.center-stop:active {
  background: #d83b48 !important;
}

.hint {
  color: var(--muted);
  font-size: 11px;
  text-align: center;
  line-height: 1.4;
}

.turn-row {
  display: flex;
  width: 100%;
  gap: 9px;
}

.turn {
  flex: 1;
  min-width: 0;
  min-height: 58px;
  border: 1px solid #418cff;
  border-radius: 14px;
  background: #174e98;
  color: white;
  font-size: clamp(13px, 2.3vh, 18px);
  font-weight: bold;
  touch-action: none;
}

.turn.active {
  background: var(--blue);
}

.stop-button {
  width: 100%;
  max-width: 280px;
  min-height: 48px;
  border: 0;
  border-radius: 13px;
  background: var(--red);
  color: white;
  font-size: 17px;
  font-weight: 900;
  touch-action: manipulation;
}

.speed-head {
  display: flex;
  width: 100%;
  align-items: center;
  justify-content: space-between;
  gap: 5px;
  font-size: clamp(12px, 2vh, 15px);
}

.speed-value {
  font-size: clamp(18px, 3.2vh, 25px);
  font-weight: 900;
  color: #7bb9ff;
}

input[type="range"] {
  width: 100%;
  margin: 12px 0;
  accent-color: var(--blue);
  touch-action: pan-y;
}

.speed-labels {
  width: 100%;
  display: flex;
  justify-content: space-between;
  color: var(--muted);
  font-size: 11px;
}

.presets {
  width: 100%;
  display: grid;
  grid-template-columns: repeat(3,1fr);
  gap: 6px;
}

.preset {
  min-height: 39px;
  border: 1px solid #4b5264;
  border-radius: 10px;
  background: #292c39;
  color: white;
  font-weight: bold;
  font-size: 11px;
  touch-action: manipulation;
}

.preset.selected {
  background: var(--blue);
  border-color: var(--blue);
}

@media (orientation: portrait) {
  .app { padding: 6px; }
  main { gap: 6px; }
  .panel { padding: 7px; }
  .dpad { --pad-size: min(39vw,32vh,260px); }
  .turn-row { flex-direction: column; }
  .turn { min-height: 42px; }
  .stop-button { min-height: 40px; }
}

@media (max-height: 400px) and (orientation: landscape) {
  .app { padding: 3px 10px; }
  header { min-height: 27px; }
  .panel { padding: 5px 9px; }
  .dpad { --pad-size: min(42vh,34vw,220px); }
  .turn { min-height: 42px; }
  .stop-button { min-height: 37px; }
  input[type="range"] { margin: 5px 0; }
  .preset { min-height: 31px; }
}
</style>
</head>

<body>
<div class="app">
<header>
  <h1>🏎️ RoboRacer</h1>
  <div class="status" id="status">● Ready</div>
</header>

<main>
<section class="panel">
  <div class="label">DIRECTION PAD</div>

  <div class="dpad">
    <button class="arrow-up" data-c="F"
      aria-label="Forward">▲</button>
    <button class="arrow-down" data-c="B"
      aria-label="Backward">▼</button>
    <button class="arrow-left" data-c="L"
      aria-label="Left">◀</button>
    <button class="arrow-right" data-c="R"
      aria-label="Right">▶</button>
    <button class="center-stop" id="centerStop"
      aria-label="Stop"></button>
  </div>

  <div class="hint">Hold an arrow to move. Center = STOP.</div>
</section>

<section class="panel">
  <div class="label">STEERING</div>

  <div class="turn-row">
    <button class="turn" data-c="L">◀ LEFT</button>
    <button class="turn" data-c="R">RIGHT ▶</button>
  </div>

  <div class="hint">Hold a direction to move. Release it to stop.</div>
  <button class="stop-button" id="stopButton">■ STOP</button>

  <div class="speed-head">
    <strong>Motor speed</strong>
    <span class="speed-value" id="speedValue">170</span>
  </div>

  <input id="speedSlider" type="range"
    min="0" max="255" value="170"
    aria-label="Motor speed">

  <div class="speed-labels">
    <span>Slow</span><span>Medium</span><span>Fast</span>
  </div>

  <div class="presets">
    <button class="preset" data-speed="100">LOW</button>
    <button class="preset selected" data-speed="170">MED</button>
    <button class="preset" data-speed="255">MAX</button>
  </div>
</section>
</main>
</div>

<script>
let held = null;
let pointerId = null;
let heartbeat = null;
let speedTimer = null;
let speed = 170;
let requestQueue = Promise.resolve();
let stopped = true;

// Serialize requests so speed and direction updates do not
// overtake one another.
function request(path) {
  const task = requestQueue.then(() =>
    fetch(path, {
      method: 'GET',
      cache: 'no-store'
    })
  );

  requestQueue = task.catch(() => {});
  return task;
}

function setStatus(message) {
  document.getElementById('status').textContent = message;
}

function sendCommand(c) {
  request('/cmd?c=' + c)
    .then(response => {
      if (!response.ok) throw new Error('Command failed');
      setStatus('● Connected');
    })
    .catch(() => setStatus('● Connection error'));
}

function sendSpeed(value) {
  request('/speed?v=' + value)
    .then(response => {
      if (!response.ok) throw new Error('Speed failed');
      setStatus('● Connected');
    })
    .catch(() => setStatus('● Speed update failed'));
}

function startHeartbeat() {
  stopHeartbeat();

  heartbeat = setInterval(() => {
    if (held !== null) sendCommand(held);
  }, 300);
}

function stopHeartbeat() {
  if (heartbeat !== null) clearInterval(heartbeat);
  heartbeat = null;
}

function clearActive() {
  document.querySelectorAll('[data-c]').forEach(button => {
    button.classList.remove('active');
  });
}

function releasePointer(event) {
  // Ignore stale events belonging to a previous pointer.
  if (pointerId === null || event.pointerId !== pointerId) return;

  const commandWasHeld = held !== null;

  held = null;
  pointerId = null;
  stopped = true;
  stopHeartbeat();
  clearActive();

  if (commandWasHeld) sendCommand('S');
}

document.querySelectorAll('[data-c]').forEach(button => {
  button.addEventListener('pointerdown', event => {
    event.preventDefault();

    if (held !== null) return;

    held = button.dataset.c;
    pointerId = event.pointerId;
    stopped = false;

    button.classList.add('active');

    try {
      button.setPointerCapture(event.pointerId);
    } catch (error) {}

    sendCommand(held);
    startHeartbeat();
  });

  button.addEventListener('pointerup', releasePointer);
  button.addEventListener('pointercancel', releasePointer);
  button.addEventListener('lostpointercapture', releasePointer);
});

function stopAll() {
  const shouldSendStop = !stopped;

  held = null;
  pointerId = null;
  stopped = true;

  stopHeartbeat();
  clearActive();

  // Always send STOP, even if the browser thinks it is stopped.
  sendCommand('S');
}

document.getElementById('centerStop').addEventListener(
  'pointerdown', event => {
    event.preventDefault();
    stopAll();
  }
);

document.getElementById('stopButton').addEventListener(
  'click', stopAll
);

function updateSpeed(value) {
  speed = Number(value);

  document.getElementById('speedValue').textContent = speed;

  document.querySelectorAll('[data-speed]').forEach(button => {
    button.classList.toggle(
      'selected',
      Number(button.dataset.speed) === speed
    );
  });

  if (speedTimer !== null) clearTimeout(speedTimer);

  speedTimer = setTimeout(() => {
    sendSpeed(speed);
    speedTimer = null;
  }, 50);
}

document.getElementById('speedSlider').addEventListener(
  'input', event => updateSpeed(event.target.value)
);

document.querySelectorAll('[data-speed]').forEach(button => {
  button.addEventListener('click', () => {
    document.getElementById('speedSlider').value =
      button.dataset.speed;
    updateSpeed(button.dataset.speed);
  });
});

window.addEventListener('blur', stopAll);

document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopAll();
});

window.addEventListener('pagehide', stopAll);
</script>
</body>
</html>
)rawliteral";

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  analogWriteRange(255);
  analogWriteFreq(1000);

  stopMotors();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  bool apStarted = WiFi.softAP(ssid, password);

  if (apStarted) {
    Serial.println();
    Serial.println("RoboRacer Wi-Fi started");
    Serial.print("Network: ");
    Serial.println(ssid);
    Serial.print("Open: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Wi-Fi startup failed!");
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", webpage);
  });

  server.on("/cmd", HTTP_GET, handleCommand);
  server.on("/speed", HTTP_GET, handleSpeed);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();

  lastComm  andTime = millis();

  Serial.println("Web server ready");
}

// ===== MAIN LOOP =====
void loop() {
  server.handleClient();

  // Stop if direction heartbeats stop, regardless of PWM state.
  if (currentCommand != 'S' &&
      millis() - lastCommandTime >= TIMEOUT_MS) {
    currentCommand = 'S';
    stopMotors();
  }

  yield();
}
