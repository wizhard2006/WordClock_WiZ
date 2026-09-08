# -*- mode: python ; coding: utf-8 -*-
#
# Recette PyInstaller pour fabriquer ota_json_generator_v3.0.exe
#
#   pip install pyinstaller
#   pyinstaller --clean tools/ota_json_generator.spec
#
# L'exe sort dans dist/. Il n'est PAS versionne dans le depot : on le joint en
# asset de release, comme le firmware. Les dossiers build/ et dist/ produits par
# PyInstaller sont ignores par le .gitignore.

a = Analysis(
    ['ota_json_generator_v3.0.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='ota_json_generator_v3.0',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
