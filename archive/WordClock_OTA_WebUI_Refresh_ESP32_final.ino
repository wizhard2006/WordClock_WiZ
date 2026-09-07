/*
 * WordClock ESP32 – Version LittleFS avec mDNS et OTA auto (http://wordclock.local)
 * OTA robuste et 100% compatible ESPAsyncWebServer/ESP32-IDF v5+ (non bloquant, watchdog toujours actif).
 * © 2025
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
 * - Reset usine, reboot, et MAJ firmware via WebUI
 * - mDNS activé : accès facile par nom réseau (plus besoin de connaître l'IP !)
 * - Mise à jour auto du firmware via bouton "Vérifier MAJ" (OTA HTTP, non bloquant)
 * - Contrôle de version firmware (pas de download si dernière version déjà installée)
 * - URL de MAJ firmware configurable via WebUI
 *
 * Librairies requises :
 * - Adafruit_NeoPixel
 * - ESPAsyncWebServer (+ AsyncTCP)
 * - NTPClient, WiFiUdp
 * - ArduinoJson
 * - LittleFS
 * - ESPmDNS (inclus dans l'ESP32 Arduino Core)
 * - HTTPClient (pour OTA)
 * - Update (pour OTA)
 *
 * Important :
 * - La LED intégrée bleue (GPIO2, LED_BUILTIN) s'allume avec digitalWrite(HIGH), s'éteint avec digitalWrite(LOW) sur ESP32 (logique NORMALE)
 * - La variable FIRMWARE_VERSION est générée automatiquement à chaque build
 * - Le contrôle de version évite les téléchargements inutiles (compare version locale vs. distante)
 * - Le firmware distant doit fournir un JSON à l'URL spécifiée (voir documentation plus bas)
 *
 * Fichier JSON distant attendu :
 * {
 *   "version": "1.5.0",
 *   "bin_url": "https://tonsite.com/wordclock/wordclock_v1.5.0.bin",
 *   "md5": "xxxx" // optionnel mais recommandé
 * }
 *
 * Si la version distante est supérieure à la version locale, le bouton "Mettre à jour" devient actif.
 *
 * Génération automatique de la version :
 * - Utilise __DATE__ et __TIME__ pour indiquer la date/heure de build
 * - Le champ "FIRMWARE_VERSION" s'affiche dans la WebUI
 *
 * Auteur : wizhard2006 + Copilot
 */

// ================== INCLUDES & DEFINITIONS ==================
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
#include <HTTPClient.h>
#include <Update.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Version automatique (build date + option manuelle)
#define WC_FW_VERSION_MAJOR 2
#define WC_FW_VERSION_MINOR 0
#define WC_FW_VERSION_PATCH 0
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FIRMWARE_VERSION  TOSTRING(WC_FW_VERSION_MAJOR) "." TOSTRING(WC_FW_VERSION_MINOR) "." TOSTRING(WC_FW_VERSION_PATCH) " (" __DATE__ " " __TIME__ ")"

// URL de MAJ firmware par défaut (modifiable via WebUI)
#define DEFAULT_FW_UPDATE_URL "https://github.com/wizhard2006/WiZ/releases/download/Last_Update_WiZ/latest.json"

#define DEFAULT_LED_PIN    4
#define NUM_LEDS           104
#define DEFAULT_BRIGHTNESS 32
#define CONFIG_PATH        "/config.json"
#define MDNS_HOSTNAME      "wordclock"

// ================== AJOUT pour résultat OTA persistant ==================
// On affiche le résultat d'une MAJ OTA (succès ou erreur) au prochain démarrage, grâce à ce fichier
#define OTA_RESULT_PATH "/ota_result.txt"
String persistentOtaMsg = ""; // Contenu du message à afficher au prochain boot (après MAJ OTA)

void saveOtaResult(const String& msg) {
  File f = LittleFS.open(OTA_RESULT_PATH, "w");
  if (f) { f.print(msg); f.close(); }
}

void loadOtaResult() {
  persistentOtaMsg = "";
  if (LittleFS.exists(OTA_RESULT_PATH)) {
    File f = LittleFS.open(OTA_RESULT_PATH, "r");
    if (f) { persistentOtaMsg = f.readString(); f.close(); }
    LittleFS.remove(OTA_RESULT_PATH);
  }
}
// ================== FIN AJOUT persistance résultat OTA ==================

