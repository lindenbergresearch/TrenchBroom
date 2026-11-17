# Theme-Verwaltung

Die aktive Oberfläche verwendet `app/resources/stylesheets/base.qss`. Farben und
gemeinsam verwendete Größen stammen aus `ThemeManager`. Der Manager liefert dieselben
Werte an das Stylesheet, die Layer-Icons und die selbst gezeichneten Menü-/Tooltip-Rahmen.

## Einstellungen und Theme-Dateien

Die bisherigen Preferences bleiben erhalten. Fenster-, Text- und Auswahlfarben,
Helligkeit, Fehler-/Warnfarben, Schriftdateien, Schriftgrößen, Mindestgröße,
Deckkraft des Detailtexts und Layer-Iconfarben werden weiterhin dort eingestellt.
Die daraus abgeleiteten Theme-Werte werden zentral berechnet.

Zusätzliche Farben und gemeinsame Maße stehen in
`app/resources/stylesheets/theme.json`. Im ausgelieferten Programm befindet sich
die Datei neben `base.qss` im Ressourcenverzeichnis.

Eine persönliche `theme.json` direkt im Verzeichnis der Preferences kann einzelne
Werte ergänzen oder überschreiben. Sie muss nur die Änderungen enthalten:

```json
{
    "version": 1,
    "colors": {
        "source.background": "#20242b",
        "layer.hidden.text": "#95a8bf",
        "syntax.comment": "${color.text.detail}"
    },
    "lengths": {
        "radius.menu": 12,
        "radius.tooltip": 8
    },
    "numbers": {
        "icon.brightness": 0.75,
        "icon.disabledBrightness": 0.25,
        "icon.disabledOpacity": 1.0
    }
}
```

Werte, die an Preferences gebunden sind, werden nicht durch einen gleichnamigen
Eintrag in der Theme-Datei ersetzt. Beispielsweise wird `color.text` über die
Textfarbe in den Einstellungen gesteuert, `color.icon.brush` über die Brush-Iconfarbe.
Ein widersprüchlicher Dateieintrag meldet einen Fehler. So bleiben die Einstellungen
wirksam. Eigene zusätzliche Namen können ohne Änderungen an C++ ergänzt werden.

Änderungen an der persönlichen Datei, der mitgelieferten Theme-Datei und `base.qss`
werden automatisch übernommen. Auch atomisches Speichern durch einen Texteditor
wird berücksichtigt. Das Entfernen der persönlichen Datei stellt die betreffenden
Standardwerte wieder her.

## Platzhalter im Stylesheet

Neue Platzhalter haben eine eindeutige Schreibweise:

```css
QWidget#MyPanel {
    color: ${color.text};
    background-color: ${color.window};
    border-radius: ${length.radius.border};
    font-family: ${family.ui};
    font-size: ${length.font.ui};
}
```

Schriftfamilien werden automatisch in Anführungszeichen gesetzt und maskiert.
Die bisherigen Schreibweisen wie `#TEXT_COLOR`, `#WINDOW_COLOR` und
`"#UI_FONT_FAMILY"` bleiben verwendbar. Hex-Farben, Objekt-IDs, URL-Fragmente und
Kommentare werden dabei nicht als gleichnamige Platzhalter umgeschrieben.

Die Theme-Datei unterstützt folgende Abschnitte:

| Abschnitt | Platzhalter-Präfix | Inhalt |
| --- | --- | --- |
| `colors` | `color.` | Qt-Farbnamen, `#RRGGBB` oder `#AARRGGBB` |
| `lengths` | `length.` | Nichtnegative logische Pixelwerte ohne `px` |
| `numbers` | `number.` | Endliche Zahlen, etwa Deckkraft-Faktoren |
| `fontFamilies` | `family.` | Schriftfamilien |
| `fontWeights` | `weight.` | Ganzzahlige CSS-Schriftgewichte von 0 bis 1000 |
| `keywords` | `keyword.` | Einzelne Schlüsselwörter |

Ein vollständiger `${…}`-Wert kann auf einen anderen Wert desselben Typs verweisen.
Zyklische Verweise, unbekannte Namen und falsche Typen werden abgewiesen. Längen
liegen zwischen 0 und 4096 logischen Pixeln. Die Einstellungen für Icon-Helligkeit
und deaktivierte Icons liegen zwischen 0 und 1. Der Manager behandelt die Besonderheiten der
Schriftgewichte unter Qt 5 und Qt 6 für die vorhandenen Font-Preferences.

## Fehler und Aktualisierung

Der Compiler verarbeitet ein Stylesheet in einem Durchlauf. Fehlende und ungültige
Platzhalter werden mit Datei, Zeile und Spalte gemeldet. Theme-Fehler nennen die
Datei und den betroffenen Schlüssel; JSON-Syntaxfehler zusätzlich die Position.
Die Meldungen erscheinen im Log bzw. in der Editor-Konsole.

