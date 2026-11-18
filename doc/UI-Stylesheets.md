# UI-Gestaltung und Quellstruktur

Die vormals flachen UI-Quellen liegen jetzt in fachlichen Unterordnern unter
`common/src/View`. Die Zuordnung ist in `common/src/View/README.md` beschrieben;
die Tests folgen derselben Struktur. Klassen und Namespaces behalten ihre Namen,
damit bestehende QSS-Klassenselektoren gültig bleiben.

## Einstieg für die Gestaltung

`app/resources/stylesheets/base.qss` enthält die zentralen Regeln. ThemeManager
liefert Werte aus Preferences und `theme.json`, QSSBuilder prüft und ersetzt die
Referenzen `${color.*}`, `${length.*}`, `${family.*}` usw. Die bereits vorhandene
Dateiüberwachung und Fehlerbehandlung gelten weiterhin.

Ein anderes QSS-Design kann die gleichen semantischen Rollen verwenden. Die
Darstellung hängt dadurch vom Stylesheet ab, während C++ die Funktion und den
Zustand liefert. Der aktuelle Lader verwendet weiterhin `stylesheets/base.qss`;
eine Auswahloberfläche für mehrere Theme-Dateien ist ein eigener späterer Schritt.

## Neu bzw. erweitert über QSS steuerbar

| Bereich | Selektor / Eigenschaften |
| --- | --- |
| Property-Tabelle | `QWidget#ItemCellStyle[cellContext="entityProperties"]`; Rollen `key`/`value`, Zustände `readonly`, `default`, `subset`, `mixed` |
| Advanced-/Colors-Tabellen | `#ItemCellStyle[cellContext="preferences"]` bzw. `[cellContext="colors"]`, Rolle `value` |
| Ausgeblendete Issues | `#ItemCellStyle[cellContext="issues"][cellRole="hidden"]` |
| Konsole und Prozessausgabe | `TrenchBroom--View--SemanticTextEdit`: Schrift, Standardfarben und Meldungsfarben |
| Compiler-Systemmeldungen | `#CompilationOutput`: `infoColor`, `warningColor`, `errorColor`, `successColor`, `systemColor`, `systemFontWeight` als `qproperty-*` |
| Trennlinien | `QFrame#borderLine`, `orientation`, `separatorWeight`; `qproperty-thickness`, Hintergrundfarbe |
| Panel-Rahmen | `TrenchBroom--View--BorderPanel`: `qproperty-borderColor`, `qproperty-borderWidth`, Hintergrundfarbe |
| Tab-Markierung | `#TabSelectionIndicator`, Zustand `[selected="true"]`: Farbe und Höhe |
| Farb-Historie im Smart-Editor | `TrenchBroom--View--ColorTable`: Zellgröße, Abstand, Auswahlfarbe und Auswahlbreite |
| Preferences-Navigation | `#ToolBar_PreferenceDialog`, `#ToolButton_PreferenceDialog`: Icongröße und Mindestbreite |
| Action-/Toolbar-Listen | `#EditorConfigurationList`, `#EditorToolbarAssignedActions`: Icongröße |
| SVG-Auswahldialog | `#EditorIconGrid`: Icon-/Kachelgröße; `#EditorIconPreview`: Größe, Rahmen, Padding |
| Welcome-Aktionen | `#WelcomeWindow_Button_Left`, `_Middle`, `_Right`: Icongröße |
| Kleine Werkzeugbuttons | `QToolButton#toolButton_borderless`, `[popupButton="true"]`: Icongröße |
| Gemeinsame Flächen | `[uiSurface="base"]` und `[uiSurface="window"]`: Hintergrund über gemeinsame Regeln |

Die bestehenden Rollen `uiTextRole`, `uiEmphasized`, `uiItalic`, die Layer-Zeilen,
Inspector-Titel und -Toolbars, Splitter, Prefab-Karten und Overlay-Themes sind
weiterhin gültig. `makeEmphasized`, `makeSmall` usw. setzen bereits semantische
Eigenschaften; ihre Verwendung bedeutet keine fest im C++ gesetzte Schrift.

## Tabellenzellen

Tabellenmodelle liefern jetzt Bedeutung über `StyledItemDelegate::TextStyleRole`
und optional `TextStatesRole`. Ein gemeinsamer Delegate liest Schrift und optionale
Textfarben aus einem unsichtbaren `ItemCellStyle`-Widget. Bearbeitungsfelder erhalten
als `StyledCellEditor` dieselben Kontext-, Rollen- und Zustandsattribute. Pro Rolle/Zustand gibt es
einen wiederverwendeten Stil, nicht ein zusätzliches Widget pro Tabellenzeile.

Beispiel für eine andere Darstellung der Property-Werte:

