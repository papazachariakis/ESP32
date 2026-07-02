/*
 * ESP32 Full Control
 * WiFi + Web UI + GPIO control via screw terminals
 *
 * Pins (breakout labels):
 *   D2  = GPIO 2  (onboard LED)
 *   D4  = GPIO 4  (output 1 - relay/LED)
 *   D5  = GPIO 5  (output 2 - relay/LED)
 *   D18 = GPIO 18 (output 3)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"

#define LED_PIN   2
#define OUT1_PIN  4
#define OUT2_PIN  5
#define OUT3_PIN  18

const int OUTPUT_PINS[] = { LED_PIN, OUT1_PIN, OUT2_PIN, OUT3_PIN };
const char* OUTPUT_NAMES[] = { "LED (D2)", "Output D4", "Output D5", "Output D18" };
const int OUTPUT_COUNT = 4;

WebServer server(80);
bool apMode = false;
bool pinState[4] = { false, false, false, false };

String pageHeader(const char* title) {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>" + String(title) + "</title>";
  html += "<style>";
  html += "body{font-family:system-ui,sans-serif;max-width:480px;margin:24px auto;padding:0 16px;background:#0f172a;color:#e2e8f0}";
  html += "h1{font-size:1.4rem} .card{background:#1e293b;border-radius:12px;padding:16px;margin:12px 0}";
  html += "button,a.btn{display:block;width:100%;padding:14px;margin:8px 0;border:0;border-radius:8px;";
  html += "font-size:1rem;cursor:pointer;text-align:center;text-decoration:none}";
  html += ".on{background:#22c55e;color:#052e16}.off{background:#475569;color:#f8fafc}";
  html += ".info{color:#94a3b8;font-size:.9rem} .nav a{display:inline-block;width:auto;margin-right:8px}";
  html += "</style></head><body>";
  html += "<h1>" + String(title) + "</h1>";
  return html;
}

String pageFooter() {
  return "<p class='info'>ESP32 Full Control</p></body></html>";
}

void setOutput(int index, bool on) {
  if (index < 0 || index >= OUTPUT_COUNT) return;
  pinState[index] = on;
  digitalWrite(OUTPUT_PINS[index], on ? HIGH : LOW);
}

void setupPins() {
  for (int i = 0; i < OUTPUT_COUNT; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
    digitalWrite(OUTPUT_PINS[i], LOW);
    pinState[i] = false;
  }
}

bool connectWiFi() {
  if (strlen(WIFI_SSID) == 0) return false;

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("WiFi OK! IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi failed.");
  return false;
}

void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP mode: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASS);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

void handleRoot() {
  String html = pageHeader(apMode ? "ESP32 - AP Mode" : "ESP32 Control");
  html += "<div class='card'>";
  html += "<p class='info'>";
  if (apMode) {
    html += "Mode: Access Point<br>SSID: " + String(AP_SSID) + "<br>IP: ";
    html += WiFi.softAPIP().toString();
    html += "<br><br>Connect phone/PC to WiFi <b>" + String(AP_SSID) + "</b>";
  } else {
    html += "WiFi: " + String(WIFI_SSID) + "<br>IP: ";
    html += WiFi.localIP().toString();
    html += "<br>mDNS: <a href='http://esp32.local'>esp32.local</a>";
  }
  html += "</p></div>";

  for (int i = 0; i < OUTPUT_COUNT; i++) {
    html += "<div class='card'><strong>" + String(OUTPUT_NAMES[i]) + "</strong>";
    html += "<p class='info'>GPIO " + String(OUTPUT_PINS[i]) + " - ";
    html += pinState[i] ? "ON" : "OFF";
    html += "</p>";
    html += "<a class='btn " + String(pinState[i] ? "off" : "on") + "' href='/toggle?pin=";
    html += String(i) + "'>" + String(pinState[i] ? "Turn OFF" : "Turn ON") + "</a></div>";
  }

  html += "<div class='card nav'>";
  html += "<a class='btn off' href='/alloff'>All OFF</a>";
  html += "<a class='btn on' href='/status'>Status (JSON)</a>";
  html += "</div>";
  html += pageFooter();
  server.send(200, "text/html", html);
}

void handleToggle() {
  if (!server.hasArg("pin")) {
    server.send(400, "text/plain", "Missing pin");
    return;
  }
  int pin = server.arg("pin").toInt();
  if (pin < 0 || pin >= OUTPUT_COUNT) {
    server.send(400, "text/plain", "Invalid pin");
    return;
  }
  setOutput(pin, !pinState[pin]);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAllOff() {
  for (int i = 0; i < OUTPUT_COUNT; i++) setOutput(i, false);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStatus() {
  String json = "{";
  json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"cores\":" + String(ESP.getChipCores()) + ",";
  json += "\"flash_mb\":" + String(ESP.getFlashChipSize() / (1024 * 1024)) + ",";
  json += "\"ap_mode\":" + String(apMode ? "true" : "false") + ",";
  json += "\"ip\":\"" + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"outputs\":[";
  for (int i = 0; i < OUTPUT_COUNT; i++) {
    if (i) json += ",";
    json += "{\"gpio\":" + String(OUTPUT_PINS[i]) + ",\"name\":\"" + OUTPUT_NAMES[i] + "\",\"on\":";
    json += pinState[i] ? "true" : "false";
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/alloff", handleAllOff);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Web server started on port 80");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupPins();

  Serial.println();
  Serial.println("=================================");
  Serial.println("  ESP32 Full Control");
  Serial.println("=================================");

  if (!connectWiFi()) {
    startAP();
  } else if (MDNS.begin("esp32")) {
    Serial.println("mDNS: http://esp32.local");
  }

  setupServer();
}

void loop() {
  server.handleClient();
}
