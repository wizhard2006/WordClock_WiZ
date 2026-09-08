# Marche à suivre — pas à pas

Aucune de ces commandes n'a été lancée. Tu fais tout toi-même, dans l'ordre.
Compte une heure, tranquillement.

---

## Ce qu'il faut savoir avant de commencer

Ton dépôt `WordClock_WiZ` **existe déjà** et il est **public**. Il contient :

- un seul fichier, `NoReadMe`, sur la branche `main` ;
- deux étiquettes (tags) : `v1.5.1` et `Last_Update_WiZ` ;
- une publication (release) `Last_Update_WiZ` avec deux fichiers joints :
  `WordClock_OTA_WebUI_Refresh_ESP32_final.ino.bin` et `latest.json`.

Il s'appelait `WiZ` avant, tu l'as renommé. On ne repart donc pas de zéro : on
ajoute le contenu dans un dépôt qui a déjà un commit et un historique.

**Vocabulaire, une fois pour toutes :**

| Mot | Ce que c'est |
|---|---|
| **dépôt** (*repository*) | le dossier de ton projet, avec tout son historique |
| **commit** | une photo de l'état de tes fichiers, avec un message qui dit pourquoi |
| **tag** | une étiquette posée sur un commit, pour retrouver « la version 2.0.1 » |
| **push** | envoyer tes commits locaux vers GitHub |
| **clone** | copier le dépôt GitHub sur ton disque, avec son historique |
| **release** | une page de publication sur GitHub, à laquelle on **joint** des fichiers |
| **asset** | un fichier joint à une release (ici : le `.bin` et le `latest.json`) |

---

## Étape 1 — Installer Git (si ce n'est pas déjà fait)

Ouvre PowerShell et tape :

```powershell
git --version
```

Si ça affiche un numéro de version, passe à l'étape 2. Sinon, télécharge Git for
Windows sur <https://git-scm.com/download/win> et installe-le en laissant toutes
les options par défaut. Ferme et rouvre PowerShell ensuite.

Première utilisation, à faire une seule fois sur ta machine :

```powershell
git config --global user.name "wizhard2006"
git config --global user.email "ton.email@exemple.fr"
```

Cet email apparaîtra dans l'historique public. Si tu ne veux pas exposer le tien,
GitHub fournit une adresse de substitution : *Settings > Emails > Keep my email
address private*, et tu utilises l'adresse en `@users.noreply.github.com` qu'il
t'indique.

---

## Étape 2 — Récupérer le dépôt existant sur ton disque

On ne fait **pas** `git init` : le dépôt existe déjà en ligne, on le clone.

```powershell
cd D:\
mkdir Projets
cd D:\Projets
git clone https://github.com/wizhard2006/WordClock_WiZ.git
cd WordClock_WiZ
```

