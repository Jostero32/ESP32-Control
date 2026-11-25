#!/usr/bin/env python3
"""Genera ESP32_Control/WebAssets.h a partir de los archivos de web/.

La interfaz se incrusta en el firmware como cadenas PROGMEM, de modo que Arduino IDE
compila y sube todo en un único paso (sin LittleFS ni plugins de subida de archivos).

Uso (desde la carpeta del proyecto):
    python tools/generar_recursos_web.py          # regenera WebAssets.h
    python tools/generar_recursos_web.py --check  # solo comprueba que está actualizado

Solo es necesario ejecutarlo después de modificar algún archivo de web/.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB_DIR = ROOT / "web"
OUTPUT = ROOT / "ESP32_Control" / "WebAssets.h"
DELIMITER = "ESP32WEB"

# (archivo en web/, nombre del símbolo C++)
ASSETS = [
    ("index.html", "WEB_INDEX_HTML"),
    ("admin.html", "WEB_ADMIN_HTML"),
    ("styles.css", "WEB_STYLES_CSS"),
    ("wifi.js", "WEB_WIFI_JS"),
    ("admin.js", "WEB_ADMIN_JS"),
]


def render() -> str:
    parts = [
        "// ARCHIVO GENERADO por tools/generar_recursos_web.py a partir de la carpeta web/.",
        "// No lo edite a mano: modifique web/ y vuelva a ejecutar el script.",
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
    ]
    for filename, symbol in ASSETS:
        text = (WEB_DIR / filename).read_text(encoding="utf-8").replace("\r\n", "\n")
        if f"){DELIMITER}\"" in text:
            sys.exit(f"error: web/{filename} contiene el delimitador ){DELIMITER}\"")
        parts.append(f"// web/{filename}")
        parts.append(f'const char {symbol}[] PROGMEM = R"{DELIMITER}({text}){DELIMITER}";')
        parts.append("")
    return "\n".join(parts)


def main() -> int:
    content = render()
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else None
    if "--check" in sys.argv:
        if current != content:
            print(f"{OUTPUT.name} está desactualizado: ejecute tools/generar_recursos_web.py")
            return 1
        print(f"{OUTPUT.name} está actualizado")
        return 0
    OUTPUT.write_text(content, encoding="utf-8", newline="\n")
    print(f"Generado {OUTPUT.relative_to(ROOT)} ({len(content.encode('utf-8'))} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
