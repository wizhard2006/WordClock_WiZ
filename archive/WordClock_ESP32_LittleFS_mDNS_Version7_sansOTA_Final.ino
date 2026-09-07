/*
 * WordClock ESP32 – Version LittleFS avec mDNS (http://wordclock.local)
 * © 2024
 *
 * Fonctionnalités :
 * - Horloge à mots sur 104 LEDs WS2812B (8x13), mapping heures/minutes fourni
 * - Synchro NTP, gestion été/hiver France
 * - Animation de test LEDs au démarrage
 * - Affichage statique, LEDs inutilisées éteintes
 * - Affichage AM/PM et "TIMEBYWIZ"
 * - Couleurs, luminosité, PIN LED, etc. configurables via WebUI
 * - LED intégrée bleue (GPIO2) : diagnostic connexion (logique NORMALE HIGH=allumée)
 * - Persistance complète via LittleFS
 * - WebUI conviviale (anglais), accessible via http://wordclock.local
 * - Reset usine et reboot via WebUI
 * - mDNS activé : accès facile par nom réseau (plus besoin de connaître l'IP !)
 *
 * Librairies requises :
 * - Adafruit_NeoPixel
 * - ESPAsyncWebServer (+ AsyncTCP)
 * - NTPClient, WiFiUdp
 * - ArduinoJson
 * - LittleFS
 * - ESPmDNS (inclus dans l'ESP32 Arduino Core)
 *
 * Important :
 * - La LED intégrée bleue (GPIO2, LED_BUILTIN) s'allume avec digitalWrite(HIGH), s'éteint avec digitalWrite(LOW) sur ESP32 (logique NORMALE)
 * - Ne lance les services réseau (NTP, mDNS, serveur web) qu'après connexion WiFi.
 * - En mode AP, seul le serveur web minimal est lancé pour la configuration.
 * - LED_BUILTIN est définie pour assurer la compatibilité avec toutes cartes ESP32.
 */

// === INCLUDES ET DÉFINITION LED_BUILTIN SI NÉCESSAIRE ===
#include <Arduino.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// Définit LED_BUILTIN si non défini (GPIO2 sur la plupart des ESP32 DevKit)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// === PARAMÈTRES PAR DÉFAUT (modifiables via WebUI) ===
#define DEFAULT_LED_PIN    4     // GPIO4 (adapter selon câblage réel)
#define NUM_LEDS           104
#define DEFAULT_BRIGHTNESS  32
#define CONFIG_PATH        "/config.json"
#define MDNS_HOSTNAME      "wordclock" // Accès via http://wordclock.local

// === OBJETS GLOBAUX ===
Adafruit_NeoPixel strip(NUM_LEDS, DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);

// === STRUCTURE CONFIGURATION PERSISTANTE ===
struct Config {
  char ssid[32];
  char password[32];
  char ntpServer[64];
  uint8_t brightness;
  uint32_t colorHour;
  uint32_t colorMinute;
  uint32_t colorAMPM;
  uint32_t colorWIZ;
  uint8_t ledPin;
  bool ledBuiltinOn; // true = LED bleue activée (par défaut), false = LED désactivée
} config;

