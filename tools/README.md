# tools/

## `ota_json_generator_v3.0.py`

Prepare les deux fichiers a joindre a une release GitHub : le firmware renomme
avec son numero de version, et le `latest.json` que l'horloge va lire.

Double-clic sur le `.exe`, ou en ligne de commande :

```
python tools/ota_json_generator_v3.0.py
```

Taper `?` au demarrage affiche la procedure complete.

Le script **n'envoie rien** : il ecrit deux fichiers dans `dist/`, la publication
sur GitHub reste manuelle.

### Ce qu'il verifie

- La version que tu annonces est comparee a celle reellement compilee dans le
  croquis (`WC_FW_VERSION_MAJOR/MINOR/PATCH`). En cas d'ecart, il s'arrete et
  explique quoi faire. C'est l'erreur la plus courante : publier un binaire sous
  un numero qui n'est pas le sien.
- Le binaire est renomme `WordClock_ESP32_<version>.bin`, pour que deux releases
  ne produisent pas deux fichiers de meme nom.
- L'URL GitHub Releases est construite automatiquement, en `releases/latest`, qui
  suit toujours la derniere publication : l'URL enregistree dans l'horloge n'a
  jamais a changer.

Le mode « autre URL » reste disponible pour un NAS ou un serveur local.

### Fabriquer l'executable

```
pip install pyinstaller
pyinstaller --clean tools/ota_json_generator.spec
```

L'exe sort dans `dist/`. Il n'est pas versionne dans le depot : un binaire
Windows declenche des alertes antivirus au telechargement et pese dans
l'historique pour toujours. Il se joint en asset de release, comme le firmware.

## Historique

Ce script succede a `ota_json_generator_Version2.py`, ecrit pour les premieres
mises a jour OTA et conserve dans `archive/`. Meme principe, meme format de
JSON, meme interface graphique ; les verifications de coherence et le mode
GitHub Releases sont les ajouts de la v3.
