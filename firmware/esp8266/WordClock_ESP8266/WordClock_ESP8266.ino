/*
 * WordClock ESP8266 – Version complète sur-mesure
 * © 2024
 * 
 * Fonctionnalités :
 * - Affichage d'une horloge à mots sur 104 LEDs WS2812B (8x13), mapping heures/minutes fourni
 * - Synchro automatique NTP (serveur configurable via WebUI), gestion été/hiver France
 * - Animation de test LEDs au démarrage (arc-en-ciel ~5 secondes)
 * - Affichage statique, LEDs inutilisées éteintes
 * - Affichage AM/PM et "TIMEBYWIZ" selon les règles demandées
 * - Couleurs, luminosité, PIN LED, etc. configurables via WebUI
 * - LED intégrée : diagnostic (lent = recherche WiFi, rapide = échec/AP, fixe = connecté, off = désactivée)
 * - Persistance complète via EEPROM
 * - WebUI conviviale (en anglais, interface légère)
 * - Reset usine et reboot depuis la WebUI
 * 
 * Librairies requises :
 * - Adafruit_NeoPixel
 * - ESPAsyncWebServer (+ dépendances ESPAsyncTCP, AsyncTCP)
 * - NTPClient, WiFiUdp
 * - EEPROM
 * 
 * À adapter : le mapping LED doit correspondre à ton panneau réel.
 */

#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// === PARAMÈTRES PAR DÉFAUT (modifiables via WebUI) ===
#define DEFAULT_LED_PIN    D2
#define NUM_LEDS          104
#define EEPROM_SIZE       512
#define DEFAULT_BRIGHTNESS  32

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
  bool ledBuiltinOn;
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

// === EEPROM PERSISTANCE ===
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  // Valeurs par défaut si première utilisation
  // CORRECTIF : sur Xtensa char est signé, un octet 0xFF vaut -1 et la comparaison
  // à 255 était toujours fausse. Le reset usine ne rechargeait donc jamais les
  // valeurs par défaut : la carte repartait avec une config aléatoire.
  if (config.ssid[0] == '\0' || (uint8_t)config.ssid[0] == 0xFF) {
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.ntpServer, "pool.ntp.org");
    config.brightness = DEFAULT_BRIGHTNESS;
    config.colorHour = strip.Color(255,0,0);       // Rouge
    config.colorMinute = strip.Color(0,128,255);   // Bleu
    config.colorAMPM = strip.Color(0,255,0);       // Vert
    config.colorWIZ = strip.Color(128,0,255);      // Violet
    config.ledPin = DEFAULT_LED_PIN;
    config.ledBuiltinOn = true;
    saveConfig();
  }
}
void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}
void resetConfig() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
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
  } else {
    wifiConnected = false;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WordClock_Config");
    myIP = WiFi.softAPIP().toString();
    ledState = WIFI_ERROR_AP;
  }
}

// === ANIMATION SELFTEST (arc-en-ciel ~5 secondes) ===
void selfTestLeds() {
  strip.setBrightness(config.brightness);
  const int steps = 50;           // Nombre d'étapes de l'arc-en-ciel
  const int delayPerStep = 100;   // Durée d'une étape en ms (100 ms)
  for (int j = 0; j < steps; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.ColorHSV((i * 65536 / NUM_LEDS + j * 65536 / steps) % 65536, 255, 255));
    }
    strip.show();
    delay(delayPerStep);
  }
  // Extinction après test
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0, 0, 0);
  strip.show();
}

