# Changelog

Format [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/), versionnage SemVer.
Les deux firmwares évoluent séparément : l'ESP8266 n'a ni OTA ni mDNS.

## Les deux firmwares — [ajouts communs]

### Ajouté
- **Mémo « Où retrouver ce projet » dans l'interface web.** Section dépliable en bas
  de la page : URL du dépôt, chemin du croquis, carte à sélectionner dans l'IDE,
  emplacement de la documentation de la lettrine, GPIO du ruban, procédure de
  remise à zéro, adresse MAC. Aucun identifiant : l'horloge peut être offerte
  telle quelle. Le bloc HTML est identique dans les deux croquis, seules les
  constantes `WC_MEMO_*` en tête de fichier diffèrent.
- **`tools/ota_json_generator_v3.0.py`** — successeur de l'outil de génération du
  JSON OTA écrit pour les premières mises à jour. Même interface graphique, même
  format de JSON, plus trois ajouts : la version annoncée est vérifiée contre
  celle compilée dans le croquis, le binaire est renommé avec son numéro, et
  l'URL GitHub Releases est construite automatiquement. La version d'origine est
  conservée dans `archive/`.
- **Numéro de version sur l'ESP8266** (`FIRMWARE_VERSION`, 18.1.0). Il n'y a pas
  d'OTA sur cette cible : il sert au mémo et au tag git correspondant.

### Corrigé
- **Le mot de passe WiFi était exposé en clair dans la page de configuration.**
  Le champ était pré-rempli avec `value='...'` ; `type='password'` ne masque que
  l'affichage, la valeur restait lisible dans le code source de la page, visible
  par quiconque sur le réseau local. Le champ n'est plus pré-rempli, et un champ
  laissé vide conserve le mot de passe enregistré.
- **`strncpy` sans terminaison garantie** sur `ssid` et `password` : une valeur de
  31 caractères exactement laissait la chaîne non terminée.

## ESP32 — [2.0.1]

Corrections seules. Aucun changement de structure : le diff avec la 2.0.0
(conservée dans `archive/`) fait 20 lignes, commentaires compris.

### Corrigé
- **Minutes 58 et 59 bloquées sur « moins cinq ».** L'arrondi au multiple de 5 le
  plus proche donne 60, c'est-à-dire l'heure pleine suivante ; le clamp à 11
  ramenait cette valeur sur 55. « Moins cinq » restait affiché 7 minutes et
  l'heure pleine seulement 3. Chaque tranche dure maintenant exactement 5 minutes.
- **MD5 du firmware jamais vérifié.** La somme annoncée dans `latest.json` était
  lue puis ignorée : `Update.setMD5()` n'était appelé nulle part. Un
  téléchargement corrompu était accepté. Elle est maintenant vérifiée, et une
  mise à jour corrompue est refusée sans toucher au firmware en place.
- **TIMEBYWIZ déclenché sur l'heure décalée** plutôt que sur l'heure réelle. Sans
  effet observable (le test porte sur `m == 0`, moment où les deux coïncident),
  aligné sur l'ESP8266 pour que `displayTime()` soit strictement identique dans les
  deux croquis.
- **URL de mise à jour par défaut injoignable.** Elle pointait sur
  `wizhard2006/WiZ`, qui répond 404 : les assets d'un dépôt privé exigent un
  jeton. Elle pointe désormais sur le dépôt public, via `releases/latest` qui suit
  automatiquement la dernière publication.

## ESP8266 — [v18 corrigé]

### Corrigé
- **AM/PM affiché à l'envers 54 minutes par jour.** Les pastilles étaient
  calculées sur l'heure prononcée, déjà incrémentée pour dire « moins … », au lieu
  de l'heure réelle. De 11 h 33 à 11 h 59 l'horloge affichait PM en pleine matinée,
  de 23 h 33 à 23 h 59 elle affichait AM en pleine soirée. La correction existait
  déjà côté ESP32 depuis la version 7, elle n'était jamais remontée ici.
- **Minutes 58 et 59 bloquées sur « moins cinq »**, même défaut que sur ESP32.
- **TIMEBYWIZ déclenché sur l'heure décalée** plutôt que sur l'heure réelle. Sans
  effet observable (le test porte sur `m == 0`, moment où les deux coïncident),
  corrigé par cohérence avec AM/PM.
- **Reset usine sans effet.** La détection d'EEPROM vierge comparait
  `config.ssid[0]` à `0xFF` ; sur Xtensa `char` est signé, un octet `0xFF` vaut
  `-1`, et la comparaison était toujours fausse. Après un reset usine la carte
  repartait avec une luminosité, un GPIO et des couleurs aléatoires.
- **Include `<ArduinoJson.h>` supprimé.** La bibliothèque n'était utilisée nulle
  part dans ce croquis (la configuration est en EEPROM). L'include restait un
  vestige et exposait la compilation à des conflits de bibliothèques.

## Antérieur

- **ESP32 2.0.0** — mise à jour OTA depuis la WebUI, machine d'état non bloquante,
  contrôle de version par `latest.json` distant. Conservée dans `archive/`.
- **ESP32 v7** — LittleFS, mDNS, correction AM/PM. Conservée dans `archive/`.
- **ESP8266 v18** — première version complète : EEPROM, WebUI, point d'accès de
  secours, test des LED au démarrage. Conservée dans `archive/`.