Adafruit_NeoPixel strip(NUM_LEDS, DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);

// ================== STRUCTURE CONFIGURATION PERSISTANTE ==================
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
  char fwUpdateURL[128];
} config;

// ================== MAPPING LEDS (heures/minutes) fourni par l'utilisateur ==================
const uint8_t led_heures[13][16] = {
  {33,34,35,36,37,38, 255},
  {0,1,2,52,53,54,55,56, 255},
  {5,6,7,8,52,53,54,55,56,57, 255},
  {26,27,28,29,30,52,53,54,55,56,57, 255},
  {17,18,19,20,21,22,52,53,54,55,56,57, 255},
  {22,23,24,25,52,53,54,55,56,57, 255},
  {30,31,32,52,53,54,55,56,57, 255},
  {9,10,11,12,52,53,54,55,56,57, 255},
  {13,14,15,16,52,53,54,55,56,57, 255},
  {1,2,3,4,52,53,54,55,56,57, 255},
  {43,44,45,52,53,54,55,56,57, 255},
  {48,49,50,51,52,53,54,55,56,57, 255},
  {44,45,46,47, 255}
};
const uint8_t led_minutes[12][16] = {
  {255},
  {65,66,67,68, 255},
  {78,79,80, 255},
  {75,76,81,82,83,84,85, 255},
  {70,71,72,73,74, 255},
  {65,66,67,68,69,70,71,72,73,74, 255},
  {75,76,86,87,88,89,90, 255},
  {59,60,61,62,63,65,66,67,68,69,70,71,72,73,74, 255},
  {59,60,61,62,63,70,71,72,73,74, 255},
  {59,60,61,62,63,76,77,81,82,83,84,85, 255},
  {59,60,61,62,63,78,79,80, 255},
  {59,60,61,62,63,65,66,67,68, 255}
};
const uint8_t led_timebywiz[9] = {95,96,97,98,99,100,101,102,103};
const uint8_t led_am[2] = {93,94};
const uint8_t led_pm[4] = {91,92,93,94};

enum LedState { WIFI_SEARCH, WIFI_ERROR_AP, WIFI_CONNECTED, LED_OFF };
LedState ledState = WIFI_SEARCH;
unsigned long lastLedToggle = 0;
bool ledBuiltinStatus = false;

