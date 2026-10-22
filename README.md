# 🌙 RoteNacht

[![Platform](https://img.shields.io/badge/Platform-macOS%2012%2B-blue.svg)](https://apple.com)
[![Language](https://img.shields.io/badge/Language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**RoteNacht** ist ein eleganter, extrem leichtgewichtiger macOS-Hintergrunddienst, der den gesamten Bildschirm automatisch zu einer festlegbaren Uhrzeit (Standard: **20:00 Uhr**) in einen reinen **Schwarz/Rot-Monochrom- bzw. Binärfarbmodus** umstellt.

Wie ein klassischer Schwarz/Weiß-Modus – jedoch ohne kaltes Weiß, sondern in tiefem, augenschonenden **Rot mit vollständiger Luminanz- und Lesbarkeitserhaltung aller Farben (insbesondere Grün & Blau)**.

---

## 👁️ Warum Luminanz-basiertes Schwarz & Rot?

Herkömmliche 1D-Gammamatrizen schalten den Grün- und Blaukanal einfach stumm. Das führt dazu, dass grüne Elemente (wie Ordner, Terminal-Text, Schaltflächen) fast schwarz und unlesbar werden, da Grün für das menschliche Auge 71,5 % der wahrgenommenen Helligkeit ($Y = 0.2126 R + 0.7152 G + 0.0722 B$) ausmacht.

**RoteNacht nutzt macOS WindowServer Luminanz-Mapping + Hardware-Kanalfilter:**
- **Volle Helligkeitserhaltung:** Grüne, weiße, gelbe und blaue Bildschirminhalte werden anhand ihrer natürlichen wahrgenommenen Luminanz in klares Rotlicht umgewandelt. Grüne Texte und Icons bleiben gestochen scharf und hell.
- **100 % Blaulicht- und Grünlicht-Befreiung:** Am Hardware-Ausgang strahlt ausschließlich der langwelligste sichtbare Lichtkanal (Rot) ab.
- **Erhalt der Dunkeladaption:** Rotes Licht schützt das Rhodopsin in den Stäbchen der Netzhaut und verhindert die Unterdrückung von Melatonin am Abend.

---

## ✨ Features

- ⏰ **Vollautomatischer Zeitplan:** Stellt um 20:00 Uhr (oder jede gewünschte Zeit via `-z`) automatisch auf Rot um und morgens (Standard: 06:00 Uhr via `-e`) zurück.
- ⚡ **Zero Dependencies & Native C:** Nativ in C mit CoreGraphics und MediaAccessibility implementiert. Nahezu 0 % CPU-Last und minimaler Speicherverbrauch (< 5 MB).
- 🖥️ **Multi-Monitor:** Funktioniert automatisch auf allen angeschlossenen Displays (intern & extern).
- 🔄 **Display- & Aufwach-Resistent:** Erkennt Monitor-Verbindungen (`CGDisplayRegisterReconfigurationCallback`) und Aufwachen aus dem Ruhezustand (`IORegisterForSystemPower`) und behält den Rotmodus zuverlässig bei.
- 🎛️ **Maximale Einfachheit:** Komfortabel steuerbar mit dem deutschen Terminal-Befehl `nachtrot ein` und `nachtrot aus`.

---

## 🚀 Installation

```bash
git clone https://github.com/jonathank55/RoteNacht.git
cd RoteNacht
make install
```

Das Skript kompiliert das native C-Binary, installiert den Befehl `nachtrot` (und `rotenacht`) in dein `~/bin/`-Verzeichnis und registriert den Hintergrunddienst als systemkonformen `LaunchAgent`.

---

## 💻 Verwendung

Der zentrale Befehl lautet **`nachtrot`**:

```bash
# Startzeit festlegen (z. B. täglich um 20:00 Uhr)
nachtrot -z 20:00

# Endzeit festlegen (z. B. morgens um 07:00 Uhr)
nachtrot -e 07:00

# Rotmodus sofort manuell ein- oder ausschalten
nachtrot ein
nachtrot aus

# Status & Konfiguration prüfen
nachtrot status
# oder einfach:
nachtrot

# Modus umschalten (Toggle)
nachtrot toggle

# Manuellen Override beenden und dem automatischen Zeitplan folgen
nachtrot auto

# Zwischen weichem Schwarz/Rot-Verlauf und hartem 1-Bit Binär-Rot umschalten
nachtrot -b

# Hintergrunddienst starten oder stoppen
nachtrot install
nachtrot uninstall

# Hilfe anzeigen
nachtrot --help
```

---

## 🎨 Farbmodi

RoteNacht unterstützt zwei Farbmodelle:
1. **Schwarz/Rot Luminanz-Monochrom (Standard):** Alle Bildschirminhalte werden anhand ihrer natürlichen Helligkeit in reines Rot gewandelt. Grüne Texte, Syntax-Highlighting und Benutzeroberflächen bleiben mit perfekter Klarheit lesbar.
2. **1-Bit Binär-Rot (`nachtrot -b`):** Schwellenwert-Modus (unter 50 % Helligkeit = Schwarz, ab 50 % = Vollrot). Für extremen Kontrast und Null-Ablenkung.

---

## 🗑️ Deinstallation

```bash
cd RoteNacht
make uninstall
```
Oder direkt über das Terminal:
```bash
nachtrot uninstall
```
Entfernt den LaunchAgent, stoppt den Hintergrunddienst und stellt alle Display-Farbprofile auf Werkseinstellungen zurück.

---

## 📄 Lizenz

Dieses Projekt ist unter der [MIT-Lizenz](LICENSE) lizenziert.
