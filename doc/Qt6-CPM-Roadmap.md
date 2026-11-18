# Roadmap: Qt 6 und CPM.cmake

Stand: 18. November 2026. **Meilenstein A ist umgesetzt:** Vorbereitung und
Umstellung auf CPM, weiterhin mit Qt 5.15.18. Die eigentliche Qt6-Portierung
(Schritte 2–6) steht noch aus.

**Aktualisierte Restschätzung für macOS-arm64: 350–650 %, also etwa
3,5–6,5 volle 5h-Kontingente einschließlich Reserve.** Praktisch würde ich
**4–7 Kontingente einplanen**, mit **500 % / fünf Kontingenten** als Arbeitsbudget
für die verbleibende Migration. Der ursprüngliche Gesamtrahmen vor Beginn war
500–900 %. Der Aufwand der abgeschlossenen Etappe ist darin bereits enthalten
und wird nicht erneut zum Rest addiert.

Die Prozente verwenden die gewünschte Vergleichseinheit: **100 % = ein volles
5h-Kontingent, 200 % = zwei Kontingente**. Sie sind eine grobe Aufwandsschätzung,
keine Messung oder Zusage des tatsächlichen Kontingentverbrauchs. Download- und
Compilerwartezeiten machen daraus keinen verlässlichen Kalenderzeitplan.

## Ergebnis der ersten Etappe

- Referenzstand `8c103d6ea` und sein Qt5-App-Bundle gesichert; Migrationsbranch
  `codex/qt6-cpm` angelegt. Build-Umstellung in Commit `af78068ba`.
  Vorhandene persönliche Änderungen bleiben getrennt.
- CPM **0.43.1** eingebunden, acht Bibliotheken auf feste Quellarchive mit
  SHA-256-Prüfsummen festgelegt. Qt bleibt separat installiert. CMake benötigt
  jetzt mindestens **3.28**; der konkrete Build verwendet **4.2.3**.
- Frischer Release-Build mit Apple Clang 21, arm64 und Qt **5.15.18** erfolgreich.
  In Cache, Compiler- und Link-Aufrufen des neuen Builds gibt es keine
  vcpkg-Verweise. Alte Build-Verzeichnisse werden vor dem Laden der vcpkg-Toolchain
  zurückgewiesen; Submodul, Manifest und Overlays bleiben vorerst Referenzdateien.
- Quellcache `.cache/cpm` und Compiler-Cache `.cache/ccache` eingerichtet. Ein
  zweites, leeres Build-Verzeichnis lässt sich offline konfigurieren. Dessen
  Kontroll-Build von fmt liefert **zwei direkte Cache-Treffer ohne neue
  Kompilierung**. Assimp darf die globalen Compiler-Launcher nicht mehr verändern.
- **177 Regressionstests / 74.650 Assertions erfolgreich**, als reproduzierbares
  Ziel `tb-regression-tests` im Projekt. Der gemeinsame Catch2-Helper und fehlende
  direkte Includes der ausgewählten Tests sind korrigiert. Die historische
  Gesamttestsuite ist damit nicht vollständig modernisiert.
- **Zwei zusätzliche native OpenGL-Regressionen / 43 Assertions erfolgreich**,
  darunter Shader-Initialisierung nach dem Schließen eines Editors und Floating
  eines OpenGL-Browsers. Zusätzlich native Cocoa-Prüfung mit den echten
  Editor-Komponenten:
  Renderfenster, Dokument/Properties-Bearbeitung, Inspector-Floating/Docking,
  Fonts, Themes und SVG-Vorschau. Das Qt5-App-Bundle lädt eine Testmap mit seinen
  gebündelten Qt-Frameworks. Paketierung ergänzt um transitive Plugin-Bibliotheken,
  Bereinigung kopierter Finder-Metadaten sowie lokale Signatur und deren Prüfung.
- Build-Anleitung, macOS-Skript und gemeinsamer CI-Cache-Schritt auf CPM angepasst.
  Die Handbuch-Erzeugung läuft ohne Desktop-Zugriff und ohne persönliche
  Editor-Einstellungen. Windows-/Linux-Läufe sind in dieser Etappe nicht geprüft.

FreeImage ist die einzige hier geänderte Bibliotheksversion: **3.18.0 → 3.19.11**
(CMake-fähiger Fork wie im Original). Die bisherigen anderen Versionen bleiben
bestehen, einschließlich Catch2 2.13.8 und GLEW 2.3.1. OpenEXR, RAW und JPEG-XR
bleiben aktiviert; für JPEG-XR sind die unvollständige CMake-Quellliste und kleine
Compiler-Kompatibilitätsprobleme der Bibliothek ergänzt. Die älteren Codecs werden
allein in C++14 gebaut, der Editor weiterhin in C++20. Bildformate, Farb-/Alpha-
Daten, Modelle samt externen Texturen und die Renderer-Schriften sind geprüft.

