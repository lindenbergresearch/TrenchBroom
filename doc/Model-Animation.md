# Modellanimation in der 3D-Ansicht

Unter **View Settings → Objects → Entities** stehen zwei unabhängige Auslöser:

- **Animate selected models**: Die Animation läuft, solange die Entity oder ihre
  übergeordnete Gruppe ausgewählt ist. Standardmäßig eingeschaltet.
- **Animate models near the 3D camera**: Die Animation läuft innerhalb des
  eingestellten Abstands zur Modellbegrenzung. Standardmäßig ausgeschaltet.

Sind beide Optionen aktiv, genügt einer der Auslöser. Außerhalb der Reichweite
und ohne Auswahl wird wieder die ursprüngliche Pose dargestellt. Beim erneuten
Start beginnt die Animation von vorn. **Animation radius** verwendet Map-Einheiten
(Standard: 512). **Animation speed** reicht von 0 bis 400 Prozent; 100 Prozent
entspricht der ursprünglichen Zeitsteuerung, 0 pausiert die Wiedergabe.

Die Einstellungen werden in den Preferences unter `Map view/Model animation/`
gespeichert und über **Restore View Defaults** zurückgesetzt.

## Animationsdaten

Quake-MDL-Frame-Gruppen behalten ihre gespeicherten Zeitintervalle. Bei MDL-, MD2-
und MD3-Modellen werden aufeinanderfolgende Frame-Namen wie `stand1`, `stand2`,
`stand3` zu einer Sequenz zusammengefasst (10 Frames pro Sekunde). Die zum
konfigurierten Modell-Frame gehörende Sequenz wird abgespielt; andere Aktionen wie
Angriff oder Tod werden dadurch nicht an die Standanimation angehängt.

Bei über Assimp geladenen Modellen wird der zur konfigurierten Sequenznummer
gehörende Animationsclip verwendet. Knoten- und Skelettanimationen werden zwischen
den Schlüsselpositionen interpoliert. Die bisherigen Sequenznummern bleiben
erhalten. Modelle ohne passende Animationsdaten bleiben statisch.

Die Vorschau verändert weder Entity-Properties noch die Map oder den Undo-Verlauf.
Auswahl und Bearbeitung verwenden weiterhin die ursprüngliche Modellpose. In
2D-Ansichten bleibt ebenfalls diese statische Pose sichtbar.

## Aufwand beim Rendern

Modellinstanzen teilen sich die geladenen Posen und Render-Puffer. Neue Posen werden
vor der Vorbereitung des Render-Batches angefordert und nur einmal aufgebaut.
Importierte Clips verwenden bis zu 20 Vorschauposen pro Sekunde, begrenzt auf 60
Posen je Clip. Lange Clips behalten ihre Laufzeit bei entsprechend geringerer
Abtastrate. Der importierte Szeneninhalt bleibt für die Animation verfügbar;
erneutes Einlesen der Modelldatei pro Frame entfällt. Zusätzliche Vorschauposen
benötigen keinen eigenen Baum für die Mausselektion.

Die Zeitsteuerung verwendet verstrichene Zeit und berücksichtigt die vorhandene
Framerate-Begrenzung. Für die Wiedergabe wird kein zusätzlicher Render-Timer gestartet.
