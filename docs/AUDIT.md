# Audit de l'archive de travail

État des lieux fait sur `WordClock.zip` (16 fichiers, 235 Mo décompressés), avant
constitution de ce dépôt. Conservé comme trace : il explique pourquoi le dépôt est
organisé ainsi et ce qui a été écarté.

## Filiation des versions

```
WordClock par IA V3.txt  (cahier des charges d'origine)
        │
        ▼
ESP8266 v18 ──────────► ESP32 v7 (LittleFS + mDNS) ──────────► ESP32 2.0.0 (OTA)
  EEPROM                  correction AM/PM                       machine d'état OTA
  bug AM/PM               services réseau après WiFi             versionning firmware
                                                                 − commentaires perdus
```

Vérifié ligne à ligne : la 2.0.0 est un **sur-ensemble fonctionnel strict** de la
v7, zéro régression de code, seules 5 lignes HTML reformatées. La v7 est donc
archivée sans perte.

En revanche la 2.0.0 a **perdu 30+ lignes de commentaires** de la v7 : tous les
bandeaux de section, et surtout celui-ci, qui documentait une correction :

```
// --- AM/PM : affichage basé sur l'heure réelle, et non l'heure "wordclock" ---
// Cela évite d'afficher le point AM avant minuit réel (ex: "minuit moins vingt-cinq" à 23h33)
```

Le code était bon, la mémoire du *pourquoi* avait disparu — et la correction n'est
jamais remontée vers l'ESP8266.

## Écarté de l'archive

| Fichier | Raison |
|---|---|
| `Word Clock ... grille v8.f3z` | doublon exact du v9, même MD5 `691a7e4c05ad3040ddc0a4416d533a69` |
| `build/esp32.esp32.esp32/*` | 42 Mo d'artefacts de compilation, régénérables |
| `build_=_BIN_pour_depot_github_OTA.txt` | fichier vide |
| `leds_Wiring-Diagram.png` | schéma générique Adafruit sur Arduino UNO, conservé faute de mieux mais à refaire |

Point positif relevé : `latest.json` et le `.bin` livrés étaient **cohérents**,
MD5 `fd3c95be…` vérifié.

## Défauts trouvés et corrigés

Détail dans `CHANGELOG.md`. Résumé :

| # | Cible | Défaut | Effet observable |
|---|---|---|---|
| 1 | ESP8266 | AM/PM calculé sur l'heure décalée | pastilles inversées 54 min/jour |
| 2 | les deux | minutes 58-59 ramenées sur « moins cinq » | tranche de 7 min au lieu de 5 |
| 3 | ESP8266 | `0xFF` comparé à un `char` signé | reset usine sans effet |
| 4 | ESP32 | MD5 lu mais jamais vérifié | mise à jour corrompue acceptée |
| 5 | ESP32 | URL OTA sur un dépôt privé | erreur 404 à chaque vérification |

Le bug que tu soupçonnais — l'heure qui avance dès 10 h 30 alors que l'affichage
dit encore « et demie » — **était déjà corrigé** dans le v18, par le seuil
`m > 32` et son commentaire. Simulation sur les 1440 minutes : la bascule se fait
bien entre 10:32 et 10:33.

## État du dépôt GitHub avant reprise

Inspection de `wizhard2006/WordClock_WiZ` : dépôt **public**, un seul fichier
(`NoReadMe`), deux tags (`v1.5.1` sans asset, `Last_Update_WiZ`), une release
`Last_Update_WiZ` portant `WordClock_OTA_WebUI_Refresh_ESP32_final.ino.bin`
(1 206 880 o) et `latest.json`.

Le binaire publié est **identique** à celui de l'archive de travail, MD5
`fd3c95bee4d750733942dcdd7dcc2688`. Rien de plus récent en ligne que ce qui était
dans l'archive : le dépôt ne contenait aucune version que nous n'ayons déjà.

Deux points relevés :

- Le dépôt s'appelait `WiZ` et a été renommé. Le champ `bin_url` du `latest.json`
  publié pointe toujours sur l'ancien nom. GitHub redirige (301), et le croquis
  ESP32 appelle bien `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)`, donc la
  mise à jour fonctionne encore. Elle cassera le jour où un dépôt nommé `WiZ`
  réapparaîtra sur ce compte.
- L'asset s'appelle `latest.json`. L'URL par défaut du croquis a été alignée sur ce
  nom plutôt que d'introduire un `latest-esp32.json` qui aurait obligé à
  reconfigurer les horloges déjà en service.

## Défauts connus, non corrigés

Écartés volontairement : ce sont des refontes, pas des corrections, et le principe
retenu est de ne pas toucher à du code qui fonctionne.

- **ESP32 — requêtes bloquantes dans les gestionnaires du serveur web.** La
  vérification de mise à jour et le redémarrage sont déclenchés depuis le contexte
  de la pile réseau asynchrone. Ça fonctionne aujourd'hui, mais c'est le genre de
  chose qui produit un reset watchdog sous charge.
- **ESP8266 — pas d'OTA ni de mDNS.** La parité avec l'ESP32 est faisable
  (LittleFS, mDNS, OTA existent sur cette plateforme), avec une contrainte réelle :
  une connexion HTTPS BearSSL coûte 6 à 8 ko de RAM à côté du serveur asynchrone.
  Chantier à part entière, à ne pas mélanger avec l'archivage.
- **ESP32 — 92 % de la zone applicative.** Fonctionne, marge faible. Voir README.
- **Adresse MAC** demandée dans le cahier des charges d'origine, jamais implémentée
  dans aucune version.
