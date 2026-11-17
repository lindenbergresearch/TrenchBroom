# Editor Actions und Editor Toolbar

Die Preferences enthalten zwei Bereiche, die dieselbe Konfiguration verwenden.
Änderungen gelten für alle geöffneten MapEditor-Ansichten und werden mit den
Preferences gespeichert. Auf Plattformen mit Apply/Cancel folgt die Speicherung
diesen Schaltflächen; Cancel stellt auch die vorherigen Actions und Toolbars wieder her.

## Editor Actions

Eine Action hat eine stabile ID, einen Namen, einen Editor-Command, einen Shortcut,
ein SVG-Icon und einen Tooltip. Die Anforderungen des Commands an Auswahl und
Werkzeug bleiben wirksam. Derselbe Command kann in mehreren Actions mit
unterschiedlichen Icons, Namen oder Shortcuts verwendet werden.

Die vier Targets steuern die Anzeige im Main Menu, Popup Menu des MapEditors,
Main Toolbar und in den Editor Toolbars. Eine Action ohne Anzeige-Target kann
weiterhin über ihren Shortcut erreichbar sein. **Enabled** deaktiviert auch ihre
Ausführung. Bestehende Standard-Actions können geändert und deaktiviert werden;
zusätzlich angelegte Actions können dupliziert und entfernt werden.

**Main menu path** verwendet `/` zwischen Untermenüs, beispielsweise
`Editor Actions/Selection`. Fehlende Menüs werden angelegt. **Toolbar order**
bestimmt die Reihenfolge in der Main Toolbar; verschiedene **Toolbar groups**
werden durch Trenner getrennt. Tooltip und Shortcut werden in den Toolbar-Buttons
aus der Action übernommen. **Choose icon…** öffnet eine durchsuchbare Kachelansicht
der mitgelieferten SVG-Icons mit Vorschau. Alternativ kann ein Ressourcenname oder
ein eigener SVG-Pfad direkt eingetragen werden.

Die vorhandene Keyboard-Verwaltung verwendet dieselben Actions und Shortcuts.
Frühere Shortcut-Einstellungen werden berücksichtigt, solange kein neuer Override
für die jeweilige Action vorliegt. Neue, bereits anderweitig belegte Shortcuts
werden im Action Editor mit einer Meldung abgewiesen, wenn ihre Kontexte überlappen.

Die Auswahllisten für Command und Action sind durchsuchbar: Ein Klick markiert den
bisherigen Text. Beim Tippen erscheinen nur Treffer, die den Suchtext irgendwo im
Namen oder in der ID enthalten, unabhängig von Groß-/Kleinschreibung. Trefferstellen
werden hervorgehoben; die zweite Zeile zeigt die eindeutige Command-/Action-ID.
Mausklick oder Enter übernehmen den Treffer. Escape verwirft die Suche; reiner
Suchtext ändert keine gespeicherte Zuordnung.

## Editor Toolbar

Toolbars können angelegt, dupliziert, entfernt und umgeordnet werden. Jede Toolbar
enthält eine geordnete Liste von Actions und optionalen Trennern. Rechts sind
**Toolbar settings** und **Elements** getrennt: Die erste Seite enthält die
Toolbar-Einstellungen, die zweite die Action-Auswahl und die geordnete Elementliste.
Die Auswahl eines Elements zeigt dessen Action. Eine bestätigte Action-Auswahl
ersetzt die Zuordnung dieses Elements einschließlich Beschriftung und Icon.
**+** fügt ein weiteres Element hinzu; Trenner haben einen eigenen Button.
Beim Zuweisen einer Action wird deren Target **Editor Toolbars** aktiviert. Die Reihenfolge lässt
sich mit Up/Down ändern. Die Standard-Toolbar enthält die Create-Formen und bleibt
auch im Select-Modus sichtbar. Ein Klick auf eine Form aktiviert Create.

Pro Toolbar lassen sich Position (Left, Top, Right, Bottom), Ausrichtung,
bevorzugte Icongröße und Mindestgröße einstellen. Start/End heißen je nach Seite
Top/Bottom oder Left/Right. Die Anordnung gilt pro Seite:

- **Along the edge:** Toolbars teilen sich eine Reihe bzw. Spalte und werden
  entsprechend ihrer Ausrichtung längs der Kante angeordnet.
- **Parallel rows / columns:** Jede Toolbar bekommt eine eigene parallele Reihe
  oder Spalte.

Reicht der Platz nicht aus, werden die Toolbars derselben Seite gemeinsam
verkleinert. Die eingestellten bevorzugten Größen bleiben gespeichert. Passen sie
auch an der Mindestgrenze nicht, wird die betreffende Seite ausgeblendet; bei mehr
Platz erscheint sie wieder. Links/rechts und oben/unten werden auch gegeneinander
geprüft, damit sich Toolbars nicht überdecken. Obere und untere Toolbars halten die
seitlichen Toolbar-Bereiche frei.
Das Zustands-Overlay berücksichtigt ebenfalls die belegten Toolbar-Bereiche.

## Design und Konfigurationsdateien

Die Standarddefinitionen liegen unter `app/resources/defaults/EditorActions.json`.
Die registrierten Commands führen die Editor-Operationen aus; Beschriftung, Icon,
Tooltip, Shortcut-Overrides, Menüpfad und Targets sind davon getrennte Daten.
Eigene Actions können über diese Definitionen oder die Preferences ergänzt werden.
Persönliche Änderungen werden als Overrides unter `Editor/Actions and toolbars`
in den Preferences gespeichert. Import/Export überträgt Actions und Toolbars als
versionierte JSON-Konfiguration. Ungültige Konfigurationen werden vollständig
abgewiesen; gültige Einstellungen werden nicht teilweise überschrieben.

Das globale Toolbar-Design steht ausschließlich in `base.qss`. Die vier Selektoren
sind getrennt anpassbar:

```css
QFrame#EditorToolbar[position="left"] { background-color: ${color.base}; }
QFrame#EditorToolbar[position="top"] { background-color: ${color.base}; }
QFrame#EditorToolbar[position="right"] { background-color: ${color.base}; }
QFrame#EditorToolbar[position="bottom"] { background-color: ${color.base}; }
```

An `QFrame#EditorToolbar` stehen außerdem `qproperty-panelPadding`,
`qproperty-buttonPadding`, `qproperty-buttonSpacing`, `qproperty-edgeMargin` und
`qproperty-toolbarGap` zur Verfügung. Diese Werte gehen in die Platzberechnung ein.
Buttons verwenden `QToolButton#EditorToolbarButton` und die üblichen Zustände
`:hover`, `:checked`, `:pressed` und `:disabled`.

Checkboxen und Zahlenfelder verwenden die allgemeinen Regeln für `QCheckBox` und
`QAbstractSpinBox` in `base.qss`, gemeinsam mit den übrigen Preferences.
Die Trefferfarben der Suche sind über `qproperty-matchForeground` und
`qproperty-matchBackground` an `QComboBox[searchable="true"]` konfigurierbar;
das Ergebnisfenster verwendet `QListView#SearchableComboPopup`.

In vorhandenen Map-Fenstern bleiben die QAction-Objekte bei Änderungen erhalten. Offene Menüs werden
erst nach dem Schließen neu aufgebaut. Änderungen allein an Toolbar-Größe oder
Position bauen weder Menüs noch Shortcut-Bindings neu auf.
