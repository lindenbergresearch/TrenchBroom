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

- Die Toolbar enthält **Undo**, **Redo** und **Return to this state**.
- Ein Klick auf einen Eintrag stellt den Zustand nach diesem Eintrag her.
  Alternativ lässt sich ein Eintrag mit der Tastatur auswählen und der Sprung
  über das Toolbar-Icon, Enter oder **Return to this state** im Kontextmenü auslösen.
- **Initial state** springt vor den ersten noch verfügbaren Undo-Eintrag.
- Neue Änderungen nach Undo ersetzen den bisherigen Redo-Zweig.
- **Clear history** im Kontextmenü leert Undo und Redo. Der Inhalt der Map und
  ihr gespeicherter/ungespeicherter Zustand bleiben erhalten. Während einer
  laufenden Transaktion sind Sprünge und das Leeren deaktiviert.

Reine Auswahl- und andere Änderungen des Editorzustands werden in eingeklappten
**Editor state**-Gruppen gebündelt. Ausklappen zeigt die einzelnen Schritte;
ein Klick auf die Gruppe springt an deren Ende. Die tatsächlichen Undo-Schritte
bleiben unverändert erhalten. Ausgeführte und für Redo verfügbare Schritte bilden
getrennte Gruppen, damit der aktuelle Zustand eindeutig bleibt.

Der Verlauf wird aus dem Befehlsspeicher gelesen; er wird nicht separat in der Map
oder zwischen Programmstarts gespeichert. Die Oberfläche bündelt Aktualisierungen
und verwendet bestehende Listeneinträge weiter. `#HistoryList` und `#HistoryEditor`
können in base.qss gezielt gestaltet werden. Die History verwendet den gemeinsamen
Tabellen-Delegate. Unter `ItemCellStyle[cellContext="history"]` gibt es die Rollen
`action`, `time` und `stateGroup` sowie die Zustände `current` und `redo`.

## Ladehinweise

**Preferences → View → Map Loading → Load summary** schaltet die automatische
Zusammenfassung von Warnungen und Fehlern nach erfolgreichem Laden einer Map ein
oder aus. Die Vorgabe ist eingeschaltet. Auch beim erneuten Laden über **Revert**
wird sie berücksichtigt. Ohne Warnungen oder Fehler erscheint kein Dialog;
fehlgeschlagene Ladevorgänge behalten ihren bisherigen Fehlerdialog.

Die Zusammenfassung ist rein informativ und blockiert den Editor nicht. Sie zeigt
Dateipfad, Gesamtzahlen und die vollständigen Meldungen. Identische Meldungen
gleicher Schwere werden mit einer Anzahl zusammengefasst. Unter der Liste steht
die ausgewählte Meldung vollständig, mehrzeilig und kopierbar. Die Erfassung ist vom
eingestellten Console-Loglevel unabhängig und erfasst synchron während des Ladens
gemeldete Diagnosen; spätere Meldungen aus der Bearbeitung gehören nicht dazu.
Direkt im Dialog kann die automatische Anzeige ebenfalls abgeschaltet werden.

In **base.qss** sind `#MapLoadDiagnosticsDialog` (einschließlich
`qproperty-initialWidth`/`qproperty-initialHeight`), `#MapLoadDiagnosticsSummary`,
`#MapLoadDiagnosticsList` und `#MapLoadDiagnosticsMessage` anpassbar. Die Tabellenrollen unter
`ItemCellStyle[cellContext="loadDiagnostics"]` heißen `warning`, `error`,
`message` und `count`.

## Dialoggrößen

Die Anfangsgrößen von **Preferences** und **View Settings** stehen ausschließlich
in **base.qss**, im Abschnitt `INITIAL DIALOG SIZES`:

```css
QDialog#PreferenceDialog {
    qproperty-initialWidth: 1100;
    qproperty-initialHeight: 800;
}
QWidget#ViewDetailsPopup {
    qproperty-initialWidth: 820;
    qproperty-initialHeight: 760;
}
```

Die Werte sind logische Pixel. Sie legen keine feste oder minimale Fenstergröße
fest; die Fenster bleiben größenveränderbar und werden auf die verfügbare
Bildschirmfläche begrenzt. In Preferences gibt es dafür keine Eingabefelder mehr;
alte gespeicherte Größen-Preferences werden nicht mehr ausgewertet. Popups
berücksichtigen auch Bildschirme mit negativen Desktop-Koordinaten.

## Splitter

Die Trennflächen sind dunkler und haben sichtbare Griffpunkte. In **base.qss**
steuert `QSplitter` mit `qproperty-handleWidth` die Breite und `QSplitter::handle`
den Hintergrund einschließlich Hover-Zustand. Die Klasse
`TrenchBroom--View--SplitterHandle` stellt zusätzlich `qproperty-gripColor`,
`qproperty-gripSpacing` und `qproperty-gripDotSize` bereit.

Das Ein-/Ausklappen eines zweiteiligen Splitters stellt die vorherigen Größen
wieder her. Auch Inspector und Toolbar-/Action-Einstellungen verwenden die
gezeichneten Griffpunkte.