Aufrufe und Grenzen der Testbasis stehen in [Build.md](../Build.md).

## Aktualisierte Restschätzung

| Offene Arbeit | Aufwand ab jetzt |
| --- | ---: |
| Qt6-Build und allgemeine APIs | 80–130 % |
| Eigene UI-Funktionen portieren | 60–110 % |
| Renderfenster integrieren | 60–100 % |
| Regressionen und Darstellung | 40–80 % |
| Qt6-Auslieferung und Abschluss | 40–80 % |
| **Verbleibende Umsetzung** | **280–500 %** |
| **Verbleibende Reserve** | **70–150 %** |
| **Verbleibender Planungsrahmen** | **350–650 %** |

Die Schätzung ist nur vorsichtig enger: Bibliotheks-Build, Cache-Verhalten und
mehrere macOS-Paketierungsprobleme sind jetzt praktisch geklärt. Die offenen
Qt6-Arbeitspakete bleiben unverändert geschätzt; Audio-Backend, Renderkontexte,
High-DPI und die eigenen UI-Erweiterungen sind noch nicht unter Qt6 geprüft.
Deshalb sinkt hauptsächlich die Reserve gegenüber dem früheren Restansatz von
380–700 %. Es wird kein gemessener Kontingentverbrauch dieser Sitzung behauptet.

Der nächste konkrete Schritt ist **Schritt 2: Qt6-Build und allgemeine APIs**.
Bis zum Qt6-Integrationsmeilenstein B sind ab jetzt **200–340 %** für die Schritte
2–4 ohne Reserve veranschlagt.

## Grundlage und Ziel

Ursprünglich verglichen wurden der Fork bei `18d4eb05d` und der eingebundene Original-Clone bei
`1cdbf7236` vom 12. November 2026. Die öffentlich verfügbare Bauanleitung bestätigt
die gleiche grundsätzliche Build-Methode.

