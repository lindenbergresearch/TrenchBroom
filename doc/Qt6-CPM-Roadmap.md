# Roadmap: Qt 6 und CPM.cmake

Stand: **18. November 2026**. Die technische macOS-arm64-Basisumstellung auf
**Qt 6.10.1 und CPM.cmake ist umgesetzt**. Der Editor baut, die automatisierten
Regressionen und nativen Render-/Audio-Prüfungen bestehen, und das gebündelte
Programm lädt eine Testmap und beendet sich regulär.

**Aktualisierte Restplanung für macOS: 40–80 %, also 0,4–0,8 eines
5h-Kontingents für manuelle Abnahme und daraus entstehende gezielte Nacharbeit.**
Die frühere Restschätzung von 350–650 % ab Meilenstein A gilt nicht mehr für die
bereits erledigte Portierung. Windows-/Linux-Auslieferung bleibt separat mit
**150–300 %** geschätzt. Zusammen mit der macOS-Abnahme ergibt das **190–380 %**;
praktisch wären dafür etwa **2–4 weitere Kontingente** einzuplanen.

Die gewünschte Vergleichseinheit bleibt **100 % = ein volles 5h-Kontingent**.
Diese Zahlen beschreiben geschätzten verbleibenden Arbeitsumfang, keine Messung
des tatsächlich verbrauchten Kontingents. Compiler-/Downloadzeiten und das
Kontingent des Benutzerkontos sind daraus nicht zuverlässig abzuleiten.

## Umgesetzter Stand

| Bereich | Ergebnis |
| --- | --- |
| Abhängigkeiten | CPM 0.43.1, feste Quellarchive mit SHA-256, getrennte Quell- und Compiler-Caches; kein aktives vcpkg |
| Qt | Qt 6.10.1, explizite OpenGL-/OpenGLWidgets-Module, Release-/Debug-Presets |
| Allgemeine APIs | Textkonverter, String-Ansichten, reguläre Ausdrücke, Bildschirme, Eingabeereignisse, Signale und Shortcuts portiert |
| Eigene UI | VectorIcon/DPI-Cache, Theme/QSS, Fonts, Properties, Quelltextdialog, Prefabs und Audio angepasst |
| Audio | QMediaPlayer mit QAudioOutput; Einmal-/Loop-Wiedergabe, Lautstärke, Stoppen und temporäre Dateien geprüft |
| Renderer | Bestehender OpenGL-Renderer bleibt erhalten; Kontextlebensdauer und Floating von OpenGL-Widgets nativ geprüft |
| macOS-Bundle | Frameworks, Plugins und transitive Bibliotheken gebündelt, interne Pfade geprüft, lokal signiert und Signatur verifiziert |
| CLion | Frische Qt6-Konfiguration mit enthaltenem CMake 4.3.1 und Ninja erfolgreich; Kontroll-Build von fmt erfolgreich |
| Alte Build-Dateien | vcpkg-Submodulverweis, Manifest und Overlays entfernt; vorhandener lokaler vcpkg-Ordner samt Änderungen erhalten |
| Plattformskripte | Qt6-Anpassung vorbereitet; Windows-/Linux-Läufe und Pakete noch nicht ausgeführt |

Die Migration liegt im Branch `codex/qt6-cpm`. Änderungen werden thematisch lokal
committet; ein Push erfolgt durch den Benutzer. Die vorherige Qt5/CPM-Etappe ist
in `af78068ba` dokumentiert. Referenz vor der eigentlichen Qt6-Portierung war
`cd3b05b96`; ein Qt5-App-Bundle wurde für den Vergleich gesichert.

## Praktisch geprüfte Ergebnisse

- Frischer Qt6-Release-Build mit Apple Clang 21, arm64 und CMake 4.2.3. Qt5- und
  vcpkg-Build-Verzeichnisse werden vor einer inkompatiblen Wiederverwendung
  zurückgewiesen. Ein zweites Verzeichnis wurde mit CLions CMake offline aus
  dem gemeinsamen CPM-Quellcache konfiguriert.
- **223 Regressionstests / 75.793 Assertions erfolgreich.** Enthalten sind
  vorhandene Prüfungen für Themes, Fonts/SVG/DPI, Actions/Toolbars, Docking,
  Layer-Selektion, Prefabs, WAD-Properties, History/Undo, Ansichten und Caches.
  Neu abgedeckt sind Qt6-Textkodierung mit eingebetteten Nullbytes und
  Unicode-Zeichen sowie unvollständigen Zeichenfolgen, Shortcut-Serialisierung, alte QSS-Schriftgewichte, gebrochene
  Eingabekoordinaten und portable Einstellungspfade.
- Je **29 Schrift-/Icon-/Qt6-Testfälle mit 3.240 Assertions** zusätzlich bei
  150 % und 200 % Skalierung erfolgreich. Auch SVGs aus der früheren Bitmap-
  Ladefunktion verwenden den gemeinsamen VectorIcon-Cache und die aktuelle
  Pixeldichte.