Tu obtiens un dossier `D:\Projets\WordClock_WiZ` qui contient `NoReadMe` et un
sous-dossier caché `.git` (l'historique — ne jamais y toucher à la main).

Une fenêtre de connexion GitHub peut s'ouvrir au moment du `clone` ou du `push` :
connecte-toi avec ton compte, Windows retiendra l'autorisation.

---

## Étape 3 — Copier les fichiers livrés

Dans `D:\Projets\WordClock_WiZ`, copie **tout le contenu** du dossier que je t'ai
livré : `README.md`, `CHANGELOG.md`, `LICENSE`, `.gitignore`, et les dossiers
`docs\`, `firmware\`, `hardware\`, `tools\`, `archive\`.

Ne supprime pas `NoReadMe`, on s'en occupe à l'étape suivante.

Attention : le fichier `.gitignore` commence par un point, l'explorateur Windows le
cache par défaut. Active *Affichage > Éléments masqués* pour le voir et le copier.

### Les deux fichiers à ajouter toi-même

**Le modèle 3D.** Copie
`Word Clock French Mini upgrade grille v9.f3z`
dans `hardware\fusion360\`, puis supprime le fichier `A_COPIER_ICI.txt` qui s'y
trouve. Ne copie **pas** le « v8 » : c'est un doublon exact du v9, même empreinte.

**Les STL, si tu les as.** Dans `hardware\stl\`. C'est ce dont ont besoin les gens
qui veulent seulement imprimer la lettrine sans ouvrir Fusion 360.

---

## Étape 4 — Ranger le fichier NoReadMe

Il ne sert plus à rien maintenant qu'il y a un vrai `README.md`. Dans PowerShell,
depuis `D:\Projets\WordClock_WiZ` :

```powershell
git rm NoReadMe
```

`git rm` supprime le fichier **et** enregistre la suppression. Un simple clic droit
puis Supprimer marcherait aussi, mais autant rester dans Git.

---

## Étape 5 — Vérifier ce que Git voit avant de valider

```powershell
git status
```

Tu obtiens la liste de tout ce qui a changé. Vérifie deux choses :

- **aucun dossier `build`** ne doit apparaître (le `.gitignore` doit les écarter) ;
- le `.f3z` doit être listé, une seule fois.

Pour voir la liste exacte de ce qui va partir :

```powershell
git add -A
git status --short
```

`git add -A` prépare tout. Le `--short` affiche une ligne par fichier : `A` pour
ajouté, `D` pour supprimé.

Si un dossier `build` apparaît malgré tout :

```powershell
git reset
```

qui annule le `add` sans rien détruire. Vérifie ensuite que `.gitignore` est bien à
la racine et bien nommé.

---

## Étape 6 — Le premier commit

```powershell
git commit -m "feat: firmwares ESP8266 et ESP32 corriges, documentation et archive"
```

Il ne se passe rien de visible en ligne : le commit est **local**. C'est normal.

---

## Étape 7 — Poser les étiquettes de version

```powershell
git tag -a esp32-v2.0.1 -m "WordClock ESP32 2.0.1"
git tag -a esp8266-v18.1 -m "WordClock ESP8266 v18 corrige"
```

Deux étiquettes distinctes, parce que les deux firmwares évoluent séparément.
`-a` crée une étiquette annotée, qui garde la date et l'auteur — préférable à une
étiquette simple.

---

## Étape 8 — Envoyer sur GitHub

```powershell
git push origin main --follow-tags
```

`--follow-tags` envoie les étiquettes en même temps que les commits.

Va voir <https://github.com/wizhard2006/WordClock_WiZ> : le README doit s'afficher
sous la liste des fichiers.

### Si le push est refusé

Message du type « Updates were rejected » : le dépôt en ligne a changé depuis ton
clone. Récupère d'abord, puis renvoie :

```powershell
git pull --rebase origin main
git push origin main --follow-tags
```

`--rebase` rejoue tes commits par-dessus ce qui existe en ligne, sans créer de
commit de fusion inutile.

---

## Étape 9 — Publier le firmware pour l'OTA

À faire seulement quand tu auras recompilé l'ESP32 en version 2.0.1.

**9a. Compiler et exporter.** Dans l'IDE Arduino, ouvre
`firmware\esp32\WordClock_ESP32\WordClock_ESP32.ino`, vérifie que la carte est
*ESP32 Dev Module*, puis : *Croquis > Exporter les binaires compilés*.

Le fichier apparaît dans un sous-dossier `build\` du croquis. Ce dossier est
ignoré par Git, c'est voulu.

**9b. Fabriquer les fichiers de publication.** Double-clic sur
`ota_json_generator_v3.0.exe`, ou dans PowerShell depuis `D:\Projets\WordClock_WiZ` :

```powershell
python tools\ota_json_generator_v3.0.py
```

Il ouvre une fenêtre pour choisir le `.bin`, propose tout seul la version lue
dans le croquis, calcule l'empreinte MD5, et écrit dans `dist\` :

- `WordClock_ESP32_2.0.1.bin` — le firmware, renommé avec son numéro de version ;
- `latest.json` — le fichier que l'horloge lit pour savoir s'il y a du neuf.

Si le numéro que tu annonces ne correspond pas à celui compilé dans le croquis,
il refuse et explique quoi faire. C'est volontaire : c'est l'erreur classique.

Tape `?` au démarrage pour afficher la procédure complète.

**9c. Créer la release.** Sur GitHub : onglet *Releases* puis *Draft a new release*.

- *Choose a tag* : `esp32-v2.0.1`
- *Release title* : `WordClock ESP32 2.0.1`
- Description : copie la section 2.0.1 du `CHANGELOG.md`
- **Glisse-dépose les deux fichiers de `dist/`** dans la zone *Attach binaries*
- *Publish release*

`dist/` est dans le `.gitignore` : ces deux fichiers ne se commitent pas, ils
s'attachent à la release. C'est toute la différence entre le dépôt, qui garde tout
pour toujours, et une release, des fichiers posés à côté et remplaçables.

---

## Étape 10 — Vérifier que l'OTA fonctionne

Ouvre l'interface web de ton horloge ESP32, puis *Check for update*.

Elle doit répondre qu'elle est déjà à jour en 2.0.1. Si elle propose une mise à
jour vers 2.0.0, c'est que l'ancienne release `Last_Update_WiZ` est encore
considérée comme la plus récente : voir l'étape suivante.

Le vrai test, c'est de publier une 2.0.2 sans aucun changement de code, juste pour
valider le chemin complet **avant** d'en avoir besoin en vrai.

---

## Étape 11 — Faire le ménage dans l'ancienne release

Ton dépôt contient déjà la release `Last_Update_WiZ`, avec le firmware 2.0.0 et un
`latest.json` dont le champ `bin_url` pointe encore sur `wizhard2006/WiZ`,
l'ancien nom du dépôt.

GitHub redirige les anciennes URL, donc ça fonctionne encore aujourd'hui. Mais
c'est fragile : la redirection disparaît si quelqu'un recrée un dépôt nommé `WiZ`.

Une fois la release 2.0.1 publiée **et vérifiée depuis l'horloge** :

- soit tu supprimes la release `Last_Update_WiZ` (*Releases > Last_Update_WiZ >
  Edit > Delete*) ;
- soit tu la gardes en archive et tu corriges son `latest.json` pour qu'il pointe
  sur `WordClock_WiZ`.

Ne fais rien de tout ça **avant** d'avoir validé la nouvelle : tant que la 2.0.1
n'est pas confirmée, l'ancienne est ton filet de sécurité.

Le tag `v1.5.1` n'a aucun fichier joint. Si tu ne sais plus à quoi il correspond,
laisse-le : une étiquette ne pèse rien et ne gêne personne.

---

## Résumé des commandes

```powershell
# une seule fois sur la machine
git config --global user.name "wizhard2006"
git config --global user.email "ton.email@exemple.fr"

# mise en place
cd D:\Projets
git clone https://github.com/wizhard2006/WordClock_WiZ.git
cd WordClock_WiZ
#   ... copier les fichiers livrés + le .f3z ...
git rm NoReadMe
git add -A
git status --short
git commit -m "feat: firmwares ESP8266 et ESP32 corriges, documentation et archive"
git tag -a esp32-v2.0.1 -m "WordClock ESP32 2.0.1"
git tag -a esp8266-v18.1 -m "WordClock ESP8266 v18 corrige"
git push origin main --follow-tags
```

Et pour chaque modification future :

```powershell
git add -A
git commit -m "fix: ce que tu as corrige"
git push
```
