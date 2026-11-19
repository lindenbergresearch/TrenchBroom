# Kameras, Hilfslinien, History und Fenstergrößen

## Ansichten wechseln

Direktes Umschalten zwischen 2D und 3D fokussiert nicht mehr automatisch die
Selektion. Jede Kamera behält Position, Blickrichtung und Zoom. Auch beim Wechsel
zwischen Ein-, Zwei-, Drei- und Vier-Pane-Layouts werden die Zustände pro
Perspektiv-/Achsenansicht übernommen. Verknüpfte 2D-Kameras bleiben verknüpft.
Der explizite Befehl **Focus Camera on Selection** behält seine Funktion.

## Animation der Selektions-Hilfslinien

**View Settings → Grid & Bounds → Selection Bounds → Guide animation speed** regelt die Bewegung
der gestrichelten Hilfslinien zwischen 0 und 200 %. 0 % deaktiviert die Animation;
100 % entsprechen 32 logischen Pixeln pro Sekunde. Die langsamere Vorgabe ist
25 % (8 Pixel pro Sekunde). Gespeicherte Werte bleiben kompatibel.
Bei durchgezogenen Hilfslinien steuert derselbe Regler das Pulsieren.
**Reset** stellt auch diese Geschwindigkeit zurück auf die Vorgabe.

## History

Das neue Inspector-Werkzeug **History** lässt sich wie die anderen Werkzeuge
andocken, als Tab gruppieren und als eigenes Fenster platzieren. Die vorhandene
Speicherung der Werkzeuganordnung gilt auch hier.

Die Liste zeigt den tatsächlichen Undo-/Redo-Verlauf des aktuellen Dokuments mit
Bezeichnung und lokaler Uhrzeit. Zusammengefasste Befehle und Transaktionen
erscheinen als ein Eintrag. Abgebrochene Transaktionen erzeugen keinen Eintrag.
Die Pfeil-Icons kennzeichnen ausgeführte bzw. wiederherstellbare Einträge.

- Die Toolbar enthält **Undo** und **Redo**.
- Ein Doppelklick stellt den Zustand nach dem gewählten Eintrag her.
- **Initial state** springt vor den ersten noch verfügbaren Undo-Eintrag.
- Neue Änderungen nach Undo ersetzen den bisherigen Redo-Zweig.

Der Verlauf wird aus dem Befehlsspeicher gelesen; er wird nicht separat in der Map
oder zwischen Programmstarts gespeichert. Die Oberfläche bündelt Aktualisierungen
und verwendet bestehende Listeneinträge weiter. `#HistoryList` und `#HistoryEditor`
können in base.qss gezielt gestaltet werden.

## Dialoggrößen

Unter **Preferences → View → Dialog Sizes** lassen sich die anfänglichen Breiten
und Höhen von **Preferences** und **View Settings** einstellen. Standardwerte:
1100 × 800 bzw. 820 × 760 logische Pixel. Die Größe wird auf die verfügbare
Bildschirmfläche begrenzt. Popups berücksichtigen auch Bildschirme mit negativen
Desktop-Koordinaten.

## Splitter

Die Trennflächen sind dunkler und haben sichtbare Griffpunkte. In **base.qss**
steuert `QSplitter` mit `qproperty-handleWidth` die Breite und `QSplitter::handle`
den Hintergrund einschließlich Hover-Zustand. Die Klasse
`TrenchBroom--View--SplitterHandle` stellt zusätzlich `qproperty-gripColor`,
`qproperty-gripSpacing` und `qproperty-gripDotSize` bereit.

Das Ein-/Ausklappen eines zweiteiligen Splitters stellt die vorherigen Größen
wieder her. Auch Inspector und Toolbar-/Action-Einstellungen verwenden die
gezeichneten Griffpunkte.
