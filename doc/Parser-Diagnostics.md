# Parser-Diagnosen mit Quelltext

Meldungen mit bekannter Quelle zeigen die unveränderte Originalzeile und bei
bekannter Spalte eine Markierung:

```text
entities.fgd:118:5: Unknown property definition type 'custom' for property 'model'
118 |     model(custom) : "Model"
    |     ^
```

Loglevel und Zeitstempel ergänzt weiterhin der Logger. Zusammengehörige Zeilen
bleiben eine Meldung, auch in der Zusammenfassung beim Laden einer Map. Ohne
bekannte Spalte wird die ganze Quellzeile gezeigt, ohne eine Position zu erfinden.
Ohne Quelltext bleibt das bisherige Meldungsformat erhalten.

Die Anbindung erfolgt für Map-Dateien und Map-Fragmente, FGD einschließlich
verschachtelter Includes, DEF, ENT und Quake-3-Shader. Auch ParserExceptions aus
diesen Einstiegspunkten erhalten Quelltext. Bereits angereicherte Exceptions
werden beim Weiterreichen nicht erneut formatiert.

## Verwendung

`ParserStatus::setSource(text, name)` übernimmt den Text in einen eigenen
Speicher und erstellt einen Index seiner Zeilen. LF, CRLF und CR werden wie im
Tokenizer ausgewertet. Wiederholtes Setzen derselben Quelle verwendet den Index
weiter. Der Name kann ein Dateipfad sein; sonst gilt der vorhandene Log-Präfix.

```cpp
status.setSource(input, "example.map");
status.warn(line, column, "Unexpected property");
```

`scopedSource` wechselt die Quelle für eine Teiloperation und stellt sie danach
auch im Fehlerfall wieder her. Das wird insbesondere für FGD-Includes genutzt.

```cpp
const auto source = status.scopedSource(includedText, includedPath.string());
parseIncludedDefinitions(status);
```

`location(line, column)` liefert einen `SourceLocation`-Wert mit Zeile, Spalte,
Dateiname und einer eigenen Kopie der Originalzeile. Er bleibt gültig, wenn der
Eingabetext freigegeben oder die aktuelle Quelle gewechselt wurde. FGD-Klassen
bewahren diesen Wert auf, damit spätere Vererbungs- und Duplikatmeldungen auf die
richtige Include-Datei verweisen.

```cpp
const auto origin = status.location(line, column);
// ... spätere Validierung ...
status.error(origin, "No matching super class found");
```

Tokenizer-Spalten sind weiterhin einsbasierte Bytepositionen. Für die sichtbare
Markierung werden UTF-8-Zeichen berücksichtigt und Tabs beibehalten. Die Ausgabe
ist für eine Schrift mit fester Zeichenbreite gedacht. Überlange Spaltenangaben
werden auf das Zeilenende begrenzt, ohne große Füllzeichenketten anzulegen.

## Prüfen

```sh
QT_QPA_PLATFORM=offscreen \
  cmake-build-cpm-release/regression/tb-regression-tests '[parser-status]'
```
