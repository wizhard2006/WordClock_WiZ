#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generateur de JSON OTA pour WordClock ESP32 — version 3.0

Reprend l'outil d'origine (ota_json_generator_Version2.py) en ajoutant deux
garde-fous et le mode GitHub Releases :

  - la version saisie est verifiee contre celle reellement compilee dans le
    croquis (WC_FW_VERSION_MAJOR/MINOR/PATCH), pour ne plus publier un binaire
    sous un numero qui ne correspond pas ;
  - le binaire est renomme avec son numero de version, pour que deux releases
    ne produisent pas deux fichiers de meme nom ;
  - l'URL GitHub Releases est construite automatiquement, l'URL libre reste
    disponible pour un serveur local ou un NAS.

Aucun envoi, aucune publication : le script ecrit deux fichiers sur le disque,
c'est tout. La publication reste manuelle.

Depend uniquement de la bibliotheque standard. Fonctionne en fenetre graphique
si tkinter est disponible, sinon en saisie clavier.
"""

import hashlib
import json
import os
import re
import shutil
import sys

# --- Interface graphique, seulement si un affichage existe reellement --------
GUI_AVAILABLE = False
try:
    import tkinter as tk
    from tkinter import filedialog
    if os.name == "nt" or os.environ.get("DISPLAY"):
        GUI_AVAILABLE = True
except ImportError:
    pass

DEFAULT_REPO = "wizhard2006/WordClock_WiZ"
BIN_PREFIX = "WordClock_ESP32"

PROCEDURE = """
--- Publier une mise a jour OTA WordClock ESP32 ---

1. Incrementer la version en tete du croquis :
       #define WC_FW_VERSION_MAJOR 2
       #define WC_FW_VERSION_MINOR 0
       #define WC_FW_VERSION_PATCH 1
   Correction d'affichage -> PATCH, nouvelle fonction -> MINOR,
   rupture du format de configuration -> MAJOR.

2. Compiler, puis dans l'IDE Arduino :
       Croquis > Exporter les binaires compiles
   Le .bin apparait dans le sous-dossier build\\ du croquis.

3. Lancer ce script. Il va :
       - retrouver la version compilee dans le croquis et la comparer a
         celle que tu annonces ;
       - calculer le MD5 du .bin ;
       - renommer le binaire avec son numero de version ;
       - ecrire le latest.json qui va avec.

4. Sur GitHub : Releases > Draft a new release
       - choisir le tag correspondant (ex: esp32-v2.0.1)
       - joindre les DEUX fichiers produits par ce script
       - Publish release

5. Sur l'horloge : WebUI > Check for update, puis Update now.

Le depot doit etre PUBLIC : les fichiers joints a une release privee
exigent un jeton d'authentification, l'horloge recevrait une erreur 404.

Format du JSON genere :
{
  "version": "2.0.1",
  "bin_url": "https://github.com/<compte>/<depot>/releases/latest/download/WordClock_ESP32_2.0.1.bin",
  "md5": "empreinte du binaire"
}
"""


# ---------------------------------------------------------------------------
# Saisie
# ---------------------------------------------------------------------------
def choose_bin_file():
    if GUI_AVAILABLE:
        print("Une fenetre va s'ouvrir pour choisir le fichier .bin.")
        root = tk.Tk()
        root.withdraw()
        path = filedialog.askopenfilename(
            title="Choisir le firmware compile (.bin)",
            filetypes=[("Firmware BIN", "*.bin")])
        root.destroy()
        return path
    return input("Chemin du fichier .bin : ").strip().strip('"')


def choose_folder(title):
    if GUI_AVAILABLE:
        print(f"Une fenetre va s'ouvrir : {title}")
        root = tk.Tk()
        root.withdraw()
        path = filedialog.askdirectory(title=title)
        root.destroy()
        return path
    return input(f"{title} : ").strip().strip('"')


def ask(prompt, default=""):
    suffix = f" [{default}]" if default else ""
    answer = input(f"{prompt}{suffix} : ").strip()
    return answer or default


def confirm(question):
    return input(f"{question} (o/N) : ").strip().lower() in ("o", "oui", "y", "yes")


# ---------------------------------------------------------------------------
# Lecture de la version dans le croquis
# ---------------------------------------------------------------------------
def find_sketch(bin_path):
    """Remonte l'arborescence depuis le .bin a la recherche du croquis.

    Chemin typique produit par l'IDE Arduino :
        .../WordClock_ESP32/build/esp32.esp32.esp32/WordClock_ESP32.ino.bin
    Le croquis est donc deux a trois niveaux au-dessus.
    """
    folder = os.path.dirname(os.path.abspath(bin_path))
    for _ in range(4):
        for name in sorted(os.listdir(folder)):
            if name.endswith(".ino"):
                return os.path.join(folder, name)
        parent = os.path.dirname(folder)
        if parent == folder:
            break
        folder = parent
    return None


def read_sketch_version(sketch_path):
    """Retourne 'x.y.z' lue dans le croquis, ou None."""
    try:
        with open(sketch_path, encoding="utf-8", errors="replace") as f:
            src = f.read()
    except OSError:
        return None
    parts = []
    for key in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(r"#define\s+WC_FW_VERSION_" + key + r"\s+(\d+)", src)
        if not m:
            return None
        parts.append(m.group(1))
    return ".".join(parts)


def find_repo_root(start):
    """Remonte jusqu'au dossier contenant .git, pour proposer dist/ par defaut."""
    folder = os.path.dirname(os.path.abspath(start))
    for _ in range(8):
        if os.path.isdir(os.path.join(folder, ".git")):
            return folder
        parent = os.path.dirname(folder)
        if parent == folder:
            break
        folder = parent
    return None