// === LED INTÉGRÉE MANAGEMENT ===
void updateLedBuiltin() {
  if (!config.ledBuiltinOn) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  unsigned long now = millis();
  switch (ledState) {
    case WIFI_SEARCH: // lent
      if (now - lastLedToggle > 500) {
        ledBuiltinStatus = !ledBuiltinStatus;
        digitalWrite(LED_BUILTIN, ledBuiltinStatus ? LOW : HIGH);
        lastLedToggle = now;
      }
      break;
    case WIFI_ERROR_AP: // rapide
      if (now - lastLedToggle > 120) {
        ledBuiltinStatus = !ledBuiltinStatus;
        digitalWrite(LED_BUILTIN, ledBuiltinStatus ? LOW : HIGH);
        lastLedToggle = now;
      }
      break;
    case WIFI_CONNECTED:
      digitalWrite(LED_BUILTIN, LOW);
      break;
    case LED_OFF:
      digitalWrite(LED_BUILTIN, HIGH);
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

  int h_real = timeClient.getHours();   // heure reelle, sert a AM/PM et TIMEBYWIZ
  int h = h_real;                       // heure "wordclock", decalee apres la demie
  int m = timeClient.getMinutes();

  // --- Ajustement de l'heure pour les minutes > 30 --> correction, > 32 car c'est a 32 que les minutes changent.synchro changement heure et minutes
  if (m > 32) {
    h = (h + 1) % 24; // Incrémente l'heure et gère le passage de 23h à 0h (minuit)
  }

  // --- Heures (adaptation minuit/midi) ---
  int hIdx = h % 12;
  if (h == 0) hIdx = 0; // minuit
  if (h == 12) hIdx = 12; // midi
  setLeds(led_heures[hIdx], config.colorHour);

  // --- Minutes ---
  int minIdx = (m + 2) / 5;                  // arrondi à la tranche la plus proche, 0 à 12
  // CORRECTIF : minIdx == 12 vaut 60 minutes, c'est-à-dire l'heure pleine suivante
  // (h est déjà incrémentée). L'ancien clamp à 11 renvoyait sur "moins cinq" :
  // "moins cinq" restait 7 minutes et l'heure pleine seulement 3.
  if (minIdx > 11) minIdx = 0;
  if (minIdx != 0) setLeds(led_minutes[minIdx], config.colorMinute);

  // --- AM/PM ---
  // CORRECTIF : se lit sur l'heure REELLE, pas sur l'heure prononcee. A 23h40 on
  // affiche "minuit moins vingt", mais on est toujours l'apres-midi.
  if (h_real < 12) setLeds(led_am, config.colorAMPM); // Matin
  else             setLeds(led_pm, config.colorAMPM); // Après-midi (AM/PM)

  // --- TIMEBYWIZ ---
  // Allume les LEDs 95–103 à 21h00 de 21:00:00 à 21:00:59
  if (h_real == 21 && m == 0) {
    setLeds(led_timebywiz, config.colorWIZ); // "TIMEBYWIZ"
  } else {
    // Force l'extinction de ce groupe pour éviter les chevauchements
    for (int i = 0; i < 9; i++) strip.setPixelColor(led_timebywiz[i], 0, 0, 0);
  }

  strip.setBrightness(config.brightness);
  strip.show();
}

// === WEBUI (interface légère, anglais) ===
String htmlHead() {
  return "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<title>WordClock ESP8266</title>"
         "<style>body{font-family:sans-serif;background:#222;color:#eee;margin:0;padding:0;}h1{background:#444;padding:10px;}form{margin:15px;padding:10px;background:#333;border-radius:6px;}input,select{margin:5px;}button{margin:5px;padding:8px 16px;font-size:1em;}</style>"
         "</head><body>";
}
void handleRoot(AsyncWebServerRequest *request) {
  String html = htmlHead();
  html += "<h1>WordClock ESP8266</h1>";
  html += "<form action='/save' method='POST'>";
  html += "WiFi SSID: <input name='ssid' value='" + String(config.ssid) + "'><br>";
  html += "WiFi Password: <input name='password' type='password' value='" + String(config.password) + "'><br>";
  html += "NTP Server: <input name='ntpServer' value='" + String(config.ntpServer) + "'><br>";
  html += "LED Pin (GPIO): <input name='ledPin' type='number' min='0' max='16' value='" + String(config.ledPin) + "'><br>";
  html += "Brightness: <input name='brightness' type='number' min='1' max='255' value='" + String(config.brightness) + "'><br>";
  char colorHourBuf[8]; sprintf(colorHourBuf, "#%06x", config.colorHour & 0xFFFFFF);
  char colorMinuteBuf[8]; sprintf(colorMinuteBuf, "#%06x", config.colorMinute & 0xFFFFFF);
  char colorAMPMBuf[8]; sprintf(colorAMPMBuf, "#%06x", config.colorAMPM & 0xFFFFFF);
  char colorWIZBuf[8]; sprintf(colorWIZBuf, "#%06x", config.colorWIZ & 0xFFFFFF);
  html += "Hour color: <input name='colorHour' type='color' value='" + String(colorHourBuf) + "'><br>";
  html += "Minute color: <input name='colorMinute' type='color' value='" + String(colorMinuteBuf) + "'><br>";
  html += "AM/PM color: <input name='colorAMPM' type='color' value='" + String(colorAMPMBuf) + "'><br>";
  html += "TIMEBYWIZ color: <input name='colorWIZ' type='color' value='" + String(colorWIZBuf) + "'><br>";
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
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  loadConfig();

  strip.setPin(config.ledPin);
  strip.begin();
  strip.show();

  selfTestLeds(); // animation de vérification

  setupWiFi();

  timeClient.setPoolServerName(config.ntpServer);
  timeClient.begin();
  updateNtpAndDST();

  setupWebServer();
}

void loop() {
  static unsigned long lastDisplay = 0;
  updateLedBuiltin();

  if (millis() - lastDisplay > 1000) {
    if (wifiConnected) timeClient.update();
    updateNtpAndDST();
    displayTime();
    lastDisplay = millis();
  }
}