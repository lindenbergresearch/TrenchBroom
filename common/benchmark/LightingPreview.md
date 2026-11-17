# Lichtvorschau mit Abschattung – 30. September 2026

Zweite Ausbaustufe für Quake I / vkQuake. Die Vorschau verwendet direkte,
diffuse Beleuchtung mit zwischengespeicherten Lichttexturen und Abschattung.
Sie ersetzt keinen Lichtcompiler.
Die Auswertung orientiert sich an den
[Quake-/ericw-Lichtparametern](https://ericw-tools.readthedocs.io/en/latest/light.html).

## Verhalten

- Lichtposition ist der Entity-Ursprung, unabhängig von Modell- und Auswahlgrenzen.
- `light` / `_light` berücksichtigen einen einzelnen Helligkeitswert oder RGB mit
  optionalem viertem Helligkeitswert. Explizite Eigenschaften haben Vorrang vor
  Defaults der Entity-Definition. Ohne Wert gilt für Quake 300.
- `_color` / `color` unterstützen RGB in 0–1 oder 0–255, unabhängig von der
  Schreibweise der Zahlen; die separate Farbe hat Vorrang. Half-Life erhält einen
  getrennten Kompatibilitätspfad für Byte-Farben und die andere `_falloff`-Bedeutung.
- Quake-Entfernungskurven: `wait`, `delay` 0–5, `_falloff` für lineare Lichter und
  `_anglescale` / `_anglesense`. Negative Helligkeiten subtrahieren Licht;
  lokales Minlight bildet eine Untergrenze.
- Scheinwerfer verwenden `target` oder `mangle` sowie `angle` / `_softangle`.
  Richtungslicht verwendet `_sun` oder Worldspawn-`_sunlight`, `_sun_mangle`
  und `_sunlight_color` samt üblichen Aliasnamen.
- Worldspawn-`_minlight`, `_minlight_color`, `_dist`, `_range` und `_gamma`
  werden berücksichtigt. Die vorhandenen Einstellungen `Lightning Intensity`
  und `Lightning Ambient` bleiben wirksam.
- Alle ausgewählten Lichtbeiträge werden summiert. Der bisherige feste Glanz
  und die kamerabhängige Flächenabdunklung entfallen bei aktiver Lichtvorschau.
  Auswahlfarbe, Nebel und Raster werden danach aufgetragen; Alpha bleibt erhalten.
- Brushes und Patches verwenden dieselbe Berechnung. Entity-Meshes verwenden
  geometrische Flächennormalen, da ihre bestehenden Vertexpuffer keine Normalen
  enthalten. Kameraorientierte Sprites und Browser-Vorschaubilder bleiben unbeleuchtet.

## Lichttexturen und Abschattung

Brushes, Patches und Decals erhalten gefilterte RGB-Lichttexturen. Die Berechnung
verwendet alle Lichtquellen, deren Reichweite die Fläche erreicht, ohne die Grenze
von 16 Lichtern der schnellen Vorschau. Dadurch entfallen nach der Berechnung
Übergänge durch unterschiedliche regionale Lichtlisten.

Strahlen gegen eine räumlich geordnete Kopie der Map-Geometrie prüfen, ob Licht
von einer Wand verdeckt wird. Sonnenlicht benötigt einen Treffer auf einer
Sky-Fläche ohne verdeckende Geometrie davor. Auch lokales Minlight wird auf
Sichtbarkeit geprüft; globales Worldspawn-Minlight bleibt eine Untergrenze.
Sky-Texturen selbst bleiben hell. `_nostaticlight` unterdrückt eine Lichtquelle.

Das Editor-Umgebungslicht verwendet jetzt standardmäßig 0,025 statt 1,0.
Der alte weiße Standard hatte jede Fläche mindestens mit voller Texturhelligkeit
angezeigt und den Kontrast praktisch aufgehoben. Bereits ausdrücklich gespeicherte
Benutzerwerte bleiben erhalten. Sehr hohe summierte Lichtwerte werden unter
Erhaltung ihrer Farbe auf einen Multiplikator von 2 begrenzt. Intensität und Gamma
werden auch bei fertigen Lichttexturen weiterhin im Shader angewendet.

## Aktualisierung und Aufwand

Die Berechnung beginnt nach 200 ms ohne relevante Änderung in höchstens zwei
Hintergrundaufgaben. Die Aufgaben arbeiten ausschließlich mit kopierten numerischen
Daten, ohne Map-Zugriffe oder OpenGL-Aufrufe. Weitere Änderungen verwerfen veraltete
Arbeit. Die bisherige Lichttextur bleibt bis zum fertigen Ersatz sichtbar. Bei neuer
Geometrie oder beim ersten Aktivieren erscheint zunächst die schnelle direkte
Vorschau ohne flächengenaue Schatten; danach werden die Lichttexturen übernommen.

Alle Ansichten teilen sich denselben Szenenstand. Reine Kamerabewegungen benötigen
keine erneute Lichtberechnung. Lichtänderungen verwenden die bestehende räumliche
Geometriesuche weiter. Änderungen an verdeckender Geometrie aktualisieren auch die
Beleuchtung unveränderter Flächen. Gamma und Helligkeit allein erfordern keine neue
Lichttextur. Zusammenhängende Zeichenbereiche mit gleicher Textur und Atlas-Seite
werden zusammengefasst. Die Atlas-Übertragung erfolgt seitenweise über mehrere
Bilder, um große einzelne Übertragungen zu vermeiden.

Die Auflösung beginnt bei ungefähr 16 Welteinheiten pro Lichttexel und wird für
große Szenen gröber. Ein Atlas enthält Seiten mit 512 × 512 Texeln; pro Geometriepuffer
sind höchstens 32 Seiten erlaubt. Die Dreiecke erhalten getrennte Bereiche mit
Randpolstern gegen Überblenden benachbarter Atlas-Einträge. Die zuvor eingeführte
Darstellungsunterteilung großer Dreiecke bleibt bestehen; die Map-Daten selbst
werden nicht verändert. Transparente Flächen behalten ihre Zeichenreihenfolge.

Falls das Speicherbudget überschritten wird oder Float-Texturen fehlen, bleibt
die schnelle direkte Vorschau aktiv und eine Meldung erscheint im Log. Sie verwendet
höchstens 16 Lichter je räumlichem Bereich. Entity-Modelle behalten diese direkte
Beleuchtung, ergänzt um eine zwischengespeicherte Sichtbarkeitsprüfung vom Mittelpunkt
ihrer Grenzen. Sie bekommen noch keine flächengenauen Schatten und werfen selbst
keine Modellschatten. Kameraorientierte Sprites bleiben unbeleuchtet.

## Grenzen und Validierung

Die Abschattung verwendet die gesamte gespeicherte Map-Geometrie, auch vorübergehend
im Editor ausgeblendete Brushes. Trigger, Illusionary-Entities, `_shadow 0`, Clip-,
Hint-, Skip- und Origin-Flächen sowie Quake-Flüssigkeiten blockieren das Licht nicht.
Alpha-maskierte Zäune werden vorläufig als geschlossene Flächen behandelt; durchlässige
Materialien und bewegliche Brush-Entities entsprechen nicht in jedem Detail dem
jeweiligen Compiler. Die begrenzte Lichttexturauflösung kann feine Schatten verlieren
oder sichtbare Übergänge an Dreieckskanten erzeugen.

Noch nicht enthalten: indirektes Licht, Sky-Domes (`_sunlight2/3`), Surface Lights,
Lichtanimationen, weiche Flächenschatten durch `_deviance`, Compiler-Kommandozeilen-
parameter und exakte vkQuake-Ausgabekorrektur. Surface-Light-Templates werden nicht
als falsche Punktlichter angezeigt.

Validierung: Release-Build der Anwendung; gezielte CPU-Prüfungen für Schatten,
Strahlreichweite, Sky-Sichtbarkeit, Selbstabschattung, negative Lichter, Minlight,
mehr als 16 Lichtquellen, Atlas-Ränder und Abbruch veralteter Berechnungen. Shader
mit dem lokalen OpenGL-Treiber kompiliert und verknüpft; Lichttextur-Koordinaten,
Float-Texturen, Ambient, Farberhaltung bei hoher Helligkeit und Gamma anhand tatsächlich
gerenderter Pixel geprüft. Visueller Map-Test und FPS-Messungen erfolgen durch den
Benutzer; ein neuer FPS-Wert wurde nicht gemessen.