```css
QWidget#ItemCellStyle[cellContext="entityProperties"][cellRole="value"],
QWidget#StyledCellEditor[cellContext="entityProperties"][cellRole="value"] {
    font-family: "Inter";
    font-size: 15px;
    color: #d6e5ef;
}

QWidget#ItemCellStyle[cellContext="entityProperties"][cellStates~="readonly"],
QWidget#StyledCellEditor[cellContext="entityProperties"][cellStates~="readonly"] {
    font-style: italic;
    color: #a3b0bd;
}
```

`readonly` bezeichnet hier einen nicht umbenennbaren Property-Schlüssel; die
Editierbarkeit des jeweiligen Wertes folgt weiterhin den Modell-Flags.

Bei Entity-Properties steuert QSS auch die Textfarben für Standardwerte,
Teilselektionen und unterschiedliche Werte. In Colors-Tabellen stellen die Farben
hingegen den tatsächlichen Einstellungswert dar. Dort bleibt
`qproperty-overrideTextColor: false` die Vorgabe. Wer diesen Datenfarbwert bewusst
übersteuern möchte, kann die Eigenschaft auf `true` setzen und `color` festlegen.

## Konsole und Compiler

Interne Meldungen werden mit einer semantischen Rolle markiert. Die eigentliche
Farbe kommt aus den `qproperty-*Color`-Werten des Text-Widgets. Das gilt auch für
bereits angezeigte Meldungen: Ein Stilwechsel formatiert sie in kleinen Portionen
neu, damit eine längere Konsole die UI nicht für einen kompletten Durchlauf anhält.
Die Schrift erbt die QSS-Einstellungen des Widgets.

Die Log-Farben stehen als `${color.log.trace}`, `.debug`, `.info`, `.warning` und
`.error` bereit. Ihre Vorgaben bleiben an die vorhandenen Color-Preferences
gebunden. Compiler-Systemmeldungen haben eine gezielte QSS-Regel:

```css
TrenchBroom--View--SemanticTextEdit#CompilationOutput {
    font-family: "Inter";
    font-size: 14px;
    background-color: #171b20;
    qproperty-errorColor: #ef6670;
    qproperty-warningColor: #dfbb65;
    qproperty-successColor: #77c899;
    qproperty-systemFontWeight: 700; /* Qt6/CSS: 400 normal, 700 fett */
}
```

`systemFontWeight` verwendet die Qt6-/CSS-Skala von 100 bis 900. Persönliche
Stylesheets mit früheren Qt5-Werten (z. B. 50 oder 75) werden weiterhin korrekt
in normale bzw. fette Schrift umgerechnet.

ANSI-Formatierungen aus externen Programmen und explizit als Daten übergebene
Farben behalten ihre Bedeutung. Sie werden beim Wechsel der internen Meldungsfarben
nicht ersetzt. Die Undo-Historie der Map ist von der Ausgabeformatierung unabhängig.

## Schriftvergleich und Listentext unter Qt6

Unter **Preferences → View → List text comparison** stehen Systemschrift und
Inter Variable nebeneinander. Beide verwenden dieselbe UI-Größe, Mindestgröße,
Detail-Deckkraft und Textpositionierung. Die Anzeige unter jedem Muster nennt
das verwendete Gewicht und die tatsächlich unterstützte optische Größe.

Die Schaltflächen wählen die Schriftfamilie für die gesamte Oberfläche. Die
zusätzliche Feinabstimmung gilt für die beiden Muster und den **Layer-Baum**:

| Einstellung | Bedeutung / Standard |
| --- | --- |
| List text weight | `Font default` (gespeichert als 0) übernimmt den Schriftschnitt. Zwischengewichte wie 450 verwenden bei variablen Fonts die echte `wght`-Achse; sonst wird der nächste verfügbare Schnitt verwendet. Abweichende QSS-Gewichte für hervorgehobene Zeilen bleiben erhalten. |
| Optical size | `Automatic` (0) folgt der Punktgröße jeder Textzeile. Manuelle Werte werden auf den Bereich der `opsz`-Achse begrenzt. Ohne diese Achse bleibt die Schrift unverändert. |
| Line spacing | Typografische Zeilenmetriken optional verwenden; standardmäßig aus. Bei Fonts, die diese bereits vorgeben, entsteht kein Unterschied. |
| Text positioning | Standard: `Pixel aligned`. Die Grundlinie wird nach Einbezug von Skalierung und Verschiebung am physischen Pixelraster ausgerichtet. `Subpixel positioning` erlaubt gebrochene Grundlinien mit ungehintetem Text; je nach Darstellung entstehen zusätzliche Glyphenvarianten im Qt-Cache. |

