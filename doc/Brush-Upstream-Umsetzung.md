# Umsetzung des Brush-Abgleichs

Referenz: [Abgleich](Brush-Upstream-Abgleich.md), eingebundener Original-Stand `1cdbf7236`.

Die Änderungen werden auf die vorhandene Qt-5-/MapDocument-Struktur übertragen. Bereits bestehende Anpassungen des Forks bleiben erhalten.

- [x] Stabilität: konvexe Hüllen, Hohlzylinder-Wandstärke, Ico-Parameter, veraltete Clip-Treffer; Regressionstests.
- [x] Shape-Grundlage: gemeinsame Parameter, Achse, Apply mit sicherer Auswahlregel, Gruppierung, Icons.
- [x] Generatoren: skalierbare Rundformen, Treppen, Bögen/Zwickel, Felsformationen.
- [x] Bearbeitung: Extrude-Picking, Berührungsnähte, Stamping, koplanare Flächenauswahl, Vertex-Transformationen.
- [x] Optimierung: flache Container, passende Messungen und Regressionstests.
- [x] Abschließender Release-Build und native Oberflächenprüfung.

Die Pool-Speicherverwaltung und der vollständige Umbau der UV-/Surface-Architektur wurden im Abgleich als gesonderte Vorhaben eingeordnet; sie sind keine Voraussetzung für diese Übernahmen.

## Stabilität

Release-Build erfolgreich. Vier Regressionstests mit 232 Prüfungen bestanden, darunter die Upstream-Map mit 119 Brushes, Undo/Redo und die Hohlzylinder-Grenzfälle in allen drei Achsen.

## Formwerkzeug

Gemeinsame Parameter, explizite/View-Achse, sichere Apply-Funktion, optionale Gruppierung und Form-Icons sind integriert. Skalierbare Rundformen, vier Treppenrichtungen, Bögen/Zwickel und alle acht Felsformationen sind verfügbar. Release-Build und zehn Regressionstests mit 4.266 Prüfungen bestanden. Apply bewahrt den Parent und passende Face-Attribute; Änderungen sind per Undo/Redo rückgängig zu machen.

## Bearbeitung

Extrude bevorzugt Kanten-Griffe nur an echten, überlappenden Berührungsnähten; gegenüberliegende Flächen werden nur bei räumlicher Überlappung mitgezogen. Stamping erzeugt aus der Fläche einen neuen Brush (Shift+Alt+Ctrl/Cmd). Shift+Alt-Doppelklick wählt zusammenhängende koplanare Flächen, Ctrl/Cmd ergänzt die Auswahl. Die Suche nutzt den räumlichen Knotenindex.

Rotate/Scale/Shear können parallel zu Vertex-/Edge-/Face-Werkzeugen verwendet werden. Matrix-Transformationen und Undo/Redo verwenden die vorhandenen Komponenten-Kommandos. Griffe und numerische Skalierung verwenden die Komponentengrenzen; Skalierbewegungen werden vom Anfangszustand neu berechnet. Degenerierte oder ungültige Transformationen werden verworfen. Release-Build und 16 Tests mit 4.360 Prüfungen bestanden.

## Zusätzliche Korrekturen an den Generatoren

Beim Übertragen des aktuellen Originals wurden zwei weitere Fehler behoben: Die UV-Sphäroid-Ringe beginnen nun nach dem oberen Pol und schließen symmetrisch ab. Beim skalierbaren Sphäroid wird die Anzahl der Ringe aus den tatsächlich berechneten Profilwerten abgeleitet. Die alte Formel las bei Präzision > 0 über die Profilvektoren hinaus und lieferte je nach Speicherinhalt unvollständige Körper. Alle Präzisionsstufen 0–3 werden geprüft.

Kardinale Achsenrotationen verwenden exakte Vorzeichenwechsel/Vertauschungen. Face-Ebenen aus Hüllen werden mit einer deterministisch gewählten, möglichst breiten Dreiecksgrundlage erzeugt. Offene oder degenerierte Hüllen werden als Fehler zurückgegeben.

## Geometrie-Optimierung

Die `flat_map`-/`flat_set`-Implementierungen aus dem Original wurden für C++20 übernommen und auf Vertex-Zuordnungen und den Polyhedron-Matcher begrenzt. Alle 301 KDL-Bibliothekstests mit 13.059 Prüfungen bestehen.

Lokale CPU-Messung (Release, Median aus fünf Durchläufen mit jeweils 100 Operationen, Zylinderoberseite auf 87,5 % skalieren, UV Lock aktiv):

| Zylinderseiten | Baum-Container | Flache Container | Zeitersparnis |
| --- | ---: | ---: | ---: |
| 8 | 13,72 ms | 13,53 ms | ca. 1 % |
| 32 | 140,24 ms | 127,31 ms | ca. 9 % |
| 64 | 506,26 ms | 454,67 ms | ca. 10 % |

Das sind gezielte Geometrie-Messungen, keine FPS-Messungen. Reproduzierbar über den ausgeblendeten Test `Brush component operation benchmark` (`[brush-benchmark]`). Die Darstellung und GPU-Auslastung wurden damit nicht bewertet.

## Abschließende Prüfung

- Release-Ziel `TrenchBroom` erfolgreich gebaut (Qt 5.15.18 bleibt erhalten).
- 106 Anwendungs-/Regressionstests mit 11.142 Prüfungen bestanden; davon 19 Brush-Tests mit 9.274 Prüfungen. Dazu gehören alle Formtypen, Rundungspräzision 0–3, gespeicherte und erneut geladene Maps, Linked-Group-Schutz sowie Undo/Redo für Vertices, Edges und Faces.
- 301 KDL-Tests mit 13.059 Prüfungen bestanden.
- Native macOS-Prüfung: acht Formseiten mit Icons, Vorschau und Erzeugung, sichtbares Apply bei Selektion, Quick Move, Komponentenskalierung/-rotation und Undo. Auch Inspector, Prefabs, QSS, Icons und Schrift geprüft; keine Fehler.

Bedienung (aktualisiert): Select ist der Grundmodus zum Auswählen, Verschieben und Navigieren. Create (`N`, über die Keymap änderbar) aktiviert die Brush-Erzeugung und zeigt die Formpalette links mittig im Viewport. Die Formparameter bleiben in der Toolbar darüber. Escape verwirft die laufende Vorschau und wechselt zu Select. Stamping: Shift+Alt+Ctrl/Cmd ziehen. Zusammenhängende koplanare Flächen: Shift+Alt doppelklicken, mit Ctrl/Cmd zur Auswahl hinzufügen. Zum Transformieren im Vertex-/Edge-/Face-Werkzeug Komponenten auswählen und zusätzlich Rotate, Scale oder Shear aktivieren.

Die alten Extrude-Picking-Tests wurden an den zusätzlichen Camera-Parameter angepasst. Alle oben genannten Prüfläufe sind unabhängig von den bereits vorher veralteten `vecmath`-Includes in Teilen der allgemeinen Testsammlung erfolgt.
