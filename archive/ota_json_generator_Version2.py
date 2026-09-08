#!/usr/bin/env python3
import hashlib
import json
import os
import sys
import shutil

try:
    import tkinter as tk
    from tkinter import filedialog
    GUI_AVAILABLE = True
except ImportError:
    GUI_AVAILABLE = False

PROCEDURE = """
--- Procédure de publication d'une mise à jour OTA WordClock ESP32 ---

1. Incrémente la version dans le code (WC_FW_VERSION_MAJOR/MINOR/PATCH).
2. Compile le firmware : tu obtiens un fichier .bin.
3. Lance ce script pour :
    - Calculer le hash MD5 du .bin.
    - Générer le fichier JSON OTA.
    - T'aider à renseigner les URLs.
4. Dépose le .bin et le .json sur ton serveur (même dossier recommandé).
5. Dans la WebUI WordClock, mets à jour l'URL du JSON si besoin.
6. Clique sur "Vérifier les mises à jour" puis "Mettre à jour maintenant" si dispo.

Format du JSON généré :
{
  "version": "x.y.z",
  "bin_url": "https://tonsite.com/wordclock/wordclock_vx.y.z.bin",
  "md5": "hash_md5"
}
"""

def ask_procedure():
    print(PROCEDURE)
    input("\nAppuie sur Entrée pour continuer...")

def choose_bin_file():
    if GUI_AVAILABLE:
        root = tk.Tk()
        root.withdraw()
        file_path = filedialog.askopenfilename(title="Choisir le fichier .bin", filetypes=[("Firmware BIN", "*.bin")])
        root.destroy()
        return file_path
    else:
        return input("Chemin du fichier .bin à utiliser : ").strip()

def choose_dest_folder():
    if GUI_AVAILABLE:
        root = tk.Tk()
        root.withdraw()
        folder_path = filedialog.askdirectory(title="Choisir le dossier de destination (partage réseau ou local)")
        root.destroy()
        return folder_path
    else:
        return input("Chemin du dossier de destination (local ou partagé, ex: Z:\\WordClock) : ").strip()

def compute_md5(filepath):
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def main():
    print("=== Générateur de JSON OTA pour WordClock ESP32 ===\n")
    print("Tape ? puis Entrée pour afficher la procédure détaillée à tout moment.\n")

    while True:
        action = input("Appuie sur Entrée pour commencer, ou ? pour la procédure, ou q pour quitter : ").strip()
        if action == "?":
            ask_procedure()
        elif action.lower() in ['q', 'quit', 'exit']:
            sys.exit(0)
        else:
            break

    # Sélection du .bin
    while True:
        if GUI_AVAILABLE:
            print("Une fenêtre va s'ouvrir pour choisir le fichier .bin.")
        bin_path = choose_bin_file()
        if not bin_path or not os.path.isfile(bin_path):
            print("Fichier non trouvé. Réessaie.")
        else:
            break

    print(f"Fichier choisi : {bin_path}")
    bin_name = os.path.basename(bin_path)

    # Version firmware
    while True:
        version = input("Version du firmware (ex: 1.5.0) : ").strip()
        if version.count('.') == 2 and all(s.isdigit() for s in version.replace('.','')):
            break
        else:
            print("Format incorrect. Ex: 1.5.0")

    # URL du dossier distant
    url_base = input("URL de base du dossier OTA sur le serveur (ex: https://tonsite.com/wordclock/) : ").strip()
    if not url_base.endswith('/'):
        url_base += '/'
    # URL complète du .bin
    bin_url = url_base + bin_name

    # Calcul du hash
    print("Calcul du hash MD5...")
    md5 = compute_md5(bin_path)
    print(f"MD5 : {md5}")

    # Génération du JSON
    ota_json = {
        "version": version,
        "bin_url": bin_url,
        "md5": md5
    }

    # Nom du JSON suggéré
    json_filename = f"latest.json"
    json_path = os.path.join(os.path.dirname(bin_path), json_filename)

    # Enregistrement local
    with open(json_path, "w") as f:
        json.dump(ota_json, f, indent=2)
    print(f"\nJSON OTA généré et sauvegardé sous : {json_path}\n")
    print("Contenu du JSON :")
    print(json.dumps(ota_json, indent=2))

    # Choix du dossier de destination
    print("\nOù veux-tu déposer le .bin et le .json ?\n")
    dest_folder = choose_dest_folder()
    if not os.path.isdir(dest_folder):
        print("Dossier de destination non trouvé, copie annulée.")
        return

    # Copie des fichiers
    try:
        shutil.copy2(bin_path, os.path.join(dest_folder, bin_name))
        shutil.copy2(json_path, os.path.join(dest_folder, json_filename))
        print(f"\nFichiers copiés avec succès dans : {dest_folder}")
        print(f"- {bin_name}")
        print(f"- {json_filename}")
        print("\nTu peux maintenant accéder à ces fichiers sur ton serveur/dossier partagé !")
    except Exception as e:
        print(f"Erreur lors de la copie des fichiers : {e}")

    print("\nN'oublie pas de vérifier l'URL dans la WebUI WordClock si besoin !")
    print("Bonne mise à jour OTA !\n")

if __name__ == "__main__":
    main()