Die Werte sind als `Editor/UI List …` in den Preferences gespeichert. Änderungen
berechnen Schriftmetriken und Zeilenlayout neu; ein Neuaufbau des Layer-Baums oder
ein globales QSS-Reload für jeden Gewichtsschritt ist nicht erforderlich.

Inter Variable **4.1** liegt unverändert mit SIL-OFL-Lizenz im Font-Verzeichnis.
Die Familie `Inter Variable` ist von der vorhandenen älteren Familie `Inter`
getrennt. Unterstützt werden Gewicht 100–900 und optische Größe 14–32.
Quelle, Prüfsummen und Lizenz stehen in `InterVariable-README.md` und
`InterVariable-LICENSE.txt`. Bestehende Font-Dateien und gespeicherte Auswahlen
bleiben verfügbar.

Die Variationsachsen werden an Schriftkopien für die selbst gezeichneten Zeilen
gesetzt. Sie werden nicht auf die geerbte QApplication-Schrift übertragen, weil
eine geerbte `wght`-Achse sonst beispielsweise ein späteres QSS-`font-weight: bold`
überstimmen kann. Dadurch bleiben die bisherigen Gewichtungen anderer Widgets
und die QSS-Rollen erhalten. Die OpenGL-Schriftatlanten des Map-Renderers folgen
weiterhin ihren eigenen Renderer-Fonteinstellungen.

Das Muster übernimmt Rahmen, Hintergrund und Beschriftungsstil aus `base.qss`:

- `QFrame[fontPreview="true"]`: Musterfläche und Rahmen.
- `QLabel[fontPreviewHeading="true"]`: Überschrift über dem Muster.
- `QLabel[fontPreviewInfo="true"]`: verwendetes Gewicht / optische Größe.
- `QLabel#UIFontComparisonScope`: Erläuterung zum Geltungsbereich.

Der Layer-Baum verwendet weiterhin `QWidget#LayerTreeRow` mit seinen Typ- und
Zustandsselektoren. Farben, Abstände, Detailfarbe und Mindesthöhe bleiben dort
konfigurierbar. Variable Schriftmetriken werden auch beim Ermitteln der
Zeilenhöhe und Textbreite berücksichtigt.

## Grenzen und bewusst im Code verbleibende Aufgaben

- Layout-Verhalten wie Splitter-Aufteilung, Docking, Scrollen und die Anordnung
  von Controls bleibt in Qt-Layouts. `margin`/`padding` in QSS betrifft die
  Widget-Box und ersetzt nicht alle Layout-Abstände.
- Icongrößen, die der Benutzer ausdrücklich pro Toolbar, Layer-Typ oder Prefab-Zoom
  einstellt, folgen weiterhin diesen Einstellungen. Statische Standardgrößen der
  übrigen Controls stehen im Stylesheet.
- Die Farbfelder eines ColorButton bzw. einer ColorTable zeigen echte Datenfarben.
  Ihre Inhalte werden gezeichnet; Rahmen, Maße und Auswahlmarkierung sind UI-Stil.
  ColorButton erzeugt dafür kein eigenes Inline-Stylesheet mehr.
- Schriftdateien laden, Hinting/Antialiasing und die Mindestgröße absichern ist
  Aufgabe von FontUtils und ThemeManager. Die System-/Inter-Vorschau zeigt die
  bewusst gewählten Vergleichsschriften und deren Detailgrößen.
- Layer- und SVG-Renderer setzen beim Zeichnen die aus QSS bzw. Preferences
  ermittelten Werte. Ein `painter.setFont` oder `setBrush` allein ist daher kein
  verbliebener hartcodierter UI-Stil.
- Die 3D-Szene und die Material-/Entity-Farben der Prefab-Vorschau folgen den
  Renderer-/Spiel-/Map-Daten. Das Kontext-Overlay besitzt bereits externe
  QSS-/Template-Dateien.
- Die Proxy-Zeichnung für Menüs/Tooltips stellt einen Fallback ohne aktives QSS
  bereit. Debug-Werkzeuge zum Prüfen einer Palette bleiben Entwicklertools.

## Neue UI-Komponenten ergänzen

1. Die Klasse im passenden Fachordner anlegen und explizit in CMake registrieren.
2. Stabile `objectName`s und semantische Zustände/Rollen vergeben.
3. Standard-Qt-Attribute in `base.qss` definieren. Für selbst gezeichnete Widgets
   gezielte `Q_PROPERTY`s oder wiederverwendete Stil-Proxys anbieten.
4. Im Code nur Daten und Verhalten setzen; die Konfiguration der Darstellung beim
   Zeichnen aus Palette, Widget-Font und den Stil-Properties lesen.
5. Bei QSS-abhängigen Größen auch Size-Hints, Zeichenfläche und Cache-Schlüssel
   berücksichtigen. Die SVG-Vorschau reagiert deshalb auf Größen-/Stiländerungen.
