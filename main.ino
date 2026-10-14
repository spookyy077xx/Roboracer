
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== L298N MOTOR PINS =====
#define ENA 13
#define IN1 5
#define IN2 4

#define ENB 16
#define IN3 14
#define IN4 12

// ===== WIFI SETTINGS =====
const char* ssid = "OLL_RoboRacer";
const char* password = "race1234";

ESP8266WebServer server(80);

// ===== MOTOR SETTINGS =====
int motorSpeed = 200;
bool moving = false;

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 1500;

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

// ===== CONTROL MOTORS =====
// Positive = forward, negative = reverse
void setMotors(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  digitalWrite(IN1, left > 0 ? HIGH : LOW);
  digitalWrite(IN2, left < 0 ? HIGH : LOW);

  digitalWrite(IN3, right > 0 ? HIGH : LOW);
  digitalWrite(IN4, right < 0 ? HIGH : LOW);

  analogWrite(ENA, abs(left));
  analogWrite(ENB, abs(right));

  moving = (left != 0 || right != 0);
}

// ===== WIFI CONTROL PAGE =====
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RoboRacer Controller</title>

<style>
body {
  font-family: Arial;
  text-align: center;
  background: #eeeeee;
}
button {
  width: 100px;
  height: 60px;
  margin: 5px;
  font-size: 16px;
  font-weight: bold;
  border: none;
  border-radius: 10px;
  color: white;
  background: #1976d2;
  touch-action: none;
}
.stop { background: #d32f2f; }
</style>
</head>

<body>
<h2>ESP-12E RoboRacer</h2>
<p>Hold a direction to move. Release to stop.</p>

<div>
  <button data-c="F">FORWARD</button>
</div>

<div>
  <button data-c="L">LEFT</button>
  <button class="stop" data-c="S">STOP</button>
  <button data-c="R">RIGHT</button>
</div>

<div>
  <button data-c="B">BACKWARD</button>
</div>

<hr>
<h3>Speed</h3>
<button data-c="1">LOW</button>
<button data-c="5">MEDIUM</button>
<button data-c="9">MAX</button>

<script>
let held = null;
let heartbeat = null;

function send(c) {
  fetch('/cmd?c=' + c, {cache: 'no-store'})
    .catch(() => {});
}

function release() {
  if (held === null) return;

  held = null;
  if (heartbeat) clearInterval(heartbeat);
  heartbeat = null;
  send('S');
}

document.querySelectorAll('button[data-c]').forEach(button => {
  button.addEventListener('pointerdown', event => {
    event.preventDefault();
    const c = button.dataset.c;

    if (c === 'S') {
      release();
      send('S');
      return;
    }

    if (['1', '5', '9'].includes(c)) {
      send(c);
      return;
    }

    if (held !== null) return;

    held = c;
    button.setPointerCapture(event.pointerId);
    send(c);

    heartbeat = setInterval(() => {
      if (held !== null) send(held);
    }, 300);
  });

  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('lostpointercapture', release);
});

window.addEventListener('blur', release);

document.addEventListener('visibilitychange', () => {
  if (document.hidden) release();
});
</script>
</body>
</html>
)rawliteral";

// ===== HANDLE COMMANDS =====
void handleCommand() {
  if (!server.hasArg("c") ||
      server.arg("c").length() != 1) {
    server.send(400, "text/plain", "Invalid command");
    return;
  }

  char c = server.arg("c")[0];
  lastCommandTime = millis();

  switch (c) {
    case 'F':
      setMotors(motorSpeed, motorSpeed);
      break;

    case 'B':
      setMotors(-motorSpeed, -motorSpeed);
      break;

    case 'L':
      setMotors(motorSpeed / 3, motorSpeed);
      break;

    case 'R':
      setMotors(motorSpeed, motorSpeed / 3);
      break;

    case 'S':
      stopMotors();
      break;

    case '1':
      motorSpeed = 100;
      break;

    case '5':
      motorSpeed = 170;
      break;

    case '9':
      motorSpeed = 255;
      break;

    default:
      server.send(400, "text/plain", "Unknown command");
      return;
  }

  server.send(200, "text/plain", "OK");
}

// ===== START ESP8266 =====
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

  WiFi.mode(WIFI_AP);

  bool wifiStarted = WiFi.softAP(ssid, password);

  if (wifiStarted) {
    Serial.println("RoboRacer WiFi started");
    Serial.print("Network: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("Control page: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("WiFi startup failed!");
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", webpage);
  });

  server.on("/cmd", HTTP_GET, handleCommand);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Page not found");
  });

  server.begin();

  lastCommandTime = millis();
}

void loop() {
  server.handleClient();

  // Stop if movement commands stop arriving.
  if (moving &&
      millis() - lastCommandTime > TIMEOUT_MS) {
    stopMotors();
  }

  yield();
}
