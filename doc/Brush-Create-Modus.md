# Select und Create

**Select** ist der Grundmodus: Objekte auswählen und verschieben sowie in der Map navigieren. Ziehen im leeren Bereich erzeugt hier keinen Brush. Der bisherige Action-Pfad `Controls/Map view/Deactivate current tool` bleibt für vorhandene Keymaps erhalten; die sichtbare Bezeichnung ist jetzt **Select**.

**Create** wird über die Haupt-Toolbar, Edit → Tools → Create oder standardmäßig **N** aktiviert. Das Tastenkürzel lässt sich wie die anderen Werkzeug-Actions in der Keymap ändern. Beim Aktivieren ist der Cuboid ausgewählt. Die Parameter bleiben beim Wechsel zwischen den Formen erhalten.

Im Create-Modus zeigt jeder Map-Viewport seine Formpalette links mittig. Die acht Icons wählen Cuboid, Stairs, Arch, Cylinder, Cone, UV-Sphäroid, Ico-Sphäroid oder Rock. Der aktive Eintrag wird hervorgehoben; die Namen stehen in den Tooltips. Die Formparameter erscheinen ausschließlich in der Toolbar über dem Viewport, ohne zusätzliche Popup-Auswahl. Der Mauszeiger über dem Viewport ist ein Kreuz.

Nach dem Erstellen bleibt Create aktiv, sodass weitere Brushes gezeichnet werden können. **Escape** bricht auch eine laufende Vorschau ab und schaltet unmittelbar zu Select. Der Select-Button schaltet ebenfalls zurück. Gezeichnete Brushes bleiben ausgewählt und können dort verschoben werden. Die übrigen Spezialwerkzeuge behalten ihr bisheriges Verhalten.

## Gestaltung in base.qss

Die Regeln stehen am Ende von `app/resources/stylesheets/base.qss` unter `CREATE: VIEWPORT SHAPE PALETTE`. Anpassbar sind insbesondere:

| Selektor/Eigenschaft | Bedeutung |
| --- | --- |
| `QFrame#BrushShapePalette` | Hintergrundfarbe/-alpha, Rahmen und Rundung |
| `qproperty-overlayOpacity` | Deckkraft der gesamten Leiste einschließlich Icons, 0–1 |
| `qproperty-edgeMargin` | Abstand zum linken Rand und Platzreserve oben/unten |
| `qproperty-panelPadding` | Innenabstand |
| `qproperty-buttonSpacing` | Abstand zwischen den Formbuttons |
| `QFrame#BrushShapePalette QToolButton` | Buttongröße, Hintergrund, Rahmen, Rundung |
| `qproperty-iconSize` | Icongröße, z. B. `32px 32px` |
| `QToolButton:hover`, `:checked`, `:pressed` | Hover-, Auswahl- und Klickdarstellung |

Beispiel:

```css
QFrame#BrushShapePalette {
    qproperty-edgeMargin: 16;
    qproperty-panelPadding: 8;
    qproperty-buttonSpacing: 5;
    qproperty-overlayOpacity: 0.9;
    background-color: rgba(26, 29, 34, 210);
    border-radius: 12px;
}
QFrame#BrushShapePalette QToolButton {
    qproperty-iconSize: 32px 32px;
    padding: 6px;
}
```

Bei kleinen Viewports passt sich die Leiste an die verfügbare Höhe an und kann vertikal gescrollt werden. Änderungen von Icongrößen und Fenstermaßen werden ohne manuellen Refresh berücksichtigt. Die SVG-Icons verwenden die bestehende VectorIcon-Verwaltung.

Das Informations-Overlay erkennt den neuen Zustand als `tool.create` bzw. `tool.id = "create"`; Select verwendet weiterhin `"select"`.

## Prüfung

Release-Build erfolgreich. 113 Anwendungs-/Regressionstests mit 11.264 Prüfungen bestanden; die drei gezielten Select-/Create-Tests wurden nach dem Abschluss nochmals geprüft (110 Prüfungen). Geprüft wurden Aktivierung, Wechsel zwischen Werkzeugen, wiederholtes Zeichnen in 2D/3D, Abbruch während einer Vorschau, Formauswahl, QSS und Verkleinern des Viewports.

Die native macOS-Prüfung umfasst die Toolbar-Actions, echte Ziehbewegungen in Select/Create, Cursorwechsel, alle acht Formen, die Formpalette, Escape sowie die bisherigen Inspector-/Prefab-Funktionen.