// === MAPPING LEDS (heures/minutes) fourni par l'utilisateur ===
const uint8_t led_heures[13][16] = {
  {33,34,35,36,37,38, 255},                                                                  // 0h (minuit)
  {0,1,2,52,53,54,55,56, 255},                                                               // 1h
  {5,6,7,8,52,53,54,55,56,57, 255},                                                          // 2h
  {26,27,28,29,30,52,53,54,55,56,57, 255},                                                   // 3h
  {17,18,19,20,21,22,52,53,54,55,56,57, 255},                                                // 4h
  {22,23,24,25,52,53,54,55,56,57, 255},                                                      // 5h
  {30,31,32,52,53,54,55,56,57, 255},                                                         // 6h
  {9,10,11,12,52,53,54,55,56,57, 255},                                                       // 7h
  {13,14,15,16,52,53,54,55,56,57, 255},                                                      // 8h
  {1,2,3,4,52,53,54,55,56,57, 255},                                                          // 9h
  {43,44,45,52,53,54,55,56,57, 255},                                                         // 10h
  {48,49,50,51,52,53,54,55,56,57, 255},                                                      // 11h
  {44,45,46,47, 255}                                                                         // 12h (midi)
};
const uint8_t led_minutes[12][16] = {
  {255},                                                                                      // 0 min
  {65,66,67,68, 255},                                                                         // 5
  {78,79,80, 255},                                                                            // 10
  {75,76,81,82,83,84,85, 255},                                                                // 15
  {70,71,72,73,74, 255},                                                                      // 20
  {65,66,67,68,69,70,71,72,73,74, 255},                                                       // 25
  {75,76,86,87,88,89,90, 255},                                                                // 30
  {59,60,61,62,63,65,66,67,68,69,70,71,72,73,74, 255},                                        // 35
  {59,60,61,62,63,70,71,72,73,74, 255},                                                       // 40
  {59,60,61,62,63,76,77,81,82,83,84,85, 255},                                                 // 45
  {59,60,61,62,63,78,79,80, 255},                                                             // 50
  {59,60,61,62,63,65,66,67,68, 255}                                                           // 55
};
const uint8_t led_timebywiz[9] = {95,96,97,98,99,100,101,102,103}; // "TIMEBYWIZ"
const uint8_t led_am[2] = {93,94};
const uint8_t led_pm[4] = {91,92,93,94};

// === LED INTÉGRÉE STATUT WIFI ===
enum LedState { WIFI_SEARCH, WIFI_ERROR_AP, WIFI_CONNECTED, LED_OFF };
LedState ledState = WIFI_SEARCH;
unsigned long lastLedToggle = 0;
bool ledBuiltinStatus = false;

// === LITTLEFS (PERSISTANCE CONFIG) ===
void loadConfig() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }
  if (LittleFS.exists(CONFIG_PATH)) {
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (f) {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, f);
      if (!err) {
        strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
        strlcpy(config.password, doc["password"] | "", sizeof(config.password));
        strlcpy(config.ntpServer, doc["ntpServer"] | "pool.ntp.org", sizeof(config.ntpServer));
        config.brightness = doc["brightness"] | DEFAULT_BRIGHTNESS;
        config.colorHour = doc["colorHour"] | strip.Color(255,0,0);
        config.colorMinute = doc["colorMinute"] | strip.Color(0,128,255);
        config.colorAMPM = doc["colorAMPM"] | strip.Color(0,255,0);
        config.colorWIZ = doc["colorWIZ"] | strip.Color(128,0,255);
        config.ledPin = doc["ledPin"] | DEFAULT_LED_PIN;
        config.ledBuiltinOn = doc["ledBuiltinOn"] | true;
      }
      f.close();
      return;
    }
  }
  // Valeurs par défaut si première utilisation
  strcpy(config.ssid, "");
  strcpy(config.password, "");
  strcpy(config.ntpServer, "pool.ntp.org");
  config.brightness = DEFAULT_BRIGHTNESS;
  config.colorHour = strip.Color(255,0,0);
  config.colorMinute = strip.Color(0,128,255);
  config.colorAMPM = strip.Color(0,255,0);
  config.colorWIZ = strip.Color(128,0,255);
  config.ledPin = DEFAULT_LED_PIN;
  config.ledBuiltinOn = true; // Par défaut, la LED intégrée bleue (GPIO2) est allumée
  saveConfig();
}
void saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["ntpServer"] = config.ntpServer;
  doc["brightness"] = config.brightness;
  doc["colorHour"] = config.colorHour;
  doc["colorMinute"] = config.colorMinute;
  doc["colorAMPM"] = config.colorAMPM;
  doc["colorWIZ"] = config.colorWIZ;
  doc["ledPin"] = config.ledPin;
  doc["ledBuiltinOn"] = config.ledBuiltinOn;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}
void resetConfig() {
  LittleFS.remove(CONFIG_PATH);
}

// === NTP & TEMPS ===
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // UTC+1 (hiver), ajusté dynamiquement