- **Drei zusätzliche native Tests / 61 Assertions erfolgreich:** zwei
  OpenGL-Kontext-/Floating-Regressionen und echte, stummgeschaltete Audio-
  Wiedergabe einschließlich Einmal-/Loop-Wechsel, Stoppen und Dateifreigabe.
  Diese Tests benötigen eine angemeldete Desktop-Sitzung und gehören deshalb
  nicht zum Offscreen-Lauf.
- Zusätzliche native Prüfung mit echten Editor-Komponenten: 3D-Framebuffer ohne
  OpenGL-Fehler, Properties-Bearbeitung mit tatsächlichem Übernehmen des Werts,
  Inspector-Floating/Docking, Live-Farben einschließlich ANSI-Ausnahmen,
  Preferences-Toolbar sowie skalierbare SVG-Auswahl und Vorschau.
- Das App-Bundle lädt eine Testmap von außerhalb seines Arbeitsverzeichnisses,
  verwendet laut Laufzeitprotokoll seine eigene QtCore und beendet sich regulär.
  Der portable Modus erhält seinen absoluten Datenpfad bereits vor der ersten
  OpenGL-Preference-Abfrage. Der zuvor dadurch ausgelöste Lock-/Fehlerdialog ist
  korrigiert.
- **76 Mach-O-Dateien / 659 Bibliotheksbezüge geprüft:** keine fehlende,
  ungelöste oder externe Nicht-System-Abhängigkeit. `codesign --verify --deep
  --strict` ist erfolgreich. Die Paketierung unterstützt Homebrews aufgeteilte
  Qt6-Module und ersetzt beim Wiederholen ihr eigenes generiertes Plugin-/
  Framework-Verzeichnis.

Aufrufe, Voraussetzungen und Grenzen stehen in [Build.md](../Build.md).
Die historische Gesamttestsuite ist damit weiterhin nicht vollständig portiert;
der gepflegte Migrationstest-Target ist `tb-regression-tests`.

## Restumfang und präzisierte Schätzung

| Noch offene Arbeit | Geschätzter Aufwand |
| --- | ---: |
| Manuelle Abnahme: komplexe eigene Maps, längeres Arbeiten, Drag-and-drop, mehrere Bildschirme/Skalierungen; gezielte Korrekturen nach Rückmeldung eingeschlossen | 40–80 % |
| **Rest des macOS-Basisumfangs einschließlich Abnahmereserve** | **40–80 %** |
| Windows/Linux: echte Build-/CI-Läufe, benötigte Plugins und Codecs, Linux-Qt-Auslieferung und Paketabhängigkeiten, Paket- und Startprüfungen | 150–300 % zusätzlich |
| Catch2 2 → 3 und Anpassung der gesamten betroffenen Test-API, falls gewünscht | 60–120 % zusätzlich |
| Intel-/Universal-Bundles, Notarisierung, größere Dependency- oder GL-Loader-Wechsel | Separat nach Zielumfang |

Die Restschätzung ist deutlich kleiner, weil die zuvor unsicheren Bereiche
Qt6-Audio, SVG/DPI, Renderkontexte und macOS-Plugin-Abhängigkeiten jetzt praktisch
geprüft sind. Das ist keine Behauptung, dass jede individuelle Map oder jedes
Interaktionsdetail bereits manuell abgenommen wurde. Ein gemessener Performance-
Vergleich und eine inhaltliche Überarbeitung der Beleuchtung gehören nicht zur
Qt-Migration.

## Etappen und ursprünglicher Plan

Die folgenden Werte dokumentieren die ursprüngliche Planung und werden nicht
noch einmal zum Rest addiert.

| Schritt | Ursprüngliche Schätzung | Aktueller Stand |
| --- | ---: | --- |
| 0. Referenz und Testbasis | 30–50 % | Erledigt; Qt5-Vergleichsbundle und gepflegte Regressionen vorhanden |
| 1. vcpkg durch CPM ersetzen | 90–150 % | Erledigt; Meilenstein A |
| 2. Qt6-Build und allgemeine APIs | 80–130 % | Erledigt |
| 3. Eigene UI-Funktionen portieren | 60–110 % | Erledigt und automatisiert/nativ geprüft |
| 4. Renderfenster integrieren | 60–100 % | Erledigt; technischer Meilenstein B |
| 5. Regressionen und Darstellung | 40–80 % | Automatisierte/nativ ausführbare Prüfungen erledigt; manuelle Abnahme bleibt |
| 6. Auslieferung und Abschluss | 40–80 % | macOS-Bundle und Build-Anleitung erledigt, vcpkg-Konfiguration entfernt |
| Reserve im ursprünglichen Gesamtplan | 100–200 % | Durch bekannte Ergebnisse ersetzt; aktuelle Restreserve siehe oben |
| **Ursprünglicher Gesamtrahmen** | **500–900 %** | Kein zusätzliches Restbudget |

