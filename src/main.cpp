#include <DCF77FreeRTOS.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>

// Create DCF77 receiver instance
// Parameters: input pin, debug flag (DCF_DEBUG / DCF_NODEBUG)
DCF77FreeRTOS receiver(DCF_DATA, DCF_NODEBUG);

// Create TFT driver instance
TFT_eSPI display = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);
WiFiManager wifiManager;
WebServer webServer(80);

// Global variables
int uptimeSeconds = 0;
int syncedShown = 0;
int lastStatus = -1;
uint32_t lastUiTickMs = 0;
uint32_t buttonPressStartMs = 0;
bool buttonWasDown = false;

DCFtime lastTime = {};
bool hasTime = false;

// day of week string array (1..7)
constexpr const char* kDayNames[] = {
  "", "Monday", "Tuesday", "Wednesday",
  "Thursday", "Friday", "Saturday", "Sunday"
};

const char* statusToText(int status) {
  switch (status) {
    case 0:
      return "Disconnected";
    case 1:
      return "No signal";
    case 2:
      return "Synchronising";
    case 3:
      return "OK";
    default:
      return "Unknown";
  }
}

uint16_t statusToColor(int status) {
  switch (status) {
    case 0:
      return TFT_ORANGE;
    case 1:
      return TFT_RED;
    case 2:
      return TFT_MAGENTA;
    case 3:
      return TFT_GREEN;
    default:
      return TFT_WHITE;
  }
}

void handleRoot() {
  String html;
  html.reserve(1024);
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>DCF77 Clock</title>";
  html += "<style>body{font-family:system-ui;margin:1.25rem;}";
  html += ".card{max-width:34rem;padding:1rem;border:1px solid #ccc;border-radius:.5rem;}";
  html += "h1{margin-top:0;}code{background:#f3f3f3;padding:.1rem .3rem;border-radius:.25rem;}</style></head><body>";
  html += "<div class='card'><h1>DCF77 Clock</h1>";
  html += "<p><strong>Wi-Fi:</strong> ";
  html += (WiFi.status() == WL_CONNECTED) ? "Connected" : "Not connected";
  html += "</p><p><strong>IPv4:</strong> <code>" + WiFi.localIP().toString() + "</code></p>";
  html += "</p><p><strong>IPv6:</strong> <code>" + WiFi.localIPv6().toString() + "</code></p>";
  html += "<p><strong>Signal:</strong> ";
  html += statusToText(lastStatus);
  html += "</p>";

  if (hasTime) {
    html += "<p><strong>Time:</strong> ";
    html += String(lastTime.hour) + ":" + (lastTime.minute < 10 ? "0" : "") + String(lastTime.minute);
    html += "</p><p><strong>Date:</strong> ";
    if (lastTime.dow <= 7) {
      html += kDayNames[lastTime.dow];
      html += " ";
    }
    html += "20" + String(lastTime.year) + "-";
    html += (lastTime.month < 10 ? "0" : "") + String(lastTime.month) + "-";
    html += (lastTime.day < 10 ? "0" : "") + String(lastTime.day);
    html += "</p>";
  } else {
    html += "<p><strong>Time:</strong> Not synced yet</p>";
  }

  html += "<p><a href='/api/status'>JSON status</a></p></div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleApiStatus() {
  String json;
  json.reserve(384);
  json += "{";
  json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ipv4\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"ipv6\":\"" + WiFi.localIPv6().toString() + "\",";
  json += "\"dcf_status\":" + String(lastStatus) + ",";
  json += "\"dcf_status_text\":\"" + String(statusToText(lastStatus)) + "\",";
  json += "\"uptime_seconds\":" + String(uptimeSeconds) + ",";
  json += "\"has_time\":" + String(hasTime ? "true" : "false");

  if (hasTime) {
    json += ",\"time\":{\"year\":" + String(lastTime.year);
    json += ",\"month\":" + String(lastTime.month);
    json += ",\"day\":" + String(lastTime.day);
    json += ",\"hour\":" + String(lastTime.hour);
    json += ",\"minute\":" + String(lastTime.minute);
    json += ",\"dow\":" + String(lastTime.dow);
    json += "}";
  }

  json += "}";
  webServer.send(200, "application/json", json);
}

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/status", HTTP_GET, handleApiStatus);
  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "Not found");
  });
  webServer.begin();
  Serial.println("[WEB] HTTP server started on port 80");
}