// DST France calcul
bool isSummerTime_FR(int y, int m, int d, int h, int wday) {
  if (m < 3 || m > 10) return false;
  if (m > 3 && m < 10) return true;
  int lastMarchSunday = 31 - ((5 * y / 4 + 4) % 7);
  int lastOctSunday = 31 - ((5 * y / 4 + 1) % 7);
  if (m == 3) {
    if (d < lastMarchSunday) return false;
    if (d > lastMarchSunday) return true;
    return (h >= 2);
  }
  if (m == 10) {
    if (d < lastOctSunday) return true;
    if (d > lastOctSunday) return false;
    return (h < 3);
  }
  return false;
}
void updateNtpAndDST() {
  if (!timeClient.update()) return;
  time_t t = timeClient.getEpochTime();
  struct tm* tmT = gmtime(&t);
  int offset = 3600;
  if (isSummerTime_FR(tmT->tm_year + 1900, tmT->tm_mon + 1, tmT->tm_mday, tmT->tm_hour, tmT->tm_wday))
    offset = 7200;
  timeClient.setTimeOffset(offset);
}

// === WIFI & AP ===
AsyncWebServer server(80);
bool wifiConnected = false;
String myIP = "";
bool mdnsStarted = false; // Flag pour l'état mDNS

// Fonction de connexion WiFi (STA ou AP)
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  int tries = 0;
  ledState = WIFI_SEARCH;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(200);
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    myIP = WiFi.localIP().toString();
    ledState = WIFI_CONNECTED;
    Serial.print("Connecté au WiFi, IP : ");
    Serial.println(myIP);
  } else {
    wifiConnected = false;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WordClock_Config");
    myIP = WiFi.softAPIP().toString();
    ledState = WIFI_ERROR_AP;
    Serial.println("Mode AP: mDNS non disponible. Connectez-vous au WiFi 'WordClock_Config' et ouvrez http://192.168.4.1");
  }
}

// === ANIMATION SELFTEST (arc-en-ciel ~5 secondes) ===
void selfTestLeds() {
  strip.setBrightness(config.brightness);
  const int steps = 50;
  const int delayPerStep = 100;
  for (int j = 0; j < steps; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.ColorHSV((i * 65536 / NUM_LEDS + j * 65536 / steps) % 65536, 255, 255));
    }
    strip.show();
    delay(delayPerStep);
  }
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0, 0, 0);
  strip.show();
}

// === LED INTÉGRÉE BLEUE (GPIO2) MANAGEMENT ===
// HIGH = LED allumée, LOW = LED éteinte (logique NORMALE sur ESP32)
void updateLedBuiltin() {
  int builtinPin = LED_BUILTIN; // GPIO2 sur la plupart des ESP32 Dev
  // Si l'option de désactivation est cochée, on éteint la LED intégrée
  if (!config.ledBuiltinOn) {
    digitalWrite(builtinPin, LOW); // LOW = LED éteinte (logique normale)
    return;
  }
  unsigned long now = millis();
  switch (ledState) {
    case WIFI_SEARCH:
      if (now - lastLedToggle > 500) {
        ledBuiltinStatus = !ledBuiltinStatus;
        digitalWrite(builtinPin, ledBuiltinStatus ? HIGH : LOW); // HIGH = allumée, LOW = éteinte (log normale)
        lastLedToggle = now;
      }
      break;
    case WIFI_ERROR_AP:
      if (now - lastLedToggle > 120) {
        ledBuiltinStatus = !ledBuiltinStatus;
        digitalWrite(builtinPin, ledBuiltinStatus ? HIGH : LOW);
        lastLedToggle = now;
      }
      break;
    case WIFI_CONNECTED:
      digitalWrite(builtinPin, HIGH); // HIGH = LED allumée
      break;
    case LED_OFF:
      digitalWrite(builtinPin, LOW); // LOW = LED éteinte
      break;
  }
}

