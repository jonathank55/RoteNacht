# 🌙 RoteNacht

[![Platform](https://img.shields.io/badge/Platform-macOS%2012%2B-blue.svg)](https://apple.com)
[![Language](https://img.shields.io/badge/Language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**RoteNacht** ist ein eleganter, maximal stabiler macOS-Hintergrunddienst, der den gesamten Bildschirm automatisch zu einer festlegbaren Uhrzeit (Standard: **20:00 Uhr**) in einen **reinen Hardware-Rotmodus (100 % Rot, 0 % Grün, 0 % Blau)** umstellt.

---

## 👁️ Warum reines Hardware-Rot?

Herkömmliche Nachtfilter („Night Shift“, „True Tone“) reduzieren blaues Licht nur minimal. Kurzwellige Grün- und Restblaustrahlen unterdrücken weiterhin die körpereigene Melatoninproduktion.

**RoteNacht klemmt alle Grün- und Blaukanäle hardwareseitig ab:**
- **100 % Blaulicht- und Grünlicht-Befreiung:** Das Display strahlt physikalisch ausschließlich rote Photonen ab.
- **Erhalt der Dunkeladaption:** Rotes Licht zersetzt das Sehpurpur (Rhodopsin) der Netzhaut nicht.
- **Maximale Einfachheit:** Keine fehleranfälligen Accessibility-Tricks oder Graustufen-Konflikte, sondern eine direkte, saubere CoreGraphics-Gammamatrix.

---

## ✨ Features

- ⏰ **Vollautomatischer Zeitplan:** Stellt täglich um 20:00 Uhr (oder jede gewünschte Zeit via `-z`) automatisch auf Rot um und morgens (Standard: 06:00 Uhr via `-e`) zurück.
- ⚡ **Zero Dependencies & Native C:** Nativ in C geschrieben. Nahezu 0 % CPU-Last und minimaler Speicherverbrauch (< 5 MB).
- 🖥️ **Multi-Monitor:** Funktioniert automatisch auf allen angeschlossenen Displays (intern & extern).
- 🔄 **Display- & Aufwach-Resistent:** Erkennt Monitor-Verbindungen (`CGDisplayRegisterReconfigurationCallback`) und Aufwachen aus dem Ruhezustand (`IORegisterForSystemPower`) und behält den Rotmodus zuverlässig bei.
- 🎛️ **Maximale Einfachheit:** Komfortabel steuerbar mit `nachtrot ein` und `nachtrot aus`.

---

## 🚀 Installation

```bash
git clone https://github.com/jonathank55/RoteNacht.git
cd RoteNacht
make install
```

---

## 💻 Verwendung

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

# Hintergrunddienst starten oder stoppen
nachtrot install
nachtrot uninstall

# Hilfe anzeigen
nachtrot --help
```

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

---

## 📄 Lizenz

Dieses Projekt ist unter der [MIT-Lizenz](LICENSE) lizenziert.
