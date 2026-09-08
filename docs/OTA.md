# Publier une mise à jour OTA (ESP32 uniquement)

L'horloge interroge une URL qui renvoie un petit JSON décrivant la dernière
version disponible. Si le numéro est supérieur à celui installé, le bouton de mise
à jour apparaît dans l'interface web.

```json
{
  "version": "2.0.1",
  "bin_url": "https://github.com/wizhard2006/WordClock_WiZ/releases/latest/download/WordClock_ESP32_2.0.1.bin",
  "md5": "…"
}
```

Le `md5` est désormais vérifié pendant l'écriture en flash : un téléchargement
corrompu est refusé et l'ancien firmware reste en place.

## Ce qu'est un « asset de release »

Un dépôt git garde chaque version de chaque fichier pour toujours. Un binaire de
1,2 Mo commité dix fois, ce sont 12 Mo que tout le monde retélécharge à chaque
clone, définitivement.

Une **release** GitHub est une étiquette posée sur un commit, à laquelle on
attache des fichiers : les *assets*. Ils sont hébergés à côté du dépôt, pas dedans.
URL de téléchargement directe, zéro poids dans l'historique, suppression possible.
C'est exactement ce qu'il faut pour un firmware.

Le `.f3z` (48 Mo) reste dans le dépôt en revanche : il fait partie du projet, il
est sous la limite des 100 Mo de GitHub, et il n'a pas vocation à changer souvent.

## Procédure

**1. Incrémenter la version** en tête du croquis ESP32 :

```c
#define WC_FW_VERSION_PATCH 2
```

Correction d'affichage → PATCH, nouvelle fonction → MINOR, rupture du format de
configuration → MAJOR.

**2. Compiler**, puis *Croquis > Exporter les binaires compilés*. Le `.bin` sort
dans le sous-dossier `build/` du croquis.

**3. Fabriquer les fichiers de publication.** Double-clic sur
`ota_json_generator_v3.0.exe`, ou depuis le dépôt :

```
python tools/ota_json_generator_v3.0.py
```

Il demande le `.bin`, retrouve tout seul la version compilée dans le croquis,
calcule le MD5, renomme le binaire et écrit `latest.json` dans `dist/`.

Si la version que tu annonces ne correspond pas à celle compilée, il s'arrête et
te le dit : c'est l'erreur la plus courante, publier un binaire sous un numéro
qui n'est pas le sien.

**4. Taguer et publier** :

```bash
git tag -a esp32-v2.0.1 -m "WordClock ESP32 2.0.1"
git push origin main --follow-tags
```

Puis sur GitHub : *Releases > Draft a new release*, choisir le tag, joindre en
assets les deux fichiers de `dist/`.

**5. Vérifier depuis l'horloge** : WebUI → *Check for update*. La version distante
s'affiche, puis le bouton de mise à jour. La carte redémarre seule et affiche le
résultat au retour.

## Points d'attention

**Le dépôt doit être public.** Les assets d'un dépôt privé exigent un jeton :
l'horloge recevrait une erreur 404. C'était le défaut de l'ancienne URL.

**`releases/latest/download/…` suit automatiquement la dernière release.** C'est
l'URL par défaut du croquis : plus besoin de reconfigurer l'horloge à chaque
publication.

**Toujours garder la possibilité de flasher par USB.** Une release avec un mauvais
binaire rend l'OTA inutilisable ; il faut alors rebrancher le câble.
