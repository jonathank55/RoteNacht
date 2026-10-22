# 🌙 RoteNacht

[![Platform](https://img.shields.io/badge/Platform-macOS%2012%2B-blue.svg)](https://apple.com)
[![Language](https://img.shields.io/badge/Language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**RoteNacht** ist ein eleganter, extrem leichtgewichtiger macOS-Hintergrunddienst, der den gesamten Bildschirm automatisch zu einer festlegbaren Uhrzeit (Standard: **20:00 Uhr**) in einen reinen **Schwarz/Rot-Monochrom- bzw. Binärfarbmodus** umstellt.

Wie ein klassischer Schwarz/Weiß-Modus – jedoch ohne kaltes Weiß, sondern in tiefem, augenschonenden **Rot**.

---

## 👁️ Warum Schwarz & Rot?

Herkömmliche Nachtfilter ("Night Shift", "True Tone") reduzieren blaues Licht nur minimal. Kurzwellige Grün- und Restblaustrahlen unterdrücken weiterhin die körpereigene Melatoninproduktion. 

**RoteNacht eliminiert alle Grün- und Blaukanäle vollständig:**
- **100 % Blaulicht- und Grünlicht-Befreiung:** Es strahlt ausschließlich die langwelligste sichtbare Lichtkomponente (Rot) ab.
- **Erhalt der Dunkeladaption:** Rotes Licht zersetzt das Sehpurpur (Rhodopsin) der Stäbchen in der Netzhaut nicht.
- **Monochrom / Binär:** Der gesamte Bildschirminhalt (Texte, Webseiten, Bilder, Menüs) wird deterministisch in Rot-Intensitäten auf reinem Schwarz abgebildet.

---

## ✨ Features

- ⏰ **Vollautomatischer Zeitplan:** Stellt um 20:00 Uhr (oder jede gewünschte Zeit via `-z`) automatisch auf Rot um und morgens (Standard: 06:00 Uhr via `-e`) zurück.
- ⚡ **Zero Dependencies & Native C:** Nutzt direkte macOS CoreGraphics- und Accessibility-APIs. Nahezu 0 % CPU-Last und minimaler Speicherverbrauch (< 5 MB).
- 🖥️ **Multi-Monitor:** Funktioniert automatisch auf allen angeschlossenen Displays (intern & extern).
- 🔄 **Display- & Aufwach-Resistent:** Erkennt Monitor-Verbindungen (`CGDisplayRegisterReconfigurationCallback`) und Aufwachen aus dem Ruhezustand (`IORegisterForSystemPower`) und behält den Rotmodus zuverlässig bei.
- 🎛️ **Maximale Einfachheit:** Steuerbar mit dem kurzen Terminal-Befehl `nachtrot`.

---

## 🚀 Installation

### Schnell-Installation via Git:

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

# Status & Konfiguration prüfen
nachtrot status
# oder einfach:
nachtrot

# Rotmodus sofort manuell ein- oder ausschalten
nachtrot on
nachtrot off

# Modus umschalten (Toggle)
nachtrot toggle

# Manuellen Override beenden und dem Zeitplan folgen
nachtrot auto

# Zwischen fließendem Schwarz/Rot-Verlauf und hartem 1-Bit Binär-Rot umschalten
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
1. **Schwarz/Rot Monochrom (Standard):** Alle Bildschirminhalte werden in Graustufen umgewandelt und als kontinuierlicher Helligkeitsverlauf von reinem Schwarz bis Vollrot dargestellt. Ideal zum Lesen, Coden und Arbeiten.
2. **1-Bit Binär-Rot (`nachtrot -b`):** Schwellenwert-Modus (unter 50 % Helligkeit = Schwarz, ab 50 % = Vollrot). Für extremen Kontrast und Null-Ablenkung.

---

## ⚙️ Technische Funktionsweise

RoteNacht arbeitet in zwei synchronisierten Stufen auf Systemebene:
1. **Luminanz-Mapping:** Über das interne `UniversalAccess`-Framework (`UAGrayscaleSetEnabled`) wird das Bildschirmsignal des WindowServers in einfarbige Luminanz überführt.
2. **Kanal-Isolation:** Mittels CoreGraphics `CGSetDisplayTransferByTable` wird eine Hardware-Gammamatrix auf alle Grafikausgänge gelegt, die den Grün- und Blau-Transfervektor auf exakt `0.0` klemmt und den Rot-Transfervektor an die berechnete Luminanz koppelt.
3. **Launchd Daemon:** Der Hintergrunddienst läuft als `com.rotenacht.daemon` im Benutzerkontext ohne Root-Rechte.

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
