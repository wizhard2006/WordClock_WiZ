# WordClock

Horloge à mots française : 104 LED WS2812B derrière une lettrine imprimée en 3D,
pilotées par un ESP8266 ou un ESP32. L'heure est lue en toutes lettres, arrondie
à cinq minutes, et se synchronise par NTP.

> *DIX HEURES ET DEMIE* … *ONZE HEURES MOINS VINGT-CINQ* … *MIDI*

## État du projet

Deux firmwares indépendants, tous deux fonctionnels sur le matériel.

| | ESP32 | ESP8266 |
|---|---|---|
| Croquis | `firmware/esp32/WordClock_ESP32/` | `firmware/esp8266/WordClock_ESP8266/` |
| Version | 2.0.1 | v18 corrigé |
| Configuration | LittleFS `/config.json` | EEPROM |
| mDNS `wordclock.local` | oui | non |
| Mise à jour OTA par la WebUI | oui | non |
| Interface web, couleurs, luminosité, GPIO | oui | oui |
| Point d'accès de secours | oui | oui |

L'ESP32 est la branche la plus avancée. L'ESP8266 est la version historique,
fonctionnelle et corrigée, sans OTA ni mDNS.

**Les deux croquis ne sont pas équivalents** : l'écart de 450 lignes vient
entièrement des fonctions réseau de l'ESP32 (OTA, mDNS, LittleFS). En revanche, la
partie horloge est commune et vérifiée comme telle :

| Élément | État |
|---|---|
| `led_heures`, `led_minutes`, `led_am`, `led_pm`, `led_timebywiz` | identiques, valeur par valeur |
| `displayTime()` | strictement identique, 24 lignes de code |
| `clearAllLeds()`, `setLeds()`, `selfTestLeds()` | identiques |
| `isSummerTime_FR()`, `updateNtpAndDST()` | identiques |
| `updateLedBuiltin()` | polarité inversée — la LED de la carte est active à l'état bas sur D1 mini, haut sur ESP32 |

Comparaison des 1440 minutes de la journée, un croquis contre l'autre : **zéro
minute d'écart** sur la phrase affichée, les pastilles AM/PM et TIMEBYWIZ.

Ce qui diffère au-delà : voir le tableau ci-dessus et `docs/AUDIT.md`.

## Matériel

| Élément | Détail |
|---|---|
| Carte | ESP32 DevKit v1 **ou** Wemos D1 mini (ESP8266) |
| LED | ruban WS2812B, 104 LED, 8 bandes de 13 |
| Alimentation | 5 V, 4 A minimum, injectée sur le ruban et non via l'USB |
| Condensateur | 1000 µF entre +5 V et GND, au plus près de la première LED |
| Résistance | 330 à 470 Ω en série sur la ligne de données |
| Ruban | GPIO 4 par défaut (D2 sur D1 mini), configurable dans la WebUI |

Les 8 bandes sont câblées en **serpentin** : LED 0 en haut à gauche, la ligne
suivante repart dans l'autre sens. La grille complète et la correspondance
mot → LED sont dans [`docs/LETTRINE.md`](docs/LETTRINE.md). **C'est le document de
référence du projet** : il n'existait que dans un tableur, il est maintenant
versionné avec le code.

## Compilation

Croquis Arduino d'un seul fichier. Le dossier et le `.ino` doivent porter le même
nom, c'est une contrainte de l'IDE.

| | ESP32 | ESP8266 |
|---|---|---|
| Carte | ESP32 Dev Module | LOLIN(WEMOS) D1 R2 & mini |
| Bibliothèques | Adafruit NeoPixel, AsyncTCP, ESPAsyncWebServer, NTPClient, ArduinoJson 7 | Adafruit NeoPixel, ESPAsyncTCP, ESPAsyncWebServer, NTPClient |

L'ESP8266 n'utilise **aucune bibliothèque JSON** : sa configuration est en EEPROM.

Occupation actuelle de l'ESP32 : 92 % de la zone applicative. Ça passe et l'OTA
fonctionne (deux partitions de même taille), mais la marge est faible. Si une
compilation échoue faute de place : *Outils > Partition Scheme > Minimal SPIFFS
(1.9MB APP with OTA/190KB SPIFFS)*. Attention, changer le schéma de partition
efface le système de fichiers, donc la configuration WiFi.

## Premier démarrage

1. Flasher, brancher le ruban sur le GPIO 4.
2. L'arc-en-ciel de test défile pendant 5 secondes.
3. La LED de la carte clignote vite : se connecter au WiFi `WordClock_Config`.
4. Ouvrir `http://192.168.4.1`, saisir le SSID et le mot de passe, enregistrer.
5. La carte redémarre et rejoint le réseau. Sur ESP32 : `http://wordclock.local`.

## Versionnage

Le numéro de version n'est pas dans le nom des fichiers — l'IDE Arduino impose
dossier == nom du croquis, et chaque renommage cassait les chemins. C'est ce qui
avait produit `v18`, `Version7_sansOTA_Final` et `OTA_..._final`, trois
conventions dont aucune ne disait laquelle était la plus récente.

| Niveau | Porteur |
|---|---|
| Firmware ESP32 | `WC_FW_VERSION_MAJOR/MINOR/PATCH` en tête du croquis |
| Dépôt | tag git annoté, `v2.0.1` |
| Publication | release GitHub, binaire `WordClock_ESP32_2.0.1.bin` |

Procédure de publication : [`docs/OTA.md`](docs/OTA.md).

## Arborescence

```
docs/       lettrine (référence), audit des bugs trouvés, procédure OTA
firmware/   les deux croquis Arduino
hardware/   modèle Fusion 360, STL, schéma de câblage
tools/      generateur du JSON OTA (script Python + recette PyInstaller)
archive/    versions d'origine avant correction, non maintenues, jamais compilées
```

`archive/` contient les fichiers exactement tels qu'ils étaient avant les
correctifs. En cas de doute sur une régression, la comparaison est immédiate :

```bash
diff archive/WordClock_ESP8266_v18.ino firmware/esp8266/WordClock_ESP8266/WordClock_ESP8266.ino
```

## Licence

| Quoi | Licence |
|---|---|
| `firmware/`, `tools/`, `docs/` | MIT — [`LICENSE`](LICENSE) |
| `hardware/` | CC BY-NC-SA 4.0 — [`hardware/LICENSE`](hardware/LICENSE) |

Répartition habituelle des projets makers : le code se reprend librement, la
lettrine s'imprime et se modifie en citant l'auteur mais ne se vend pas.