// ================== LITTLEFS (PERSISTANCE CONFIG) ==================
void loadConfig() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }
  if (LittleFS.exists(CONFIG_PATH)) {
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (f) {
      StaticJsonDocument<1024> doc;
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
        strlcpy(config.fwUpdateURL, doc["fwUpdateURL"] | DEFAULT_FW_UPDATE_URL, sizeof(config.fwUpdateURL));
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
  config.ledBuiltinOn = true;
  strcpy(config.fwUpdateURL, DEFAULT_FW_UPDATE_URL);
  saveConfig();
}
void saveConfig() {
  StaticJsonDocument<1024> doc;
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
  doc["fwUpdateURL"] = config.fwUpdateURL;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}
void resetConfig() {
  LittleFS.remove(CONFIG_PATH);
}

// ================== NTP & TEMPS ==================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // UTC+1 (hiver), ajusté dynamiquement

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

// ================== WIFI & AP ==================
AsyncWebServer server(80);
bool wifiConnected = false;
String myIP = "";
bool mdnsStarted = false;

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

// ================== ANIMATION SELFTEST (arc-en-ciel ~5 secondes) ==================
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

// ================== LED INTÉGRÉE BLEUE (GPIO2) MANAGEMENT ==================
void updateLedBuiltin() {
  int builtinPin = LED_BUILTIN;
  if (!config.ledBuiltinOn) {
    digitalWrite(builtinPin, LOW);
    return;
  }
  unsigned long now = millis();
  switch (ledState) {
    case WIFI_SEARCH:
      if (now - lastLedToggle > 500) {
        ledBuiltinStatus = !ledBuiltinStatus;
        digitalWrite(builtinPin, ledBuiltinStatus ? HIGH : LOW);
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
      digitalWrite(builtinPin, HIGH); // LED allumée
      break;
    case LED_OFF:
      digitalWrite(builtinPin, LOW); // LED éteinte
      break;
  }
}

// ================== AFFICHAGE PRINCIPAL HORLOGE ==================
void clearAllLeds() {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0, 0, 0);
}
void setLeds(const uint8_t *arr, uint32_t color) {
  for (int i = 0; i < 16 && arr[i] != 255; i++)
    strip.setPixelColor(arr[i], color);
}
void displayTime() {
  clearAllLeds();

  int h_real = timeClient.getHours();
  int h = h_real;
  int m = timeClient.getMinutes();
  //---> correction m > 32 car c'est a 32 que les minutes changent. synchronisation du changement de l'heure avec les minutes
  if (m > 32) {
    h = (h + 1) % 24;
  }

  int hIdx = h % 12;
  if (h == 0) hIdx = 0;
  if (h == 12) hIdx = 12;
  setLeds(led_heures[hIdx], config.colorHour);

  int minIdx = (m == 0) ? 0 : ((m + 2) / 5);
  if (minIdx > 11) minIdx = 11;
  if (m != 0) setLeds(led_minutes[minIdx], config.colorMinute);

  if (h_real < 12) setLeds(led_am, config.colorAMPM);
  else             setLeds(led_pm, config.colorAMPM);

  if (h == 21 && m == 0) {
    setLeds(led_timebywiz, config.colorWIZ);
  } else {
    for (int i = 0; i < 9; i++) strip.setPixelColor(led_timebywiz[i], 0, 0, 0);
  }

  strip.setBrightness(config.brightness);
  strip.show();
}

// ================== OTA AUTOMATIQUE VIA HTTP (NON BLOQUANT, WATCHDOG SAFE) ==================
// Cette implémentation découpe le téléchargement OTA en étapes courtes, sans bloquer la loop() (et donc sans jamais déclencher le watchdog).
// Aucune désactivation du watchdog logiciel !
// L'état du téléchargement et la progression sont visibles dans la WebUI.
// Le résultat (succès ou erreur) est affiché à la prochaine connexion à la WebUI après redémarrage (voir gestion fichier /ota_result.txt).

struct OtaCheckResult {
  String version;
  String bin_url;
  String md5;
  bool updateAvailable;
  String message;
} otaCheck;

// Machine d'état OTA non bloquante
enum OtaState {
  OTA_IDLE,
  OTA_INIT,
  OTA_HTTP_BEGIN,
  OTA_HTTP_GET,
  OTA_UPDATE_STREAM,
  OTA_UPDATE_FINISH,
  OTA_DONE,
  OTA_ERROR
};
struct OtaAsync {
  OtaState state = OTA_IDLE;
  HTTPClient http; // Non copiable/non assignable !
  WiFiClient* stream = nullptr;
  int contentLength = 0;
  size_t written = 0;
  unsigned long lastStep = 0;
  String msg;
  bool result = false;
};

OtaAsync otaAsync;

// Version parsing/check inchangé
uint32_t parseVersion(String v) {
  int a = 0, b = 0, c = 0;
  sscanf(v.c_str(), "%d.%d.%d", &a, &b, &c);
  return a * 10000 + b * 100 + c;
}
uint32_t getLocalVersionInt() {
  return WC_FW_VERSION_MAJOR * 10000 + WC_FW_VERSION_MINOR * 100 + WC_FW_VERSION_PATCH;
}

// Vérifie si une MAJ OTA est disponible (JSON dist)
bool checkForUpdate(String &infoMsg) {
  otaCheck = OtaCheckResult();
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  Serial.print("Vérification MAJ OTA sur : ");
  Serial.println(config.fwUpdateURL);
  http.begin(config.fwUpdateURL);
  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (err) {
      infoMsg = "Erreur parsing JSON OTA";
      http.end();
      return false;
    }
    otaCheck.version = doc["version"].as<String>();
    otaCheck.bin_url = doc["bin_url"].as<String>();
    otaCheck.md5 = doc["md5"].as<String>();
    uint32_t remoteVer = parseVersion(otaCheck.version);
    uint32_t localVer = getLocalVersionInt();

    Serial.printf("Version distante : %s (%u)\n", otaCheck.version.c_str(), remoteVer);
    Serial.printf("Version locale : %s (%u)\n", FIRMWARE_VERSION, localVer);

    if (remoteVer > localVer) {
      otaCheck.updateAvailable = true;
      infoMsg = "Nouvelle version disponible : " + otaCheck.version;
      http.end();
      return true;
    } else {
      otaCheck.updateAvailable = false;
      infoMsg = "Aucune mise à jour disponible (déjà à jour)";
      http.end();
      return false;
    }
  } else {
    infoMsg = "Erreur HTTP lors de la requête OTA : " + String(code);
    http.end();
    return false;
  }
}