void checkWifiResetButton() {
  const bool buttonDown = (digitalRead(WIFI_RESET_BTN) == LOW);  // active-low button

  if (buttonDown && !buttonWasDown) {
    buttonPressStartMs = millis();
    buttonWasDown = true;
  }

  if (!buttonDown && buttonWasDown) {
    buttonWasDown = false;
    buttonPressStartMs = 0;
  }

  if (buttonWasDown && (millis() - buttonPressStartMs >= 1000U)) {
    Serial.println("[WIFI] Button held 1s, clearing saved Wi-Fi settings and restarting...");
    wifiManager.resetSettings();
    delay(200);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);  // give serial time to initialize

  Serial.println("[FW] DCF77-Clock V0.1 --- https://github.com/friedkiwi/dcf77-clock");

  // configure wifi reset button GPIO (GPIO35 needs external pull-up/down)
  pinMode(WIFI_RESET_BTN, INPUT);

  // enable display backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // init display
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);

  Serial.println("[DISP] Display initialised.");

  receiver.begin(-1);  // optional LED pin
  Serial.println("[DCF77] DCF77 receiver initialised.");

  Serial.println("[WIFI] Connecting to WiFi...");

  WiFi.enableIpV6();
  WiFi.setHostname("DCF77-Clock");

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (wifiManager.getWiFiIsSaved()) {
    display.print("Connecting to saved\nWiFi network...");
  } else {
    display.print("Please set up wifi using the DCF77-Clock-Setup access point.");
  }

  const bool ok = wifiManager.autoConnect("DCF77-Clock-Setup");
  if (!ok) {
    Serial.println("[WIFI] Failed to connect and hit timeout");
  } else {
    Serial.println("[WIFI] Connected to WiFi.");
    Serial.print("[WIFI] IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] IPv6 address: ");
    Serial.println(WiFi.localIPv6());
  }

  setupWebServer();
  display.fillScreen(TFT_BLACK);
}

void loop() {
  webServer.handleClient();
  checkWifiResetButton();

  const uint32_t now = millis();
  if (now - lastUiTickMs < 1000U) {
    return;
  }
  lastUiTickMs = now;

  DCFtime t;
  if (receiver.getTime(&t)) {
    hasTime = true;
    lastTime = t;

    Serial.printf(
      "[DCF77] DCF time: %02u-%02u-%02u %02u:%02u (dow=%u)\n",
      t.year, t.month, t.day, t.hour, t.minute, t.dow
    );

    // draw clock
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(4);
    display.setCursor(40, 40);
    display.printf("%02u:%02u", t.hour, t.minute);

    // draw date
    display.setTextSize(2);
    display.setCursor(10, 90);
    const char* day = (t.dow <= 7) ? kDayNames[t.dow] : "?";
    display.printf("%s 20%02u-%02u-%02u", day, t.year, t.month, t.day);
  }

  const int status = receiver.getStatus();
  if (status != lastStatus) {
    if (status == 3 && syncedShown == 0) {
      Serial.printf("[DCF77] %05d Status: Signal synced\n", uptimeSeconds);
    } else {
      Serial.printf("[DCF77] %05d Status: %s\n", uptimeSeconds, statusToText(status));
    }

    display.setTextSize(1);
    display.fillRect(0, 0, 180, 12, TFT_BLACK);
    display.setCursor(0, 0);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextWrap(false);
    display.print("DCF:");
    display.setCursor(30, 0);
    display.setTextColor(statusToColor(status), TFT_BLACK);
    display.print(statusToText(status));

    syncedShown = (status == 3) ? 1 : 0;
    lastStatus = status;
  }

  if (uptimeSeconds > 32000) {
    uptimeSeconds = 0;
  }
  uptimeSeconds++;
}
