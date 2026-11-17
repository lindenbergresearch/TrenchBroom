# Auswahl und Dateisystem: gezielte Upstream-Übernahmen

Referenz ist der Original-Clone `1cdbf7236` vom 12. November 2026. Die Änderungen
bleiben im bestehenden Qt-5-, Texture- und MapDocument-Aufbau des Forks.

## Große Selektionen

Die Entfernung vieler Nodes und Faces verarbeitet die Auswahl jetzt gebündelt.
Die zu entfernenden Einträge werden einmal sortiert; jede Auswahlliste wird einmal
gefiltert. Die Reihenfolge der verbleibenden Einträge bleibt erhalten. Die typisierten
Node-Listen werden gemeinsam aktualisiert. Auch eine direkt übergebene `nodes()`-Liste
ist zulässig, weil die Entfernungsliste vor der Änderung kopiert wird.

Grundlage: [Improve deselection performance](https://github.com/TrenchBroom/TrenchBroom/commit/c705ced93ef79be8683999a1b3ca1b8b6c3bd5f5).

## Archivnamen

WAD-, Quake-PAK-, Daikatana-PAK- und ZIP-Einträge verwenden dieselbe Pfadkonvertierung.
Unter Windows wird zunächst UTF-8 versucht, bei ungültiger Kodierung Latin-1.
POSIX-Systeme behalten die ursprünglichen Namensbytes bei. Die Suche faltet ASCII-
Großbuchstaben direkt im nativen Pfad, ohne Unicode oder ältere Namenskodierungen
über eine lokale Codepage umzuwandeln.

Grundlage: [Archivnamen ohne gültiges UTF-8](https://github.com/TrenchBroom/TrenchBroom/pull/5508).
Die vier kleinen Regressionsarchive unter `common/test/fixture/IO/ArchiveNames` stammen
aus diesem Upstream-Testbestand.

Zusätzlich gibt ZIP-Reload den alten miniz-Zustand frei und setzt die Leseposition
zurück. Leere ZIP-Namen werden übersprungen.

## Dateisystem-Cache

`DiskFileSystem` speichert einen Verzeichnisbaum im Arbeitsspeicher. Pfadprüfungen
und Verzeichnissuchen verwenden diesen Baum; Dateiinhalte werden weiterhin beim
Öffnen vom Datenträger gelesen. Ein gemeinsamer Baum mit Namensindex wird auch für
Archiv-Dateisysteme verwendet. Die originalen Schreibweisen der Namen bleiben erhalten.

Grundlage: [DiskFileSystem-Verzeichnisbaum](https://github.com/TrenchBroom/TrenchBroom/commit/fd8b2aaa2fc2636ea8fec34eee106703799f841e).

- `reload()` baut den Baum neu auf und wird durch virtuelle Dateisysteme weitergereicht.
- Erfolgreiche Schreib-, Kopier-, Verschiebe- und Löschvorgänge aktualisieren den Cache.
  Das gilt auch für atomare Schreibvorgänge mit temporärer Datei und Umbenennung.
- Extern hinzugefügte, umbenannte oder entfernte Dateien werden nach `reload()` sichtbar.
  Der bestehende Editor-Befehl zum Neuladen der Texturen aktualisiert zunächst die
  zugrunde liegenden Dateisysteme und danach die Shader.
- Leser und Reload sind durch ein Shared Mutex geschützt. Ein fehlgeschlagener Scan
  veröffentlicht keinen unvollständigen Baum. Ein entferntes Wurzelverzeichnis leert
  den Cache; Zugriffsfehler werden gemeldet und erhalten den bisherigen Baum.
- Verzeichnis-Symlinks werden berücksichtigt; Rückverweise auf Vorfahren werden nicht
  weiter verfolgt. Defekte Symlinks und spezielle Dateien werden nicht als Dateien geführt.
- Mount-Überlagerungen behandeln unterschiedlich geschriebene Namen ohne doppelte
  Einträge. Die zuletzt eingebundene Quelle behält Vorrang.

Die Suche nach Mod-Verzeichnissen und PAK-Dateien bleibt eine flache Suche, damit dafür
kein zweiter vollständiger Verzeichnisbaum aufgebaut wird. Es gibt keine neue persistente
Cache-Datei und keinen zusätzlichen Hintergrunddienst.

## Prüfungen

Regressionen prüfen große und gemischte Node-Auswahl, Face-Auswahl mit Undo/Redo,
Archivnamen und wiederholten ZIP-Reload, Schreiboperationen und Cache-Aktualisierung,
Mount-Prioritäten, gleichzeitige Leser, Symlink-Schleifen sowie das Neuladen von
Game-Dateisystemen vor der Shader-Suche.

Release-Build erfolgreich. Die gezielten 13 Tests bestehen mit 269 Prüfungen; der
erweiterte Lauf besteht mit 146 Testfällen und 73.061 Prüfungen. Dazu bestehen die
neun KDL-Pfadtests mit 108 Prüfungen.