// === AFFICHAGE PRINCIPAL HORLOGE ===
void clearAllLeds() {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0, 0, 0);
}
void setLeds(const uint8_t *arr, uint32_t color) {
  for (int i = 0; i < 16 && arr[i] != 255; i++)
    strip.setPixelColor(arr[i], color);
}
void displayTime() {
  clearAllLeds();

  // --- On récupère l'heure réelle pour l'affichage AM/PM ---
  int h_real = timeClient.getHours();
  int h = h_real;
  int m = timeClient.getMinutes();

  // --- Ajustement de l'heure pour les minutes > 30 (affichage wordclock) ---> correction m > 32 car c'est a 32 que les minutes changent. synchronisation du changement de l'heure avec les minutes
  if (m > 32) {
    h = (h + 1) % 24;
  }

  // --- Heures (adaptation minuit/midi) ---
  int hIdx = h % 12;
  if (h == 0) hIdx = 0;
  if (h == 12) hIdx = 12;
  setLeds(led_heures[hIdx], config.colorHour);

  // --- Minutes (arrondi au plus proche, logique inchangée) ---
  int minIdx = (m == 0) ? 0 : ((m + 2) / 5);
  if (minIdx > 11) minIdx = 11;
  if (m != 0) setLeds(led_minutes[minIdx], config.colorMinute);

  // --- AM/PM : affichage basé sur l'heure réelle, et non l'heure "wordclock" ---
  // Cela évite d'afficher le point AM avant minuit réel (ex: "minuit moins vingt-cinq" à 23h33)
  if (h_real < 12) setLeds(led_am, config.colorAMPM);
  else             setLeds(led_pm, config.colorAMPM);

  // --- TIMEBYWIZ ---
  if (h == 21 && m == 0) {
    setLeds(led_timebywiz, config.colorWIZ);
  } else {
    for (int i = 0; i < 9; i++) strip.setPixelColor(led_timebywiz[i], 0, 0, 0);
  }

  strip.setBrightness(config.brightness);
  strip.show();
}