def compute_md5(filepath):
    h = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


# ---------------------------------------------------------------------------
def main():
    print("=== Generateur de JSON OTA pour WordClock ESP32 — v3.0 ===\n")
    print("Tape ? puis Entree a tout moment pour afficher la procedure complete.\n")

    while True:
        action = input("Entree pour commencer, ? pour la procedure, q pour quitter : ").strip()
        if action == "?":
            print(PROCEDURE)
            input("\nAppuie sur Entree pour continuer...")
        elif action.lower() in ("q", "quit", "exit"):
            sys.exit(0)
        else:
            break

    # --- 1. Le binaire ------------------------------------------------------
    while True:
        bin_path = choose_bin_file()
        if bin_path and os.path.isfile(bin_path):
            break
        print("Fichier introuvable. Reessaie.")
    bin_path = os.path.abspath(bin_path)
    print(f"\nBinaire   : {bin_path}")
    print(f"Taille    : {os.path.getsize(bin_path):,} octets".replace(",", " "))

    # --- 2. La version, verifiee contre le croquis --------------------------
    sketch = find_sketch(bin_path)
    sketch_version = read_sketch_version(sketch) if sketch else None

    if sketch_version:
        print(f"Croquis   : {sketch}")
        print(f"Version compilee dans le croquis : {sketch_version}")
        version = ask("\nVersion a publier", sketch_version)
    else:
        print("\nATTENTION : croquis introuvable a cote du binaire, ou WC_FW_VERSION_*")
        print("absent. La coherence de la version ne peut PAS etre verifiee.")
        version = ask("Version a publier (ex: 2.0.1)")

    while not VERSION_RE.match(version):
        print("Format attendu : X.Y.Z (ex: 2.0.1)")
        version = ask("Version a publier", sketch_version or "")

    if sketch_version and version != sketch_version:
        print(f"\n  !!  INCOHERENCE  !!")
        print(f"  Le croquis compile annonce  : {sketch_version}")
        print(f"  Tu t'appretes a publier     : {version}")
        print("\n  C'est l'erreur classique : le binaire ne contient pas la version")
        print("  que le JSON annonce. L'horloge proposera indefiniment une mise a")
        print("  jour qu'elle a deja installee, ou refusera celle dont elle a besoin.")
        print("\n  A faire : corriger WC_FW_VERSION_* dans le croquis, recompiler,")
        print("  reexporter le binaire, puis relancer ce script.")
        if not confirm("\n  Continuer quand meme"):
            print("\nAnnule. Rien n'a ete ecrit.")
            return

    out_bin_name = f"{BIN_PREFIX}_{version}.bin"

    # --- 3. Ou sera publie le binaire ---------------------------------------
    print("\nOu le binaire sera-t-il telechargeable par l'horloge ?")
    print("  1 - Release GitHub (recommande)")
    print("  2 - Autre URL : NAS, serveur local, site perso")
    mode = ask("Choix", "1")

    if mode == "2":
        url_base = ask("URL du dossier contenant le .bin (ex: http://nas.local/wordclock/)")
        if not url_base.endswith("/"):
            url_base += "/"
        bin_url = url_base + out_bin_name
    else:
        repo = ask("Depot GitHub", DEFAULT_REPO)
        # "releases/latest/download" suit automatiquement la derniere release
        # publiee : l'URL enregistree dans l'horloge n'a jamais a changer.
        bin_url = f"https://github.com/{repo}/releases/latest/download/{out_bin_name}"

    # --- 4. Empreinte -------------------------------------------------------
    print("\nCalcul de l'empreinte MD5...")
    md5 = compute_md5(bin_path)
    print(f"MD5       : {md5}")

    ota_json = {"version": version, "bin_url": bin_url, "md5": md5}

    # --- 5. Destination -----------------------------------------------------
    root = find_repo_root(bin_path)
    default_dest = os.path.join(root, "dist") if root else ""
    print("\nDossier de destination des deux fichiers a publier.")
    if default_dest:
        print(f"Entree pour accepter : {default_dest}")
        dest = input("Autre dossier (Entree pour accepter) : ").strip().strip('"') or default_dest
    else:
        dest = choose_folder("Dossier de destination")

    if not dest:
        print("Aucune destination. Rien n'a ete ecrit.")
        return
    os.makedirs(dest, exist_ok=True)

    # --- 6. Ecriture --------------------------------------------------------
    out_bin = os.path.join(dest, out_bin_name)
    out_json = os.path.join(dest, "latest.json")
    try:
        shutil.copy2(bin_path, out_bin)
        with open(out_json, "w", encoding="utf-8") as f:
            json.dump(ota_json, f, indent=2)
            f.write("\n")
    except OSError as e:
        print(f"\nErreur d'ecriture : {e}")
        return

    # --- 7. Recapitulatif ---------------------------------------------------
    print("\n" + "-" * 68)
    print("Fichiers prets a joindre a la release :")
    print(f"  {out_bin}")
    print(f"  {out_json}")
    print("\nContenu du latest.json :")
    print(json.dumps(ota_json, indent=2))
    print("-" * 68)
    print("\nEtapes suivantes, a faire a la main :")
    print(f"  1. git tag -a esp32-v{version} -m \"WordClock ESP32 {version}\"")
    print("     git push --follow-tags")
    print("  2. GitHub > Releases > Draft a new release")
    print(f"     tag esp32-v{version}, joindre les DEUX fichiers ci-dessus")
    print("  3. Horloge > WebUI > Check for update")
    print("\nLe depot doit etre public, sinon l'horloge recevra une erreur 404.\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrompu.")
    if os.name == "nt":
        input("Appuie sur Entree pour fermer...")