// ----------- OTA non bloquant : machine d'état pilotée dans loop() -----------
bool startOtaAsync(String &resultMsg) {
  if (!otaCheck.updateAvailable || otaCheck.bin_url == "") {
    resultMsg = "Aucune MAJ disponible ou URL du .bin manquante";
    return false;
  }
  if (otaAsync.state != OTA_IDLE) {
    resultMsg = "OTA déjà en cours";
    return false;
  }
  // Réinitialise tous les champs sauf http (qui reste inchangé car non assignable)
  otaAsync.state = OTA_INIT;
  otaAsync.stream = nullptr;
  otaAsync.contentLength = 0;
  otaAsync.written = 0;
  otaAsync.lastStep = millis();
  otaAsync.msg = "";
  otaAsync.result = false;

  resultMsg = "Démarrage de la mise à jour OTA non bloquante...";
  return true;
}

// À appeler régulièrement dans loop() pour faire avancer l'OTA (jamais bloquant, watchdog safe)
void handleOtaAsync() {
  static unsigned long otaStepTimeout = 0;
  if (otaAsync.state == OTA_IDLE || otaAsync.state == OTA_DONE || otaAsync.state == OTA_ERROR) return;

  switch (otaAsync.state) {
    case OTA_INIT:
      Serial.println("[OTA] Début du process OTA...");
      otaAsync.http.setTimeout(25000);
      otaAsync.http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      otaAsync.state = OTA_HTTP_BEGIN;
      otaAsync.lastStep = millis();
      break;

    case OTA_HTTP_BEGIN:
      if (otaAsync.http.begin(otaCheck.bin_url)) {
        otaAsync.state = OTA_HTTP_GET;
        otaAsync.lastStep = millis();
      } else {
        otaAsync.msg = "Echec de http.begin()";
        // Persiste l'erreur pour affichage au prochain reboot
        saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
        otaAsync.state = OTA_ERROR;
      }
      break;

    case OTA_HTTP_GET:
      {
        int code = otaAsync.http.GET();
        Serial.print("[OTA] Code HTTP: "); Serial.println(code);
        if (code == 200) {
          otaAsync.contentLength = otaAsync.http.getSize();
          Serial.print("[OTA] Taille (Content-Length) : ");
          Serial.println(otaAsync.contentLength);
          bool canBegin = (otaAsync.contentLength > 0) ? Update.begin(otaAsync.contentLength) : Update.begin();
          if (!canBegin) {
            otaAsync.msg = "Erreur Initialisation Update";
            otaAsync.http.end();
            saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
            otaAsync.state = OTA_ERROR;
            break;
          }
          otaAsync.stream = (WiFiClient*)&otaAsync.http.getStream();
          otaAsync.state = OTA_UPDATE_STREAM;
          otaAsync.lastStep = millis();
          otaStepTimeout = millis();
        } else {
          otaAsync.msg = "Erreur HTTP lors du téléchargement .bin : " + String(code);
          otaAsync.http.end();
          saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
          otaAsync.state = OTA_ERROR;
        }
      }
      break;

    case OTA_UPDATE_STREAM:
      {
        // Télécharge le binaire par petits morceaux, jamais bloquant
        uint8_t buf[512];
        size_t len = otaAsync.stream->available();
        if (len) {
          if (len > sizeof(buf)) len = sizeof(buf);
          int c = otaAsync.stream->readBytes(buf, len);
          if (c > 0) {
            if (Update.write(buf, c) != c) {
              otaAsync.msg = "Erreur écriture Update";
              otaAsync.http.end(); Update.end();
              saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
              otaAsync.state = OTA_ERROR;
              break;
            }
            otaAsync.written += c;
            otaStepTimeout = millis();
          }
        }
        // timeout 30s sans données reçues
        if (millis() - otaStepTimeout > 30000) {
          otaAsync.msg = "Timeout téléchargement OTA";
          otaAsync.http.end(); Update.end();
          saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
          otaAsync.state = OTA_ERROR;
          break;
        }
        // Terminé si tout écrit
        if ((otaAsync.contentLength > 0 && otaAsync.written == (size_t)otaAsync.contentLength) ||
            (otaAsync.contentLength == -1 && !otaAsync.stream->available() && !otaAsync.stream->connected())) {
          otaAsync.state = OTA_UPDATE_FINISH;
          otaAsync.lastStep = millis();
        }
      }
      break;

    case OTA_UPDATE_FINISH:
      otaAsync.http.end();
      if (Update.end()) {
        if (Update.isFinished()) {
          otaAsync.msg = "MAJ OTA réussie, redémarrage imminent...";
          otaAsync.result = true;
          otaAsync.state = OTA_DONE;
          // Persiste le succès pour affichage au prochain reboot !
          saveOtaResult("Mise à jour réussie ! (" + otaCheck.version + ")");
          delay(1000); // pour affichage éventuel...
          ESP.restart();
        } else {
          otaAsync.msg = "MAJ OTA incomplète";
          saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
          otaAsync.state = OTA_ERROR;
        }
      } else {
        otaAsync.msg = "MAJ OTA erreur : " + String(Update.getError());
        saveOtaResult("Erreur mise à jour OTA : " + otaAsync.msg);
        otaAsync.state = OTA_ERROR;
      }
      break;

    case OTA_ERROR:
      // Rien à faire, le message d'erreur est déjà prêt dans otaAsync.msg et persiste
      break;
    default:
      break;
  }
}