Der frühere Referenzstand verwendete Qt **5.15.18**, C++20 und eine im obersten
CMake-Projekt fest verdrahtete vcpkg-Toolchain. Der jetzt gebaute Stand verwendet
Qt **5.15.18**, C++20 und CPM. Das Original verwendet **CPM.cmake** als Schicht
über CMake FetchContent, separate Dateien in `cmake/dependencies/`, mindestens
CMake **3.28** und separat installiertes **Qt 6.10**. Qt selbst wird auch dort
nicht durch CPM gebaut. Quellen werden über `CPM_SOURCE_CACHE` wiederverwendet.
[Original-Bauanleitung](https://raw.githubusercontent.com/TrenchBroom/TrenchBroom/master/BUILD.md),
[CPM-Dokumentation](https://github.com/cpm-cmake/CPM.cmake).

Empfohlenes Ziel ist deshalb **Qt 6.10.x plus CPM.cmake** in der bestehenden
Projektstruktur. Lokal ist Qt 6.10.1 installiert; die benötigten Module und Plugin-Abhängigkeiten
werden zu Beginn von Schritt 2 erneut geprüft. Übernommen werden
gezielt Build-Definitionen und passende Qt-Portierungen des Originals.

Der Basisumfang umfasst den macOS-arm64-Editor, seine vorhandenen Eigenentwicklungen,
relevante Regressionstests und ein lokal startbares App-Bundle. Windows-/Linux-
Builds und deren Auslieferung sind unten separat geschätzt.

## Ursprüngliche Reihenfolge und Aufwand

Die folgende Tabelle dokumentiert die Planung vor Beginn der Migration. Schritte
0 und 1 sind inzwischen abgeschlossen. Für weitere Arbeit gilt die Restschätzung
oben; die abgeschlossenen Zeilen werden nicht nochmals addiert.

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

**Meilenstein A: abgeschlossen.** Der Editor läuft mit CPM und dem bisherigen Qt.
Vorbereitung und CPM-Umstellung waren ursprünglich mit **120–200 %** angesetzt.
Der jetzt präzisierte Rest steht oben.

**Meilenstein B:** Nach Schritt 4 läuft die Qt6-Fassung mit geprüften Kernfunktionen
und Renderfenstern. Ursprünglich kumuliert waren **320–540 %** ohne Reserve
angesetzt; ab dem abgeschlossenen Meilenstein A bleiben **200–340 %**.

**Meilenstein C:** Nach Schritt 6 sind die relevanten Regressionen geprüft, das
macOS-Bundle startet und die alten vcpkg-Referenzdateien können entfernt werden.
Der aktive Qt5/CPM-Build kommt bereits seit Meilenstein A ohne vcpkg aus.

Die beiden Build-Verzeichnisse bleiben während der Migration getrennt. Änderungen
werden thematisch lokal committet; der Vergleichsstand bleibt bis zum erfolgreichen
Abschluss verfügbar.

## Konkrete Anpassungsstellen im Fork

### Build und Bibliotheken

`CMakeLists.txt` bindet jetzt `cmake/Dependencies.cmake` ein. Die nativen
Bibliotheken werden statisch gebaut; kompatible CMake-Target-Namen vermeiden
unnötige C++-Änderungen. `common/CMakeLists.txt`, `app/CMakeLists.txt`, die
Test-/Hilfsprogramme und CI-Skripte verwenden weiterhin Qt5-Targets und
Plugin-Namen, die in den nächsten Etappen umgestellt werden.

Die Versionen des vcpkg-Referenzbuilds unterscheiden sich von den Pins des
Original-Clones. Für CPM wurden die bisherigen Versionen übernommen, mit der
oben beschriebenen FreeImage-Ausnahme:

| Bibliothek | vcpkg-Referenzbuild | CPM-Pin im Original-Clone |
| --- | --- | --- |
| Assimp | 6.0.4 | 6.0.2 |
| Catch2 | 2.13.8 | 3.10.0 |
| fmt | 12.2.0 | 11.2.0 |
| FreeImage | 3.18.0 | 3.19.11, Fork mit eigener CMake-Unterstützung |
| FreeType | 2.14.3 | 2.13.3 |
| miniz | 3.1.2 | 3.1.0 |
| tinyxml2 | 11.0.0 | 9.0.0 |

Die erste CPM-Etappe hat kompatible Versionen gezielt festgelegt. Ein
Catch2-Major-Upgrade bleibt ein eigenes Arbeitspaket. Die statische FreeImage-
Einbindung und benötigten Bildformate sind mit dem neuen Build geprüft.

Unser Renderer verwendet weiterhin GLEW; dafür enthält die CPM-Konfiguration eine
eigene Dependency-Definition mit dem vollständigen Release-Archiv. Das Original nutzt inzwischen eigene
MiniGl-Dateien. Eine Umstellung des GL-Loaders ist für die CPM-Methode nicht
erforderlich. Boost-Stacktrace ist im Quellcode nicht aktiv verwendet und wird deshalb nicht
als zusätzliche CPM-Abhängigkeit übernommen.

CPM cached heruntergeladene Quellen. Ein neues Build-Verzeichnis kompiliert die
Bibliotheken grundsätzlich erneut; `ccache` ergänzt deshalb den Quellcache um die
Wiederverwendung von Compiler-Ergebnissen. Beide Caches und ein frischer Build
sind in Meilenstein A geprüft. [CPM zu Cache und Einschränkungen](https://github.com/cpm-cmake/CPM.cmake#limitations).

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
- **Tests:** Der zentrale Catch2-Helper ist auf `vm/` korrigiert und die
  ausgewählten Tests werden getrennt kompiliert. Andere historische Testdateien
  haben weiterhin API-Abweichungen. Für die Migration sind die reproduzierbaren
  Tests der betroffenen Funktionen verbindlich; eine vollständige Modernisierung
  aller älteren Tests wird gesondert geschätzt.

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

**Ab dem abgeschlossenen Meilenstein A liegt der Rest für macOS plus
Windows/Linux grob bei 500–950 %, also etwa 5–10 einzuplanenden Kontingenten.**
Der ursprüngliche Gesamtrahmen vor Beginn war 650–1200 %. Zusatzpakete werden nur
einmal auf den aktuellen Basisrahmen aufgeschlagen; die interne Reserve ist darin
bereits enthalten.

Für Qt 6.10 nennt Qt **macOS 13 oder neuer** als unterstützte Zielplattform. Das
bisherige CI-Skript setzt noch macOS 10.15. Diese Produktentscheidung betrifft den Qt6-Beginn in Schritt 2; ein Bedarf an
älteren macOS-Versionen verändert die Wahl des Qt-Ziels und die Schätzung. [Unterstützte Qt-6.10-Plattformen](https://doc.qt.io/qt-6.10/supported-platforms.html).

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

Die Bibliotheks-Umstellung und Qt5-Paketierung sind nun praktisch abgesichert.
Den größten verbleibenden Spielraum haben Qt6-Audio/Plugin-Paketierung,
Renderkontexte, High-DPI und die Prüfung der eigenen Werkzeuge. Nach dem ersten
Qt6-Integrationsbuild wird der Rest erneut anhand konkreter Ergebnisse eingegrenzt.
