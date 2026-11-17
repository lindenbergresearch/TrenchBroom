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
- **Animation speed:** Geschwindigkeit der Pfeile von Quelle zu Target in Pixeln
  pro Sekunde. `0` hält die Animation an; die Richtung bleibt erkennbar.
- **Arrow spacing:** Abstand zwischen den Richtungspfeilen in Pixeln.
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