**Meilenstein A:** CPM mit Qt5 war bereits umgesetzt. Der ursprüngliche Rest
von 350–650 % beschrieb die damals noch ausstehende Qt6-Portierung.

**Meilenstein B:** Die Qt6-Fassung mit geprüften Kernfunktionen und Renderfenstern
ist jetzt erreicht.

**Meilenstein C:** Die technischen Abschlusskriterien für macOS — geprüfte
Regressionen, startbares signiertes Bundle und Entfernung der alten vcpkg-
Projektkonfiguration — sind erreicht. Die persönliche visuelle Abnahme bleibt
der abschließende Praxisschritt.

## Architektur und Versionsentscheidung

Das Ziel folgt der Build-Methode des eingebundenen Original-Clones
`1cdbf7236`: **Qt 6.10.x separat installieren, native Bibliotheken mit CPM.cmake
und CMake FetchContent bauen**. CMake benötigt mindestens 3.28. Qt selbst wird
nicht durch CPM gebaut. Die historische Qt6-Portierung des Originals
`1066dfe67` diente als Referenz für allgemeine API-Anpassungen; eigene Funktionen
wurden gezielt portiert, ohne fremde UI-/Renderer-Architektur zu übernehmen.

Die erste CPM-Etappe änderte bewusst nur FreeImage von 3.18.0 auf den CMake-
fähigen Fork 3.19.11. Die übrigen Pins bleiben gegenüber dem vorherigen Fork
bestehen: Assimp 6.0.4, Catch2 2.13.8, fmt 12.2.0, FreeType 2.14.3, GLEW 2.3.1,
miniz 3.1.2 und tinyxml2 11.0.0. Die bisherigen Bild-/Modell-/Schriftlader werden
weiter verwendet. Der Renderer bleibt bei GLEW und seinen bestehenden Shadern;
Qt6 allein ersetzt diese Renderarchitektur nicht.

CPM cached die Quellen in `.cache/cpm`, `ccache` die Compiler-Ergebnisse in
`.cache/ccache`. Beide sind unabhängig von einzelnen Build-Verzeichnissen.
Qt5- und Qt6-Compiler-/Autogen-Dateien bleiben strikt getrennt. Persönliche
Stylesheets mit `systemFontWeight: 50/75` bleiben kompatibel; neue QSS verwenden
Qt6-/CSS-Werte wie 400/700. [Stylesheet-Dokumentation](UI-Stylesheets.md).

## macOS-Zielsystem und offene Plattformgrenzen

Qt 6.10 unterstützt grundsätzlich **macOS 13+**. Das Projekt setzt für den Editor
standardmäßig macOS 14 als Compilerziel. **Die lokal installierten Homebrew-
Binärpakete bestimmen jedoch eine höhere reale Untergrenze:** QtCore nennt
macOS 14, QtSvg und QtMultimedia bereits macOS 26. Der geprüfte lokale Build ist
daher kein Nachweis für ein auf macOS 13/14 lauffähiges Bundle. Dafür muss ein
vollständiges Qt SDK mit entsprechendem Deployment Target verwendet und die
App damit neu gebaut werden.
[Unterstützte Qt-6.10-Plattformen](https://doc.qt.io/qt-6.10/supported-platforms.html).

Windows nutzt künftig MSVC 2022/x64 und Qt6s `windeployqt`; frühere Qt5-Plugin-
Listen und die Win32-CI sind entfernt. Die vorbereitete CI verwendet Qt 6.10.1,
Ubuntu 24.04, macOS 15 und Windows 2022. Diese Änderungen ersetzen keine echten
Windows-/Linux-Build-, Start- und Paketprüfungen. Insbesondere die Linux-
Paketierung mit separat installiertem Qt6 ist noch zu validieren.

## Abnahme des Basisumfangs

| Kriterium | Stand |
| --- | --- |
| Frischer Build ohne vcpkg | Bestanden |
| Offline-Konfiguration und getrennte Caches | Bestanden |
| Gebündelte App lädt eigene Qt6-Bibliotheken und eine Testmap | Bestanden |
| OpenGL-Kontexte, tatsächlicher Framebuffer und Floating | Bestanden |
| Relevante UI-/Daten-/Eingabe-Regressionen und natives Audio | Bestanden |
| Textkodierung, QSS-Kompatibilität und SVG-Dichte | Bestanden |
| Persönliche Maps, komplexe Workflows und verschiedene Monitor-Setups | Manuelle Abnahme offen |

Die Benutzerdaten und alten lokalen Build-Verzeichnisse werden nicht gelöscht.
Der entfernte Submodulverweis betrifft ausschließlich die Projektkonfiguration;
der vorhandene ignorierte vcpkg-Arbeitsordner samt seinen Änderungen bleibt als
lokale Referenz erhalten.