// === WEBUI (interface légère, anglais) ===
String htmlHead() {
  return "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<title>WordClock ESP32</title>"
         "<style>body{font-family:sans-serif;background:#222;color:#eee;margin:0;padding:0;}h1{background:#444;padding:10px;}form{margin:15px;padding:10px;background:#333;border-radius:6px;}input,select{margin:5px;}button{margin:5px;padding:8px 16px;font-size:1em;}</style>"
         "</head><body>";
}
void handleRoot(AsyncWebServerRequest *request) {
  String html = htmlHead();
  html += "<h1>WordClock ESP32</h1>";
  html += "<form action='/save' method='POST'>";
  html += "WiFi SSID: <input name='ssid' value='" + String(config.ssid) + "'><br>";
  html += "WiFi Password: <input name='password' type='password' value='" + String(config.password) + "'><br>";
  html += "NTP Server: <input name='ntpServer' value='" + String(config.ntpServer) + "'><br>";
  html += "LED Pin (GPIO): <input name='ledPin' type='number' min='0' max='39' value='" + String(config.ledPin) + "'><br>";
  html += "Brightness: <input name='brightness' type='number' min='1' max='255' value='" + String(config.brightness) + "'><br>";
  char colorHourBuf[8]; sprintf(colorHourBuf, "#%06x", config.colorHour & 0xFFFFFF);
  char colorMinuteBuf[8]; sprintf(colorMinuteBuf, "#%06x", config.colorMinute & 0xFFFFFF);
  char colorAMPMBuf[8]; sprintf(colorAMPMBuf, "#%06x", config.colorAMPM & 0xFFFFFF);
  char colorWIZBuf[8]; sprintf(colorWIZBuf, "#%06x", config.colorWIZ & 0xFFFFFF);
  html += "Hour color: <input name='colorHour' type='color' value='" + String(colorHourBuf) + "'><br>";
  html += "Minute color: <input name='colorMinute' type='color' value='" + String(colorMinuteBuf) + "'><br>";
  html += "AM/PM color: <input name='colorAMPM' type='color' value='" + String(colorAMPMBuf) + "'><br>";
  html += "TIMEBYWIZ color: <input name='colorWIZ' type='color' value='" + String(colorWIZBuf) + "'><br>";
  // Correction logique : la case "Disable internal LED" est cochée si la LED doit être éteinte (config.ledBuiltinOn == false)
  html += "Disable internal LED: <input name='ledBuiltinOn' type='checkbox' ";
  if (!config.ledBuiltinOn) html += "checked";
  html += "><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<form action='/reset' method='POST'><button style='background:#b44;color:white;'>Factory Reset</button></form>";
  html += "<form action='/reboot' method='POST'><button>Reboot</button></form>";
  html += "<hr><b>Time (NTP):</b> " + String(timeClient.getFormattedTime()) + "<br>";
  html += "<b>WiFi Status:</b> " + String(wifiConnected ? "Connected" : "AP Config") + "<br>";
  html += "<b>IP Address:</b> " + myIP + "<br>";
  html += "<b>mDNS:</b> ";
  if (wifiConnected && mdnsStarted) {
    html += "Access via <b>http://" MDNS_HOSTNAME ".local</b><br>";
  } else {
    html += "Unavailable in AP mode<br>";
  }
  html += "<b>RSSI:</b> " + String(WiFi.RSSI()) + " dBm<br>";
  html += "</body></html>";
  request->send(200, "text/html", html);
}
void handleSave(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid", true)) strncpy(config.ssid, request->getParam("ssid", true)->value().c_str(), 31);
  if (request->hasParam("password", true)) strncpy(config.password, request->getParam("password", true)->value().c_str(), 31);
  if (request->hasParam("ntpServer", true)) strncpy(config.ntpServer, request->getParam("ntpServer", true)->value().c_str(), 63);
  if (request->hasParam("brightness", true)) config.brightness = request->getParam("brightness", true)->value().toInt();
  if (request->hasParam("ledPin", true)) config.ledPin = request->getParam("ledPin", true)->value().toInt();
  if (request->hasParam("colorHour", true)) config.colorHour = strtoul(request->getParam("colorHour", true)->value().c_str() + 1, NULL, 16);
  if (request->hasParam("colorMinute", true)) config.colorMinute = strtoul(request->getParam("colorMinute", true)->value().c_str() + 1, NULL, 16);
  if (request->hasParam("colorAMPM", true)) config.colorAMPM = strtoul(request->getParam("colorAMPM", true)->value().c_str() + 1, NULL, 16);
  if (request->hasParam("colorWIZ", true)) config.colorWIZ = strtoul(request->getParam("colorWIZ", true)->value().c_str() + 1, NULL, 16);
  // Correction logique : la case "Disable internal LED" est cochée si on veut éteindre la LED
  config.ledBuiltinOn = !request->hasParam("ledBuiltinOn", true); // checked = désactivation
  saveConfig();
  ESP.restart();
}
void handleReset(AsyncWebServerRequest *request) {
  resetConfig();
  ESP.restart();
}
void handleReboot(AsyncWebServerRequest *request) {
  ESP.restart();
}
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.begin();
}

// === SETUP & LOOP ===
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // LOW = LED éteinte au boot (logique normale ESP32)

  loadConfig();

  strip.setPin(config.ledPin);
  strip.begin();
  strip.show();

  selfTestLeds();

  setupWiFi();

  // On ne lance les services réseau qu'après connexion WiFi réussie (mode STA)
  if (wifiConnected) {
    timeClient.setPoolServerName(config.ntpServer);
    timeClient.begin();
    updateNtpAndDST();

    if (!mdnsStarted && MDNS.begin(MDNS_HOSTNAME)) {
      mdnsStarted = true;
      Serial.println("mDNS started: http://" MDNS_HOSTNAME ".local");
    }

    setupWebServer();
  } else {
    // En mode AP, seul le serveur web minimal pour config est lancé
    setupWebServer();
  }
}

void loop() {
  static unsigned long lastDisplay = 0;
  updateLedBuiltin();

  // Affichage de l'heure toutes les secondes (si le service NTP est actif)
  if (wifiConnected && millis() - lastDisplay > 1000) {
    timeClient.update();
    updateNtpAndDST();
    displayTime();
    lastDisplay = millis();
  } else if (!wifiConnected && millis() - lastDisplay > 1000) {
    // Mode AP : on garde l'affichage
    displayTime();
    lastDisplay = millis();
  }
}