// ================== WEBUI (INCLUANT OTA) ==================
// Réagencement ergonomique : 
// - Le pavé configuration reste en haut (comme sur ta capture)
// - La zone OTA affiche la version courante, le résultat de MAJ OTA (succès/erreur) juste dessous, puis les boutons OTA
// - Les boutons reboot et factory reset sont sous la zone OTA, bien séparés

String htmlHead() {
  return "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
         "<title>WordClock ESP32</title>"
         "<style>body{font-family:sans-serif;background:#222;color:#eee;margin:0;padding:0;}h1{background:#444;padding:10px;}form{margin:15px;padding:10px;background:#333;border-radius:6px;}input,select{margin:5px;}button{margin:5px;padding:8px 16px;font-size:1em;}</style>"
         "</head><body>";
}
String lastOtaMsg = ""; // Dernier message OTA à afficher sur la page

void handleRoot(AsyncWebServerRequest *request) {
  String html = htmlHead();
  html += "<h1>WordClock ESP32</h1>";

  // Bloc configuration principal (inchangé, comme sur ta capture)
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
  html += "Disable internal LED: <input name='ledBuiltinOn' type='checkbox' ";
  if (!config.ledBuiltinOn) html += "checked";
  html += "><br>";
  html += "Firmware update URL (JSON): <input name='fwUpdateURL' type='text' size='50' value='" + String(config.fwUpdateURL) + "'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";

  // ----- Section OTA -----
  html += "<hr><div style='margin-bottom:16px;'>";
  html += "<h2>Firmware OTA Update</h2>";
  html += "<b>Firmware version :</b> " + String(FIRMWARE_VERSION) + "<br>";

  // Affiche la version disponible à distance si trouvée
  if (otaCheck.updateAvailable && otaCheck.version.length() > 0) {
    html += "<div style='color:#3cf;margin-bottom:4px;'>Nouvelle version disponible : <b>" + otaCheck.version + "</b></div>";
  }

  // Ajout : message de résultat de MAJ OTA juste sous la version
  if (persistentOtaMsg.length() > 0)
    html += "<div style='color:#6f6;background:#111;padding:10px;font-weight:bold;margin-bottom:5px;'>" + persistentOtaMsg + "</div>";

  // Boutons OTA
  html += "<form action='/checkota' method='POST' style='display:inline;'><button>Vérifier les mises à jour</button></form> ";
  if (otaCheck.updateAvailable) {
    if (otaAsync.state == OTA_IDLE || otaAsync.state == OTA_DONE || otaAsync.state == OTA_ERROR) {
      html += "<form action='/doota' method='POST' style='display:inline;'><button style='background:#468C46;color:white;font-weight:bold;'>Mettre à jour maintenant</button></form>";
    }
  }
  // Affiche le message de lancement de MAJ, ou d'erreur de lancement (lastOtaMsg)
  if (lastOtaMsg.length() > 0 &&
      otaAsync.state != OTA_UPDATE_STREAM &&
      otaAsync.state != OTA_ERROR &&
      otaAsync.state != OTA_DONE) {
      html += "<div style='color:#ff8;font-weight:bold;margin:8px 0;'>" + lastOtaMsg + "</div>";
  }

  // Affichage de l'état/progression OTA (en live si on recharge la page)
  // Correction : le script de rafraichissement automatique est injecté dans TOUT état où l'OTA est en cours (pas seulement UPDATE_STREAM)
  if (otaAsync.state == OTA_UPDATE_STREAM ||
      otaAsync.state == OTA_INIT ||
      otaAsync.state == OTA_HTTP_BEGIN ||
      otaAsync.state == OTA_HTTP_GET) {
    html += "<div style='color:#fa0;'><b>Mise à jour OTA en cours…</b>";
    if (otaAsync.written > 0 || otaAsync.contentLength > 0)
      html += " Reçu: " + String(otaAsync.written) + " / " + String(otaAsync.contentLength) + " octets";
    html += "</div>";
    // Le script ci-dessous force le rafraichissement automatique de la page toutes les secondes, dès le début de la MAJ OTA
    html += "<script>setTimeout(function(){window.location.reload();},1000);</script>";
  } else if (otaAsync.state == OTA_ERROR) {
    html += "<div style='color:#f66;'><b>Erreur OTA :</b> " + otaAsync.msg + "</div>";
  } else if (otaAsync.state == OTA_DONE) {
    html += "<div style='color:#6f6;'><b>Mise à jour terminée. Redémarrage...</b></div>";
  }
  html += "<div style='color:#ccc;font-style:italic;'>Après avoir lancé la mise à jour, patientez et rechargez la page.<br>Le résultat de la MAJ OTA (succès ou erreur) s'affichera ici après redémarrage.</div>";
  html += "</div>";

  // ----- Section administration : reboot et reset -----
  html += "<div style='margin-bottom:20px;'>";
  html += "<form action='/reboot' method='POST' style='display:inline;'><button>Reboot</button></form> ";
  html += "<form action='/reset' method='POST' style='display:inline;'><button style='background:#b44;color:white;'>Factory Reset</button></form>";
  html += "</div>";

  // ----- Infos diverses -----
  html += "<hr><b>Time (NTP):</b> " + String(timeClient.getFormattedTime()) + "<br>";
  html += "<b>WiFi Status:</b> " + String(wifiConnected ? "Connected" : "AP Config") + "<br>";
  html += "<b>IP Address:</b> " + myIP + "<br>";
  html += "<b>mDNS:</b> ";
  if (wifiConnected && mdnsStarted) html += "Access via <b>http://" MDNS_HOSTNAME ".local</b><br>";
  else html += "Unavailable in AP mode<br>";
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
  config.ledBuiltinOn = !request->hasParam("ledBuiltinOn", true);
  if (request->hasParam("fwUpdateURL", true)) strncpy(config.fwUpdateURL, request->getParam("fwUpdateURL", true)->value().c_str(), 127);
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
void handleCheckOta(AsyncWebServerRequest *request) {
  String info;
  if (checkForUpdate(info)) {
    lastOtaMsg = "Nouvelle version disponible : " + otaCheck.version;
  } else {
    lastOtaMsg = info;
  }
  request->redirect("/");
}
void handleDoOta(AsyncWebServerRequest *request) {
  String result;
  if (startOtaAsync(result)) {
    lastOtaMsg = "Mise à jour OTA lancée…";
  } else {
    lastOtaMsg = "Erreur MAJ : " + result;
  }
  request->redirect("/");
}
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/checkota", HTTP_POST, handleCheckOta);
  server.on("/doota", HTTP_POST, handleDoOta);
  server.begin();
}

// ================== SETUP & LOOP ==================
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  loadConfig();
  loadOtaResult(); // <-- Affiche le résultat de la dernière MAJ OTA si présent

  strip.setPin(config.ledPin);
  strip.begin();
  strip.show();

  selfTestLeds();

  setupWiFi();

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
    setupWebServer();
  }
}

void loop() {
  static unsigned long lastDisplay = 0;
  updateLedBuiltin();

  if (wifiConnected && millis() - lastDisplay > 1000) {
    timeClient.update();
    updateNtpAndDST();
    displayTime();
    lastDisplay = millis();
  } else if (!wifiConnected && millis() - lastDisplay > 1000) {
    displayTime();
    lastDisplay = millis();
  }

  // Appel non bloquant du process OTA (ne bloque jamais, watchdog OK)
  handleOtaAsync();
}