Bei einer fehlerhaften Live-Änderung bleibt die komplette letzte gültige Darstellung
aktiv. Eine bereits beim Programmstart fehlerhafte persönliche Theme-Datei wird
gemeldet und der mitgelieferte Standard verwendet. Nach der Korrektur wird die Datei
wieder automatisch übernommen.

Ist schon das mitgelieferte Stylesheet bzw. die grundlegende Theme-Datei fehlerhaft,
startet der Editor mit dem Qt-Basisstil und den vorhandenen Farb-/Schrifteinstellungen.
Er bleibt bedienbar und übernimmt die Korrektur der Datei ebenfalls automatisch.

Der Compiler merkt sich die tatsächlich verwendeten Werte. Ändern sich weder diese
Werte noch die Vorlage, kompiliert er nicht erneut. Ein identisches fertiges
Stylesheet wird nicht erneut an Qt übergeben. Der zugrunde liegende Qt-Stil bleibt
bestehen, und Schriftdateien werden nur bei Änderungen ihrer Auswahl oder Größe
erneut angefordert.

Bei einer gültigen Änderung ersetzt die Anwendung die Qt-Stylesheet-Hülle in einem
Durchlauf, statt alle verschachtelten Widget-Bäume wiederholt zu aktualisieren.
Der Basisstil für gerundete Menüs und Tooltips bleibt erhalten. Sichtbare Fenster
werden während dieses synchronen Austauschs kurz vom Zeichnen ausgenommen und
anschließend gemeinsam neu gezeichnet. Eine fehlerhafte QSS-Vorlage wird vor dem
Austausch an einem separaten Widget geprüft; die bisherige Darstellung bleibt aktiv.

Änderungen an Schrift und Farben werden über denselben kurzen Timer gebündelt.
Preferences-Kategorien werden beim ersten Aufruf erstellt und für weitere Besuche
behalten. Dadurch fallen noch unbenutzte Kategorien bei UI-Aktualisierungen nicht
als zusätzliche Widgets an. Schrift-/Farbänderungen berechnen außerdem keine
Selektionsstatistiken und erneuern keine Tastenkombinationen des MapEditors.

`base.qss` enthält am Anfang einen Schnellfinder mit der Zuordnung von sichtbaren
Programmteilen zu Selektoren. Die Abschnitte erklären ihre globalen Regeln,
werkzeugspezifischen Ausnahmen und die verfügbaren Zustands-/Widget-Eigenschaften.

Layer-Icons lösen ihre Theme-Farbe beim Zeichnen auf. Eine Änderung der Iconfarbe
ersetzt deshalb weder die Baum-Einträge noch deren Icons und verändert keine Auswahl,
aufgeklappten Knoten oder Scrollposition. Die vorhandenen begrenzten SVG- und
Raster-Caches werden weiterverwendet.

## Helligkeit von Oberfläche und Icons

Der Helligkeitsregler unter View übernimmt seinen Wert beim Loslassen. Während des
Ziehens aktualisiert sich die Zahlenanzeige; die aufwendige Neudarstellung der
Oberfläche erfolgt einmal. Schnell aufeinanderfolgende Tastatur- und Mausradänderungen
werden ebenfalls gebündelt.

`numbers.icon.brightness` bestimmt die gemeinsame Helligkeit der SVG-Icons. Beim
Laden wird die höchste RGB-Komponente der gezeichneten Bereiche einmal gemessen,
für Ein-/Aus-Varianten jeweils getrennt. Transparente Ränder werden dabei ignoriert.
Die RGB-Werte werden proportional auf den Zielwert skaliert: `0.75` entspricht
einer maximalen Komponente von etwa 191 bei 255 möglichen Stufen, unabhängig davon,
ob das Ausgangs-Icon dunkelgrau oder weiß war. Farbtöne, relative Abstufungen und
Deckkraft bleiben erhalten. Rein schwarze Icons erhalten den entsprechenden Grauwert.
`0` erzeugt schwarze Icons, `1` die volle normalisierte Helligkeit.

Die Rasterisierung erfolgt weiterhin direkt in der benötigten Größe und
Bildschirmauflösung. Zusätzliche Farbpräzision verhindert dunkle Säume beim Aufhellen.
Bereits geladene Icons übernehmen einen geänderten Faktor über den Raster-Cache;
die SVG-Dateien müssen dafür nicht erneut eingelesen werden. Deaktivierte Icons
werden anschließend zusätzlich mit `icon.disabledBrightness` und
`icon.disabledOpacity` abgedunkelt. Fotos, Vorschaubilder und Programm-/Spiel-Logos
aus Rasterdateien werden nicht verändert.

Die Normalisierung ist im Stylesheet über `qproperty-normalizeBrightness` am
`QWidget#IconAppearance` abschaltbar. Der Zielfaktor wird dort über
`qproperty-brightness: ${number.icon.brightness}` eingebunden.
