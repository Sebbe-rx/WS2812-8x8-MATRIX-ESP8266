#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN   4
#define LED_COUNT 64

const char* ap_ssid = "LED-MATRIX";
const char* ap_password = "alvedon7911";

ESP8266WebServer server(80);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool ledsEnabled = true;
uint8_t brightness = 64;

unsigned long lastSetTime = 0;
unsigned long lastBrightnessTime = 0;
unsigned long lastShowTime = 0;

bool pixelsDirty = false;   // <<< KEY FIX
uint32_t framebuffer[LED_COUNT];
bool frameDirty = false;

unsigned long lastShow = 0;
const unsigned long SHOW_INTERVAL = 30; // ms (≈33 FPS)// ================= WEB PAGE =================

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family:sans-serif; text-align:center; background:#111; color:#eee; }
.grid {
  display:grid;
  grid-template-columns:repeat(8,40px);
  gap:4px;
  justify-content:center;
  margin-top:10px;
  touch-action:none;
}
.cell {
  width:40px; height:40px;
  background:#000;
  border-radius:4px;
  user-select:none;
}
</style>
</head>
<body>

<h2>LED Display</h2>

Color:
<input type="color" id="color" value="#0000ff"><br><br>

Brightness:
<span id="bval">50%</span><br>
<input type="range" min="10" max="255" value="128" id="brightness"><br><br>

<div class="grid" id="grid"></div><br>

<button onclick="clearGrid()">Clear</button>
<button onclick="toggleLED()">ON / OFF</button>

<script>
const grid = document.getElementById("grid");
const picker = document.getElementById("color");
const brightness = document.getElementById("brightness");
const bval = document.getElementById("bval");

let drawing = false;
let lastSend = 0;

brightness.oninput = () => {
  bval.textContent = Math.round(brightness.value / 255 * 100) + "%";
  fetch(`/brightness?val=${brightness.value}`);
};

function hexToRgb(hex) {
  hex = hex.replace("#","");
  return {
    r: parseInt(hex.substr(0,2),16),
    g: parseInt(hex.substr(2,2),16),
    b: parseInt(hex.substr(4,2),16)
  };
}

function sendPixel(x,y) {
  const now = Date.now();
  if (now - lastSend < 60) return;
  lastSend = now;

  const rgb = hexToRgb(picker.value);
  fetch(`/set?x=${x}&y=${y}&r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`);
}

function clearGrid() {
  fetch("/clear");
  document.querySelectorAll(".cell").forEach(c => c.style.background="#000");
}

function toggleLED() {
  fetch("/power?on=1");
}

for (let y=0;y<8;y++) {
  for (let x=0;x<8;x++) {
    const c = document.createElement("div");
    c.className="cell";
    c.onpointerdown = e => {
      drawing=true;
      c.style.background=picker.value;
      sendPixel(x,y);
    };
    c.onpointerenter = e => {
      if (!drawing) return;
      c.style.background=picker.value;
      sendPixel(x,y);
    };
    c.onpointerup = () => drawing=false;
    grid.appendChild(c);
  }
}

document.body.onpointerup = () => drawing=false;
</script>
</body>
</html>
)rawliteral";

// ================= HANDLERS =================

void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleSet() {
  if (!ledsEnabled) {
    server.send(200, "text/plain", "LEDs OFF");
    return;
  }

  if (!server.hasArg("x") || !server.hasArg("y") ||
      !server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
    server.send(400, "text/plain", "BAD ARGS");
    return;
  }

  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();

  if (x < 0 || x > 7 || y < 0 || y > 7) {
    server.send(400, "text/plain", "OUT OF RANGE");
    return;
  }

  int r = constrain(server.arg("r").toInt(), 0, 255);
  int g = constrain(server.arg("g").toInt(), 0, 255);
  int b = constrain(server.arg("b").toInt(), 0, 255);

  int index = y * 8 + x;

  framebuffer[index] = strip.Color(r, g, b);
  frameDirty = true;

  server.send(200, "text/plain", "OK");
}


void handleClear() {
  for (int i = 0; i < LED_COUNT; i++) {
    framebuffer[i] = 0;
  }
  frameDirty = true;
  server.send(200, "text/plain", "CLEARED");
}


void handleBrightness() {
  int val = server.arg("val").toInt();
  brightness = constrain(val, 0, 255);
  strip.setBrightness(brightness);
  server.send(200, "text/plain", "BRIGHTNESS SET");
}


void handlePower() {
  ledsEnabled = server.arg("on").toInt() == 1;

  if (!ledsEnabled) {
    for (int i = 0; i < LED_COUNT; i++) {
      framebuffer[i] = 0;
    }
    frameDirty = true;
  }

  server.send(200, "text/plain", "POWER TOGGLED");
}


// ================= SETUP / LOOP =================

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAP(ap_ssid, ap_password);

  strip.begin();
  strip.setBrightness(brightness);
  strip.clear();
  strip.show();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/clear", handleClear);
  server.on("/brightness", handleBrightness);
  server.on("/power", handlePower);
  server.begin();

  Serial.println("READY");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (frameDirty && (now - lastShow >= SHOW_INTERVAL)) {
    lastShow = now;

    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, framebuffer[i]);
    }

    strip.show();
    frameDirty = false;
  }

  yield();
}
