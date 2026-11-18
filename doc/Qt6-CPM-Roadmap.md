# Roadmap: Qt 6 und CPM.cmake

Stand: 18. November 2026. Dies ist eine Planung; die Migration ist noch nicht begonnen.

**Planungsrahmen für den auf diesem Mac geprüften Fork: 500–900 %, also etwa
5–9 volle 5h-Kontingente einschließlich Reserve.** Als Arbeitsbudget würde ich
zunächst **700 % / sieben Kontingente** vorsehen und nach der CPM-Etappe neu schätzen.

Die Prozente verwenden die gewünschte Vergleichseinheit: **100 % = ein volles
5h-Kontingent, 200 % = zwei Kontingente**. Sie sind eine grobe Aufwandsschätzung,
keine Messung oder Zusage des tatsächlichen Kontingentverbrauchs. Download- und
Compilerwartezeiten machen daraus keinen verlässlichen Kalenderzeitplan.

## Grundlage und Ziel

Verglichen wurden der Fork bei `18d4eb05d` und der eingebundene Original-Clone bei
`1cdbf7236` vom 12. November 2026. Die öffentlich verfügbare Bauanleitung bestätigt
die gleiche grundsätzliche Build-Methode.

Der Fork verwendet aktuell Qt **5.15.18**, C++20 und eine im obersten CMake-Projekt
fest verdrahtete vcpkg-Toolchain. Das Original verwendet **CPM.cmake** als Schicht
über CMake FetchContent, separate Dateien in `cmake/dependencies/`, mindestens
CMake **3.28** und separat installiertes **Qt 6.10**. Qt selbst wird auch dort
nicht durch CPM gebaut. Quellen werden über `CPM_SOURCE_CACHE` wiederverwendet.
[Original-Bauanleitung](https://raw.githubusercontent.com/TrenchBroom/TrenchBroom/master/BUILD.md),
[CPM-Dokumentation](https://github.com/cpm-cmake/CPM.cmake).

Empfohlenes Ziel ist deshalb **Qt 6.10.x plus CPM.cmake** in der bestehenden
Projektstruktur. Lokal ist bereits Qt 6.10.1 installiert; die Version und die
benötigten Module werden am Beginn der Umsetzung geprüft. Übernommen werden
gezielt Build-Definitionen und passende Qt-Portierungen des Originals.

Der Basisumfang umfasst den macOS-arm64-Editor, seine vorhandenen Eigenentwicklungen,
relevante Regressionstests und ein lokal startbares App-Bundle. Windows-/Linux-
Builds und deren Auslieferung sind unten separat geschätzt.

## Empfohlene Reihenfolge und Aufwand

Die Zeilen enthalten jeweils zusätzlichen Aufwand; sie werden addiert.

| Schritt | Arbeit und überprüfbares Ergebnis | Aufwand |
| --- | --- | ---: |
| 0. Referenz und Testbasis | Migrationsbranch anlegen, aktuellen Qt5-Build und Einstellungen sichern, repräsentative Tests reproduzierbar festlegen, Qt-/Compiler-/CMake-Versionen prüfen. Bestehende Alt-Testfehler gesondert erfassen. | 30–50 % |
| 1. vcpkg durch CPM ersetzen | CPM und passende Dependency-Dateien übernehmen, Versionen festlegen, Bibliotheks-Targets anpassen, Quellcache und Compiler-Cache einrichten. Ein frischer Qt5-Build ohne vcpkg dient als erster Vergleichspunkt. | 90–150 % |
| 2. Qt6-Build und allgemeine APIs | CMake-Targets und Qt-Module umstellen; entfernte APIs für Bildschirme, Textkodierung, reguläre Ausdrücke und String-Ansichten ersetzen; Qt6-Typänderungen, Aktionen, Shortcuts und Eingabeereignisse anpassen. | 80–130 % |
| 3. Eigene UI-Funktionen portieren | Audio-Vorschau, VectorIcon/DPI-Cache, Theme/QSS, Quelltextanzeige, Preferences und Popup-Platzierung anpassen. Inspector-Docking, Layer-Baum, Prefabs und Action-/Toolbar-Editor funktional prüfen. | 60–110 % |
| 4. Renderfenster integrieren | OpenGL-Module, Kontextformat, sRGB, geteilte Kontexte, Framebuffer und Freigabe der Ressourcen prüfen. 2D/3D, Layoutwechsel, Bounds, Grid, animierte Links, Vorschauen und FPS-Limit vergleichen. Danach steht der erste Qt6-Integrationsmeilenstein. | 60–100 % |
| 5. Regressionen und Darstellung | Wiederholte Dokumentwechsel, Undo/Redo/History, Docking/Floating, gespeicherte Fensterlagen, Drag-and-drop, Fonts und Icons bei unterschiedlichen Skalierungen prüfen und gefundene Abweichungen beheben. | 40–80 % |
| 6. Auslieferung und Abschluss | Qt6-Frameworks und Plugins inklusive Audio in das macOS-Bundle aufnehmen; Build-Anleitung und macOS-Buildskript aktualisieren. Abschließender Build aus leerem Build-Verzeichnis; vcpkg-Submodul, Manifest und überholte Overlay-Konfiguration entfernen. | 40–80 % |
| **Geplante Umsetzung** | **macOS-arm64 mit relevanten Tests und lokalem App-Bundle** | **400–700 %** |
| **Reserve** | Unerwartete Bibliotheks-/Plugin-Probleme sowie DPI-, Kontext- und Plattformabweichungen | **100–200 %** |
| **Gesamter Planungsrahmen** | **etwa 5–9 Kontingente** | **500–900 %** |

**Meilenstein A:** Nach Schritt 1 läuft der Editor mit CPM und dem bisherigen Qt.
Das kostet zusammen mit der Vorbereitung voraussichtlich **120–200 %**. An diesem
Punkt lässt sich der weitere Aufwand wesentlich zuverlässiger einordnen.

**Meilenstein B:** Nach Schritt 4 läuft die Qt6-Fassung mit geprüften Kernfunktionen
und Renderfenstern. Kumuliert sind dafür **320–540 %** ohne Reserve angesetzt.

**Meilenstein C:** Nach Schritt 6 sind die relevanten Regressionen geprüft, das
macOS-Bundle startet und der aktive Build kommt vollständig ohne vcpkg aus.

Die beiden Build-Verzeichnisse bleiben während der Migration getrennt. Änderungen
werden thematisch lokal committet; der Vergleichsstand bleibt bis zum erfolgreichen
Abschluss verfügbar.

## Konkrete Anpassungsstellen im Fork

### Build und Bibliotheken

`CMakeLists.txt` setzt die vcpkg-Toolchain direkt. Zusätzlich hängen
`common/CMakeLists.txt`, `app/CMakeLists.txt`, die Test-/Hilfsprogramme und die
CI-Skripte an den bisherigen Targets und Qt5-Plugin-Namen.

Die installierten Bibliotheksversionen im aktuellen Release-Build unterscheiden
sich erheblich von den Pins des Original-Clones:

| Bibliothek | Aktueller Fork-Build | CPM-Pin im Original-Clone |
| --- | --- | --- |
| Assimp | 6.0.4 | 6.0.2 |
| Catch2 | 2.13.8 | 3.10.0 |
| fmt | 12.2.0 | 11.2.0 |
| FreeImage | 3.18.0 | 3.19.11, Fork mit eigener CMake-Unterstützung |
| FreeType | 2.14.3 | 2.13.3 |
| miniz | 3.1.2 | 3.1.0 |
| tinyxml2 | 11.0.0 | 9.0.0 |

Die erste CPM-Etappe legt deshalb kompatible Versionen gezielt fest. Sie kopiert
nicht ungeprüft sämtliche Versions-Pins des Originals. Insbesondere wird ein
Catch2-Major-Upgrade als eigenes Arbeitspaket behandelt. Die FreeImage-Vorlage
muss mit den benötigten Bildformaten und der statischen Einbindung geprüft werden.

Unser Renderer verwendet weiterhin GLEW; dafür braucht die CPM-Konfiguration eine
zusätzliche Dependency-Definition. Das Original nutzt inzwischen eigene
MiniGl-Dateien. Eine Umstellung des GL-Loaders ist für die CPM-Methode nicht
erforderlich. Der vcpkg-Eintrag für Boost-Stacktrace wird auf tatsächliche Nutzung
geprüft, bevor über seine Übernahme entschieden wird.

CPM cached heruntergeladene Quellen. Ein neues Build-Verzeichnis kompiliert die
Bibliotheken grundsätzlich erneut; `ccache` ergänzt deshalb den Quellcache um die
Wiederverwendung von Compiler-Ergebnissen. Beide Caches und ein frischer Build
gehören zur Abnahme. [CPM zu Cache und Einschränkungen](https://github.com/cpm-cmake/CPM.cmake#limitations).

### Qt und die eigenen Erweiterungen

- **Textkodierung:** `View/Theme/QtUtils.cpp` verwendet `QTextCodec`. Quake-Strings und
  8-Bit-Zeichen müssen beim Lesen und Schreiben erhalten bleiben. Weitere
  Fundstellen sind `QRegExp`, `midRef` und `QTextStream::setCodec` in MapFrame,
  QSSBuilder, PrefabLibrary, NodeSourceDialog und dump-shortcuts.
- **Audio:** `EntityAudioPreview` verwendet `QMediaPlaylist`, die Qt6 entfernt hat.
  Quelle, Loop, Lautstärke, Fehlerbehandlung und explizite Audio-Ausgabe werden auf
  das Qt6-Modell übertragen; Dateien aus Archiven und temporäre Dateien werden
  mitgeprüft. [Qt Multimedia-Portierung](https://doc.qt.io/qt-6/qtmultimedia-changes-qt6.html).
- **Icons und Schrift:** `IO/VectorIcon.cpp` enthält ausdrücklich Qt5-spezifische
  Behandlung für skalierte Pixmaps. Angeforderte Größe, Pixeldichte, Cache-Key,
  deaktivierte Zustände und Helligkeitsnormalisierung müssen zusammenpassen.
  Qt6 aktiviert High-DPI grundsätzlich und behandelt gebrochene Skalierungsfaktoren
  anders. [Qt-Portierungsleitfaden](https://doc.qt.io/qt-6/portingguide.html).
- **Renderfenster:** `RenderView` basiert bereits auf `QOpenGLWidget`. Unter Qt6
  werden die Module `OpenGL` und `OpenGLWidgets` explizit eingebunden. Die im Fork
  verwendeten älteren GL-Aufrufe und GLSL-1.20-Shader benötigen weiterhin einen
  passenden Kontext. Qt6 allein macht daraus keine neue Render-Architektur.
  [Qt OpenGL-Portierung](https://doc.qt.io/qt-6/opengl-changes-qt6.html).
- **Tests:** Der aktuelle Fork hat auch unabhängige Alt-Testbaustellen, etwa
  verbliebene `vecmath/`-Includes im zentralen Catch2-Helper. Diese werden bereits
  vor der Portierung erfasst. Für die Migration sind reproduzierbare Tests der
  betroffenen Funktionen verbindlich; eine vollständige Modernisierung aller
  älteren Tests wird gesondert geschätzt.

Im Original ist die historische Qt6-Portierung als Commit `1066dfe67` vorhanden
(39 Dateien, 234 hinzugefügte und 280 entfernte Zeilen). Das liefert eine konkrete
Vorlage für allgemeine API-Änderungen. Die eigenen Erweiterungen und deren
Darstellungsverhalten benötigen darüber hinaus Prüfung im laufenden Editor.

## Zusätzlicher Umfang

| Erweiterung | Zusätzlicher Aufwand |
| --- | ---: |
| Windows und Linux: Buildskripte, CI, Pakete, erforderliche Plugins und grundlegende Plattformtests; passende Runner/Testumgebungen vorausgesetzt | 150–300 % |
| Catch2 2 → 3 und Anpassung der zugehörigen Test-API, falls im selben Projektabschnitt gewünscht | 60–120 % |
| Weitere Angleichung der Dependency-Versionen bzw. des GL-Loaders | Nach den gewählten Versionswechseln separat schätzen |

**macOS plus Windows/Linux liegt damit grob bei 650–1200 %, also etwa
7–12 einzuplanenden Kontingenten.** Zusatzpakete werden nur einmal auf den
Basisrahmen aufgeschlagen; die interne Reserve ist dort bereits enthalten.

Für Qt 6.10 nennt Qt **macOS 13 oder neuer** als unterstützte Zielplattform. Das
bisherige CI-Skript setzt noch macOS 10.15. Diese Produktentscheidung muss am Anfang
festgelegt werden; ein Bedarf an älteren macOS-Versionen verändert die Wahl des
Qt-Ziels und die Schätzung. [Unterstützte Qt-6.10-Plattformen](https://doc.qt.io/qt-6.10/supported-platforms.html).

Apple-Intel-/Universal-Bundles, Signierung und Notarisierung sowie reine Wartezeiten
auf externe Testumgebungen sind im macOS-arm64-Basisrahmen nicht kalkuliert.

## Abnahme des Basisumfangs

1. Aus einem leeren Build-Verzeichnis lässt sich die App mit den festgelegten Qt-
   und CPM-Versionen bauen; dabei wird kein vorhandenes vcpkg-Verzeichnis benutzt.
2. Ein erneuter Configure-/Build-Lauf verwendet die vorgesehenen Caches.
3. Die gebündelte App startet ohne Verweis auf lokal installierte Qt-Frameworks.
4. Renderfenster, Layoutwechsel, FPS-Limit, Bounds und Link-Animationen funktionieren.
5. Docking, Layer-Auswahl, Prefabs, History, Action-/Toolbar-Einstellungen und die
   Audio-Vorschau sind geprüft; relevante Regressionstests laufen erfolgreich.
6. Einstellungen, Dateien und die zuvor angepassten Oberflächen bleiben nutzbar;
   insbesondere werden Map-Zeichenkodierung und DPI-Darstellung kontrolliert.

Die Schätzung ist am sichersten bei den allgemeinen Qt-API-Anpassungen. Den größten
Spielraum haben FreeImage-/Plugin-Paketierung, Audio, High-DPI und die Prüfung der
eigenen Werkzeuge. Nach Meilenstein A wird der verbleibende Rahmen anhand eines
tatsächlich gebauten CPM-Standes aktualisiert.
