# Beschriftungen und Entity-Verbindungen

Maßangaben, Entity- und Gruppennamen verwenden pro Kamera und Frame eine gemeinsame
Platzierung. Dabei zählen auch die farbigen Hintergrundflächen zum Platzbedarf.
Zwischen den Beschriftungen bleibt ein Abstand von vier logischen Pixeln.

Maßangaben der Selektion haben Vorrang. Anschließend werden nahe Objektbezeichnungen
platziert. Die Suche wählt einen freien Platz möglichst nahe am ursprünglichen
Anker; Beschriftungen bleiben innerhalb der Ansicht. Bei großer Selektion können
Maßangaben an den Ansichtsrand rücken. In sehr dichten Ansichten werden nachrangige
Bezeichnungen ausgeblendet, wenn in der Nähe kein freier Platz verfügbar ist.
Feste Bildschirmtexte behalten ihre Position und reservieren dort ihren Platz.

Die Platzierung berücksichtigt unterschiedliche Fonts und Render-Services gemeinsam.
Eine räumliche Unterteilung und begrenzte Suche halten den Aufwand auch bei vielen
Labels begrenzt. Die Schrift wird weiterhin an ganzen Pixelpositionen gezeichnet.

## Entity-Verbindungen

Unter **View Settings → Objects → Links and Limits** befinden sich neben den
bisherigen Sichtbarkeitsmodi folgende Einstellungen:

- **Line width:** Linienstärke in logischen Pixeln, unabhängig von Zoom und DPI.
- **Animation speed:** Geschwindigkeit der Pfeile von Quelle zu Target in Map-Einheiten
  pro Sekunde. `0` hält die Animation an; die Richtung bleibt erkennbar.
- **Arrow spacing:** Abstand zwischen den Richtungspfeilen in Map-Einheiten.
- **Maximum camera distance:** Maximale Entfernung in Map-Einheiten im 3D-Editor.
  Der Abstand wird zum jeweiligen Teil der Verbindung gemessen. Lange Verbindungen
  bleiben deshalb sichtbar, wenn sie nahe an der Kamera vorbeiführen. Am Limit
  werden sie weich ausgeblendet. `0` bedeutet unbegrenzt.
- **Behind objects:** Deckkraft verdeckter Verbindungen. `0%` blendet diese aus.

Die Werte werden als Preferences gespeichert und über Reset wiederhergestellt.
Die Entfernungseinstellung begrenzt die 3D-Ansicht; in orthografischen Ansichten
bleiben die Verbindungen unabhängig von der Kameraentfernung verfügbar.

Verbindungen werden als geglättete Dreiecksflächen gezeichnet. Bewegung und
Entfernungsbegrenzung berechnet der Shader; die Geometrie bleibt zwischen den Frames
im GPU-Puffer. Der bestehende FPS-Limiter gilt auch während der Animation.
Verbindungen ohne Länge oder mit ungültigen Koordinaten erzeugen keine Geometrie.

## Selection Bounds und Führungslinien

Die Einstellungen stehen unter **View Settings → Geometry → Selection Bounds**.

- **Show markers on selection corners:** Schaltet die Quadrate an der Selektion
  unabhängig von den flach auf Treffflächen gezeichneten X-Markierungen aus.
- **Maximum guide length:** Maximale Länge einer Führungslinie in Map-Einheiten.
  Standard: 1024. Weiter entfernte Treffer erzeugen keine Treffmarkierung und
  keine Entfernungsbeschriftung.
- **Maximum camera distance:** Grenze für Führungslinien, Eck- und Treffmarkierungen
  sowie ihre Entfernungsbeschriftungen. Gemessen wird der Abstand der Kamera zur
  nächsten Stelle der Selektionsbox. `0` bedeutet unbegrenzt. Die Maße der
  Selektion und das optionale Bounds-Volumen bleiben davon unabhängig sichtbar.
- **Show distances to hit surfaces:** Zeigt die tatsächliche Entfernung vom
  Linienanfang bis zur getroffenen Brush-Fläche. Die Darstellung verwendet die
  eingestellten Map-/Meter-Einheiten. Farbe, Schriftgröße, Position entlang der
  Strecke und ein vertikaler Versatz sind einstellbar. Die gemeinsame
  Label-Platzierung vermeidet Überschneidungen mit den Selektionsmaßen.
- **Object bounds on hover only:** Das Bounds-Volumen erscheint unter dem Cursor,
  auch bei unselektierten Objekten. Dies setzt **Show object bounds** voraus und
  gilt in 2D und 3D. Brush-Entities und geschlossene Gruppen werden als Ganzes
  behandelt. Die normale Selektion und deren Maßangaben bleiben bestehen.

Die Musterposition von Strichen und Target-Pfeilen wird aus der Entfernung vom
ursprünglichen Anfang der Verbindung im Raum berechnet. Kamerabewegungen,
Zoomen und das Abschneiden am Kameraplan setzen die Phase deshalb nicht zurück.
Musterabstände und Animationsgeschwindigkeiten beziehen sich auf Map-Einheiten;
die eigentliche Linienbreite und die Pfeilgröße bleiben in Bildschirmpixeln.
Die Animation nutzt weiterhin eine monotone Uhr. Geschwindigkeit `0` hält das
Muster an; volle Führungslinien pulsieren bei aktivierter Animation.

Die Schriftgrößenangabe für Entfernungen wird vom Text-Renderer tatsächlich
übernommen. Die Formatierung der Selektionsmaße erzeugt keine temporären,
unfreigegebenen QString-Objekte mehr.
