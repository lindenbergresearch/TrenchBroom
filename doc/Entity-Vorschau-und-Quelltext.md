# Entity-Vorschau und Objektquelltext

## Audio

Unter **View Settings → Objects → Entities** stehen folgende Einstellungen:

- **Preview audio on selection**: Audio einer einzelnen ausgewählten Point-Entity abspielen.
- **Loop audio preview**: Wiederholen statt einmaliger Wiedergabe; standardmäßig aus.
- **Audio volume**: Lautstärke von 0 bis 100 %, standardmäßig 60 %.

Erkannt werden Dateireferenzen unter anderem in `noise`, `sound`, `soundfile`, `message`
und benutzerdefinierten Properties. Numerische Sound-IDs und reine Textnachrichten
werden nicht als Audiodateien behandelt. Die Ressourcensuche verwendet dieselben
Spielverzeichnisse und Archive wie die Modellanzeige. Sowohl `world/example.wav`
als auch `sound/world/example.wav` sind möglich; absolute Dateipfade ebenfalls.

Die Vorschau funktioniert auch bei der Auswahl im Layer-Baum ohne Synchronisation
mit dem Editor. Mehrfachauswahl, Abwahl, Entfernen des Objekts, Schließen der Map
oder Wechsel in ein anderes Editorfenster bzw. eine andere Anwendung beenden die Wiedergabe. Für gepackte Ressourcen
wird nur die aktuelle Datei vorübergehend extrahiert und anschließend entfernt.

WAV wird unterstützt; weitere erkannte Formate (z. B. MP3, OGG, FLAC) hängen von den
verfügbaren Decodern des Qt-Multimedia-Backends auf dem jeweiligen System ab.
Fehlende oder nicht abspielbare Dateien werden im Log gemeldet. Audiodateien über
64 MiB werden nicht für die Auswahlvorschau geladen.

Die gespeicherten Preferences liegen unter `Map view/Audio preview/Enabled`,
`Map view/Audio preview/Loop` und `Map view/Audio preview/Volume`.

## Modelle über Entity-Properties

Point-Entities ohne wirksame Modellvorgabe aus der Spieldefinition können jetzt
direkt über `model` ein Modell aus den Spielressourcen anzeigen. Eine vorhandene
FGD-Modellvorgabe hat weiter Vorrang. Die Sichtbarkeit richtet sich nach dem
bestehenden Schalter **Show point entity models**.

Die bestehenden Modelllader übernehmen Geometrie und Texturen. Position und Drehung
werden angewendet; `skin` und `frame` werden berücksichtigt. Wenn keine Skalierung
in der Spiel-/Modellkonfiguration vorgegeben ist, werden `modelscale_vec`,
`modelscale` oder `scale` ausgewertet. Referenzen auf BSP-Untermodelle (`*1` usw.)
werden nicht als externe Modelldateien geladen.

**Focus Camera on Selection** berücksichtigt bei eingeblendeten Modellen deren
tatsächliche Ausdehnung, damit die Kamera bei großen Modellen außerhalb der Geometrie bleibt.

## Quelltext im Layer-Inspector

Ein einzelner gewählter Eintrag bietet **Show source…** im Kontextmenü und den
Code-Button (`</>`) in der Werkzeugleiste. Das funktioniert auch für gesperrte und
unsichtbare Objekte, ohne die Editor-Auswahl verändern zu müssen.

Der schreibgeschützte Dialog enthält:

- **FGD definition** bei Entities mit FGD-Definition: den Originaltext der Klasse
  mit Dateipfad und Zeile, auch aus einer eingebundenen FGD. Geerbte Properties
  werden nicht in den Originaltext hineingeschrieben.
- **Current MAP fragment**: einen Ausschnitt des aktuellen Editorzustands mit den
  notwendigen übergeordneten Gruppen/Entities, einschließlich ungespeicherter Änderungen.
- **Saved MAP** bei vorhandener MAP-Datei bis 16 MiB: den unveränderten gespeicherten
  Stand. Die ursprüngliche Zeilenposition wird angezeigt, soweit noch vorhanden;
  nach Änderungen kann sie vom aktuellen Objekt abweichen.

Die Anzeige hält eine Textkopie. Sie bleibt bei weiteren Map-Änderungen unverändert
und verändert beim Erstellen keine Dateipositionen der Originalobjekte.

Schrift, Größe, Text-/Hintergrund-/Auswahlfarben und Abstände sind in `base.qss`
unter **OBJECT SOURCE VIEWER** konfiguriert. Die Selektoren sind
`QDialog#nodeSourceDialog`, `QLabel#nodeSourceLocation` und
`QPlainTextEdit#nodeSourceText`. Für die Syntaxfarben stehen
`qproperty-keywordColor`, `qproperty-stringColor`, `qproperty-numberColor`,
`qproperty-commentColor` und `qproperty-propertyColor` zur Verfügung.

Zum Bauen wird zusätzlich das Qt-Modul **Multimedia** benötigt. Die vorhandene
macOS-Paketierung berücksichtigt es über `macdeployqt`; die Windows-Paketierung
kopiert die verfügbaren Multimedia-Backends mit.
