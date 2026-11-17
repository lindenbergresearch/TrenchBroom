# Abgleich: Brush-Erzeugung und -Bearbeitung

Stand: 14. November 2026.

Verglichen wurden die aktuelle Arbeitskopie dieses Forks (`HEAD 6296802de`, einschließlich lokaler Änderungen) und der eingebundene Original-Clone unter `/Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom` (`master`, `HEAD 1cdbf7236`, 12. November 2026). Referenz ist dieser lokale Master-Stand, kein nachträglich aus dem Internet aktualisierter Stand.

Die Untersuchung umfasst Formgeneratoren, das Shape-Werkzeug, konvexe Hüllen/CSG, Clipping, Extrusion, Vertex-Bearbeitung und unmittelbar zugehörige Auswahl- und UV-Übernahme. Rendering, Beleuchtung, Inspector-Umbauten und eine allgemeine Qt-/Projektmigration sind nicht Teil des Abgleichs.

**Ergebnis:** Viele Verbesserungen sind gezielt übertragbar. Am sinnvollsten sind zunächst vier begrenzte Stabilitätskorrekturen, anschließend die bessere Shape-Parameterverwaltung und neue Formen. Ein vollständiger Austausch des `BrushBuilder` oder der Brush-Klassen wäre wegen der inzwischen stark veränderten Upstream-Architektur unnötig aufwendig.

Die Editorquellen wurden bei diesem Abgleich nicht geändert. Für die Prüfung wurde ein kleiner Backport auf einer temporären Kopie der Geometrieheader erprobt.

## 1. Konkrete Fehler und empfohlene erste Korrekturen

### 1.1 Konvexe Hüllen: unterbrochene oder mehrteilige Horizon-Seams

**Priorität: zuerst. Aufwand: klein bis mittel, einschließlich Regressionstests.**

Im Fork setzt `Seam::push_back` voraus, dass jede neue Kante an die vorherige anschließt. Außerdem behandelt `checkSeamForWeaving` mehrere Schleifen nur durch eine Assertion. Rundungsfehler bei fast koplanaren Punkten können diese Voraussetzungen verletzen. `visitFace` kann den Fehler bisher nicht an den Aufrufer zurückgeben.

Das betrifft unter anderem den Aufbau der konvexen Hülle beim Zusammenführen von Brushes und die Erzeugung eines Brushs aus Punkten.

Der Upstream-Fix [d292f5bdc](https://github.com/TrenchBroom/TrenchBroom/commit/d292f5bdc76685c857f180edbaec6201f8684d6b) macht das Anhängen und den rekursiven Besuch prüfbar, verwirft ungültige Seams und weist mehrere Schleifen vor dem Weaving zurück.

**Reproduziert mit dem 22-Punkte-Fall aus dem Upstream-Regressionstest zu #5480:**

| Variante | Ergebnis |
| --- | --- |
| Aktuelle Header mit Assertions | Abbruch in `Seam::push_back`, `checkEdge(edge)` |
| Aktuelle Header mit `NDEBUG` | `polyhedron=1`, aber `closed=0`, 11 Vertices, 11 Faces |
| Temporäre Kopie mit den übertragenen Absicherungen, mit Assertions | Geschlossene Hülle, 10 Vertices, 14 Faces |
| Gleiche temporäre Kopie mit `NDEBUG` | Geschlossene Hülle, 10 Vertices, 14 Faces |

Der Release-Fall muss nicht unmittelbar abstürzen: `MapDocument::csgConvexMerge` prüft `closed()` und lehnt diese Hülle ab. Die Geometrieberechnung ist dennoch fehlerhaft. Der isolierte Versuch zeigt, dass die Korrektur ohne Übernahme der neuen Speicherverwaltung möglich ist.

Betroffene Stellen: [Polyhedron_ConvexHull.h](/Users/patricklindenberg/Development/TrenchBroom/common/src/Model/Polyhedron_ConvexHull.h:367), [Polyhedron.h](/Users/patricklindenberg/Development/TrenchBroom/common/src/Model/Polyhedron.h:1850), [CSG Convex Merge](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/MapDocument.cpp:2415).

Bei der Umsetzung zusätzlich den Upstream-Map-Test mit `csgConvexMergeCrash.map` übernehmen; der isolierte Punkt-Test ersetzt diesen Integrationstest nicht.

### 1.2 Hohlzylinder: zu große Wandstärke vergrößert das Objekt

**Priorität: zuerst. Aufwand: klein bis mittel.**

Die bisherige Berechnung verschiebt die Begrenzungslinien um die Wandstärke nach innen, auch wenn der Innenraum längst verschwunden ist. Dadurch kann die vermeintliche Innenkontur wieder außerhalb der Außenkontur liegen.

**Mit dem aktuellen Release-Code reproduziert:** Bounds `(-32,-32,-32)` bis `(32,32,32)`, acht Seiten, `ToEdge`, Z-Achse:

| Wandstärke | Ergebnis |
| --- | --- |
| 16 | Acht Brushes, gemeinsame Bounds bleiben bei ±32 |
| 32 | Acht Brushes, gemeinsame Bounds bleiben bei ±32 |
| 64 | Acht Brushes, gemeinsame Bounds bleiben bei ±32; Innenkontur bereits über den Mittelpunkt verschoben |
| 128 | Acht Brushes, gemeinsame Bounds werden zu `(-96,-96,-32)` bis `(96,96,32)` |

Dieser Wert ist über die aktuelle Oberfläche erreichbar: Die Wandstärke kann bis 128 eingestellt werden.

Upstream prüft in `makeHollowCylinderInnerCircle`, ob eine Querabmessung kleiner oder gleich der doppelten Wandstärke ist. In diesem Fall werden Keilsegmente ohne offenen Innenraum erzeugt. Diese Behandlung lässt sich unabhängig vom zusätzlichen skalierbaren Kreismodus übernehmen. Alternativ könnte der Fork den ungültigen Wert mit einem verständlichen Fehler zurückweisen; die Upstream-Variante erhält die Vorschau innerhalb der gewünschten Außenkontur.

Quellen: [lokale Innenkontur](/Users/patricklindenberg/Development/TrenchBroom/common/src/Model/BrushBuilder.cpp:325), [aktuelle UI-Grenze](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/DrawShapeToolExtensions.cpp:136), [Upstream-Behandlung](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbMdlLib/src/BrushBuilder.cpp:210>).

### 1.3 Ico-Sphäre: Genauigkeit 0 verletzt den Generatorvertrag

**Priorität: zuerst. Aufwand: klein.**

Die aktuelle Spinbox erlaubt 0–4, während `sphereMesh` ausdrücklich `iterations > 0` voraussetzt. Upstream begrenzt die Oberfläche mit [ef5fa75e9](https://github.com/TrenchBroom/TrenchBroom/commit/ef5fa75e90446906f69f95ba158662fa734d2209) auf 1–4.

Mit aktiven Assertions führt der vorhandene 0-Pfad zum Abbruch. Im geprüften Release-Code sind Assertions deaktiviert; dort lieferte Genauigkeit 0 tatsächlich einen 20-Face-Brush. Es wäre daher ungenau, für jeden Build einen Absturz zu behaupten.

Empfehlung: Den UI-Fix übernehmen und bei einer Überarbeitung auch am öffentlichen Builder-Einstieg ungültige Parameter über `Result` behandeln. Eine bewusste Unterstützung von 0 wäre eine andere Designentscheidung und müsste den Generatorvertrag samt Tests entsprechend ändern.

Quellen: [Spinbox](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/DrawShapeToolExtensions.cpp:240), [Mesh-Vertrag](/Users/patricklindenberg/Development/TrenchBroom/common/src/Renderer/RenderUtils.h:127).

### 1.4 Clip-Werkzeug: veralteter Trefferindex

**Priorität: zuerst. Aufwand: klein. Quellcodebefund; kein eigener UI-Crash nachgestellt.**

`PointClipStrategy::renderHighlight` greift ungeprüft auf `m_points[index]` zu. Wird ein Clip-Punkt entfernt, kann ein noch gespeicherter Treffer auf einen inzwischen ungültigen Index zeigen. Die Upstream-Korrektur [4634850a5](https://github.com/TrenchBroom/TrenchBroom/commit/4634850a583725e91a31a017a64f3956cec69b77) prüft `index < m_points.size()` vor der Hervorhebung.

Diese Prüfung fehlt noch in [ClipTool.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/ClipTool.cpp:324) und ist unabhängig von den großen Upstream-Umbauten übertragbar.

## 2. Neue Funktionen, die sich sinnvoll übernehmen lassen

Die Aufwandsangaben sind eine qualitative Einschätzung des Anpassungsumfangs, keine Zeit- oder Leistungszusage.

| Funktion | Unterschied zum Fork / Nutzen | Aufwand und Abhängigkeiten |
| --- | --- | --- |
| Gemeinsame Shape-Parameter | Seitenzahl, Kreismodus, Wandstärke und weitere passende Werte bleiben beim Wechsel zwischen Formen synchron. Im Fork liegen sie getrennt in den einzelnen Extensions. | Mittel. Gute Grundlage für die folgenden Erweiterungen; ein gemeinsames Datenmodell mit Änderungsbenachrichtigung reicht. |
| Explizite X-/Y-/Z-Achse | Die Ausrichtung ist ein Werkzeugparameter statt einer Folge der gerade verwendeten Kamera/Ansicht. | Klein bis mittel. Controller, Parameter und Oberfläche gemeinsam anpassen; das bisherige automatische Verhalten kann optional erhalten bleiben. |
| „Apply“ auf die bestehende Auswahl | Eine vorhandene Auswahl kann anhand ihrer Bounds mit der gewählten Form und den aktuellen Parametern neu erzeugt werden. | Mittel. Auswahl, Elternknoten, Texturen/Attribute und Undo müssen bewusst behandelt werden. |
| Gruppierung mehrteiliger Formen | Hohlzylinder, Treppen, Bögen und Felsen bleiben als zusammengehörige Einheit auswählbar. Upstream bietet dafür einen gespeicherten Toggle; ein einzelner Brush wird nicht zusätzlich gruppiert. | Klein bis mittel. Bestehende Gruppen-/Transaktionsfunktionen nutzen und die eigenen Layer-/Prefab-Abläufe erhalten. |
| Skalierbare Rundformen | Dritter Kreismodus neben Kanten-/Eckausrichtung. Beim Strecken bleiben die runden Enden erhalten, anstatt die ganze Kontur elliptisch zu skalieren; dadurch entstehen auch kapselartige Formen. Gilt für Zylinder, Hohlzylinder, Kegel und UV-Sphäre. | Mittel. `CircleShape`-Variante und die Kreis-/Ringberechnung übernehmen oder mit einem kompatiblen Adapter ergänzen. |
| Treppen | Stufenhöhe und vier horizontale Laufrichtungen; Generator baut mehrere normale Cuboids. | Klein bis mittel. Geometrisch gut isolierbar, keine neue primitive Brush-Art nötig. |
| Bögen und Zwickelfüllung | Halbkreisförmige Bögen aus Keil-Brushes, frei wählbare Tunnelachse und Wandstärke. Optional füllen zusätzliche Brushes die Lücke zwischen Bogen und umschließendem Rechteck. | Mittel. Kreis-/Hohlzylinderhelfer wiederverwenden; auch die späteren Fehler- und Degenerationskorrekturen mitnehmen. |
| Felsformationen | Acht Varianten: Boulder, Shelf, Strata, Crag, Crystal, Columns, Basalt, Cluster. Detailgrad, reproduzierbarer Seed und optionales Weiterzählen nach erfolgreicher Erzeugung. | Mittel bis groß. Eigenständiger Generator `RockFormation` plus Extension/UI. Zuerst den Hull-Fix übernehmen; kleine/flache Teilstücke und leere Ergebnisse korrekt behandeln. |
| Shape-Icons | Acht passende SVGs und eine grafische Formauswahl sind bereits im Original vorhanden. | Klein. Mit der vorhandenen `VectorIcon`-/QSS-Verwaltung des Forks integrieren. |

Wichtige Quellen/Entwicklungsschritte:

- Parameter und Kreisvarianten: [5680b7518](https://github.com/TrenchBroom/TrenchBroom/commit/5680b7518df2bc20adb59cc14dee1c30f613f6d8), [89f63d572](https://github.com/TrenchBroom/TrenchBroom/commit/89f63d572a479c671066c95ccdb842afdb0e8a79), [13b2c3dad](https://github.com/TrenchBroom/TrenchBroom/commit/13b2c3dad79281e3b6a790a159e393a185dc7eb0); aktuelles [CircleShape](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbMdlLib/include/mdl/CircleShape.h:33>) und [Parameter-Modell](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/include/ui/DrawShapeToolParameters.h:38>).
- Achse und Apply: [3be904b85](https://github.com/TrenchBroom/TrenchBroom/commit/3be904b85b9018476536d90bf14d9e3d4e5fa75a), [101440842](https://github.com/TrenchBroom/TrenchBroom/commit/1014408428724f0539220052ceef4056f08fe705); heutiger [Apply-Ablauf](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/src/DrawShapeTool.cpp:45>).
- Treppen: [c1198eebc](https://github.com/TrenchBroom/TrenchBroom/commit/c1198eebc4dad33c985c8a8f3f328906c931f581); heutiger [Generator](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/src/DrawShapeToolExtensions.cpp:293>).
- Bögen: [10f0e6d39](https://github.com/TrenchBroom/TrenchBroom/commit/10f0e6d392834b65aa2ad230af1a7bfd561f2458), Zwickelfüllung [0c2714716](https://github.com/TrenchBroom/TrenchBroom/commit/0c27147164a1aa73c0819f57d877c7e2d7492db0), Fehlerweitergabe [1d533a786](https://github.com/TrenchBroom/TrenchBroom/commit/1d533a786aa3f8bd347cfb54a052562957a1403d).
- Felsen: [5da813936](https://github.com/TrenchBroom/TrenchBroom/commit/5da8139369dd4d66c80022508103f869386ad371), dünne/kleine Teilstücke [c22bfbb2b](https://github.com/TrenchBroom/TrenchBroom/commit/c22bfbb2ba96c0bcfe64c8fa1d6168d41e070e96), plattformübergreifende Zufallszahlen [b503d7412](https://github.com/TrenchBroom/TrenchBroom/commit/b503d741247bc2e2c92d106606fee0be98d534e4); aktueller [Generatorvertrag](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbMdlLib/include/mdl/RockFormation.h:50>).

**Wichtige Einschränkung von Apply:** Upstream speichert an den Brushes keine dauerhafte parametrische Konstruktionshistorie. Apply erzeugt eine neue Form über den Bounds der Auswahl und entfernt die ausgewählten Knoten erst, wenn tatsächlich neue Brushes vorliegen. Es verwendet aktuelles Material und Erzeugungsdefaults. Das ist kein verlustfreies Bearbeiten beliebiger vorhandener Objekte. Der aktuelle Apply-Button kann allgemein bei ausgewählten Nodes aktiv sein; für diesen Fork wäre eine klar begrenzte Auswahlregel sinnvoll, insbesondere bei Gruppen, Entities und verlinkten Prefabs.

**Keine zusätzliche Hollow-Funktion für alle Formen:** Trotz entsprechend benannter Upstream-Testabschnitte erzeugen Kegel und beide Sphären dort weiterhin einen einzelnen massiven Brush. Die Generatorimplementierung wertet `hollow` bei diesen Formen nicht aus. Als neue vorhandene Funktion sollte deshalb nur das tatsächlich implementierte Verhalten beschrieben werden.

## 3. Verbesserungen bei der Bearbeitung

### Extrusion und Stempeln

Upstream hat das Picking an sichtbaren Silhouettenkanten überarbeitet, verarbeitet mehr Kandidaten und lässt einen Kantentreffer einen Flächentreffer nur an einer echten Berührungsnaht verdrängen. Das ist für dicht aneinanderliegende Brushes relevant.

Hinzu kommen:

- Gemeinsames Extrudieren koplanarer Flächen mit entgegengesetzten Normalen an einer Berührungsnaht.
- Prüfung auf tatsächliche Polygonüberlappung für solche gegenüberliegenden Flächen; dieselbe Ebene allein reicht nicht.
- „Stamping“: Eine Fläche entlang ihrer Normalen duplizieren und aus beiden Polygonen einen neuen Brush bilden. Bei einem schrägen/verjüngten Brush setzt das dessen Schräge nicht fort, wie eine normale Extrusion es tun würde.

Quellen: [38d6fd7f6](https://github.com/TrenchBroom/TrenchBroom/commit/38d6fd7f63cc89609e201347e9f8d53f20e2737d), [4566ffbab](https://github.com/TrenchBroom/TrenchBroom/commit/4566ffbabcb8b93690004f3523fe8a22af2418cf), [8fe8aef56](https://github.com/TrenchBroom/TrenchBroom/commit/8fe8aef56fb415cbba676efdcec0ee215767cd68), [852e9281a](https://github.com/TrenchBroom/TrenchBroom/commit/852e9281a4af709f43cd829db3d3ec90508cc1e0), [0bb951142](https://github.com/TrenchBroom/TrenchBroom/commit/0bb951142b8a4e25498b56962f377b1ff3a0fb70), Eingabeanbindung [688938ef5](https://github.com/TrenchBroom/TrenchBroom/commit/688938ef58d7141dea094dba0847de1d0a242630).

**Empfehlung: eigenes Paket, Aufwand mittel bis groß.** Picking, neue Nachbarschaftsregeln und die dazugehörigen Korrekturen zusammen portieren. Die Überlappungsprüfung ist eine Absicherung der neuen Funktion, kein Beleg dafür, dass der Fork schon genau diesen Fehler besitzt. Die im Original hinzugekommenen Controller-/Extrude-Tests sind für diesen Port besonders wertvoll.

### Vertex-Transformationen statt ausschließlich Verschiebung

Der Fork besitzt für selektierte Vertices/Edges/Faces im Wesentlichen Delta-Verschiebungen. Upstream hat diese Operationen auf Transformationsmatrizen verallgemeinert und kann das Rotate-Werkzeug parallel zur Vertex-Auswahl betreiben.

Quellen: [53b1fb588](https://github.com/TrenchBroom/TrenchBroom/commit/53b1fb588b98ecc79e71098124f6487b1f2721c9), [d09d7f9cd](https://github.com/TrenchBroom/TrenchBroom/commit/d09d7f9cd11f97cd57a6874908bb609f316f4a60), aktuelle [Brush-API](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbMdlLib/include/mdl/Brush.h:168>).

**Aufwand: groß.** Die Änderung betrifft Brush-Validierung, Dokumentoperationen, Handle-Auswahl, Undo/Redo und die Werkzeuge. Nur die gegenseitige Werkzeugsperre zu lockern wäre nicht ausreichend.

Im Clone existiert außerdem `origin/5564-vertex-scaling` mit Skalierungs-/Bounds-Korrekturen vom 14. November 2026, beispielsweise `5b7f1dc6b`. Diese Commits sind **keine Vorfahren des verglichenen Master-HEADs**. Sie wurden daher nicht als bereits im Master fertig vorhandene Verbesserungen eingeordnet. Ein späterer Skalierungs-/Shear-Port sollte diese Entwicklung separat berücksichtigen.

### Zusammenhängende koplanare Flächen selektieren

Upstream kann per erweitertem Doppelklick zusammenhängende koplanare Flächen über mehrere Brushes hinweg auswählen. Im Fork wählt der Flächen-Doppelklick die Faces des getroffenen Brushs; die Flood-Auswahl fehlt.

Quelle: [2227f89e4](https://github.com/TrenchBroom/TrenchBroom/commit/2227f89e4ff52dc699da005557d7f04802ec3daa), heutige [SelectionTool-Implementierung](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/src/SelectionTool.cpp>).

**Aufwand: mittel.** Nützlich für Wände/Böden aus mehreren Brushes. Sichtbarkeit, Sperren, Gruppenkontext und bestehende Auswahlmodifikatoren müssen dabei die Regeln des Forks behalten.

## 4. Was bereits vorhanden ist oder nicht direkt passt

| Upstream-Änderung | Befund im Fork |
| --- | --- |
| UV-Übernahme beim CSG-Merge, `d371727c3` | Die Prüfung einer nicht vorhandenen Seam-Richtung ist bereits in `BrushFace::copyTexCoordSystemFromFace` enthalten. Die isolierte Valve-Probe mit geänderten Offsets und anschließendem `cloneFaceAttributesFrom` ergibt einen unveränderten Brush. Kein erneuter Port erforderlich. |
| Aktualisierung von Clip-/Extrude-Werkzeug bei Selektionswechseln | Die entsprechenden Observer sind bereits vorhanden. Das ersetzt nicht die zusätzlich fehlende Indexprüfung im Clip-Rendering. |
| Redo nach zusammengefassten Vertex-Drags, `c22ca1598` | Der moderne Fix betrifft `NodeHandleCommand`. Die älteren `BrushVertexCommand`-/Edge-/Face-Kommandos des Forks übernehmen den Ziel-Endpunkt beim Collate bereits. Diesen neuen Architekturfehler nicht ungeprüft in den alten Aufbau zurückübersetzen. |
| Shift-Alt-Merge, `e6016a7cd` | Upstream korrigiert den Zugriff nach einer Änderung des Hit-/Handle-Datentyps. Der alte Fork speichert an dieser Stelle noch direkt `vm::vec3` und liest ihn passend aus. Kein entsprechender Datentypfehler nachgewiesen. |
| Ganze Brushes verschieben, drehen, skalieren und scheren | Diese Funktionen sind schon vorhanden. Der neue Mehrwert liegt insbesondere bei ausgewählten Teilkomponenten, nicht bei einer erstmaligen Unterstützung kompletter Objekte. |

Quellen im Fork: [UV-Übernahme](/Users/patricklindenberg/Development/TrenchBroom/common/src/Model/BrushFace.cpp:181), [Kommando-Collate](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/BrushVertexCommands.cpp:95), [Vertex-Merge](/Users/patricklindenberg/Development/TrenchBroom/common/src/View/VertexToolController.cpp:99).

## 5. Optimierungen und größere Grundlagenänderungen

- **Flache Maps/Sets:** Upstream ersetzt mehrere temporäre `std::map`-/`std::set`-Strukturen in `Brush` und `Polyhedron_Matcher` durch `kdl::flat_map`/`flat_set` ([8ba3084d2](https://github.com/TrenchBroom/TrenchBroom/commit/8ba3084d24fd1357b83cfaf5a12a120700876671), [26df37b55](https://github.com/TrenchBroom/TrenchBroom/commit/26df37b5598f8bbd438e6394d38b3ad4af3d1653)). Das ist ein gezielter Optimierungskandidat für Zuordnungen und Duplikaterkennung. Die Container-Hilfen fehlen im alten KDL; nach Anpassung sollten große Brush-Operationen gemessen werden. Ein bestimmter FPS-/Laufzeitgewinn ist durch diesen Abgleich nicht belegt.
- **Pool-Speicherverwaltung für Polyeder:** [a4289274a](https://github.com/TrenchBroom/TrenchBroom/commit/a4289274a5fbca24c5b1a3e2217a58206d6b3537) trennt Eigentum von den zirkulären Listen. Das verändert Erzeugung, Entfernen und Lebensdauer aller Polyederteile sowie die KDL-Listen. Das ist ein eigenständiger größerer Port; nicht als einzelne Header-Optimierung übernehmen. Für den nachgewiesenen Hull-Fix ist er nicht erforderlich.
- **UV-Validierung:** Die neuen UV-/Surface-Attribute und die validierenden `Result`-Pfade sind eine größere Schnittstellenänderung. Für Shape-Ports genügt zunächst die Übersetzung auf die vorhandenen `BrushFaceAttributes`. Die gesamte BrushFace-/UV-Hierarchie mitzunehmen würde den Umfang erheblich ausweiten.
- **Normalentransformation:** Der mathematische Fix [427c4ef2a](https://github.com/TrenchBroom/TrenchBroom/commit/427c4ef2a64b9df9f58d3e2cd33f6baff5cc2992) ist im alten VM noch nicht enthalten. Der Upstream-Commit stellt aber ausdrücklich klar, dass der bestehende `BrushFace::transform`-Anwendungsfall davon nicht betroffen ist. Er ist daher kein Nachweis eines aktuellen Brush-Skalierungsfehlers und für diese Aufgabe nachrangig.

## 6. Empfohlene Übernahmereihenfolge

1. **Stabilität:** Hull-Absicherungen, Hohlzylinder-Grenzfall, Ico-Spinbox/Parametervertrag, Clip-Trefferindex. Passende Regressionsfälle vor bzw. mit der Änderung portieren.
2. **Shape-Grundlage:** Gemeinsame Parameter, Achsenauswahl, sinnvoll begrenztes Apply, optionale Gruppierung und Shape-Icons. Bestehende `MapDocument`-/Undo- und QSS-/Icon-Verwaltung behalten.
3. **Neue Generatoren:** Skalierbare Rundformen, Treppen, Bögen/Zwickel; danach Felsformationen inklusive der späteren Robustheits- und Seed-Korrekturen.
4. **Bearbeitung:** Verbessertes Extrude-Picking und Stamping, koplanare Flächenauswahl, anschließend die größere Vertex-Transformations-Erweiterung.
5. **Optimierung:** Flache Container messen; Pool-Verwaltung nur als gesondertes, vollständig getestetes Vorhaben.

Für diese gezielten Ports ist eine vorherige vollständige Qt-6- oder Upstream-Projektmigration nicht grundsätzlich nötig. Die Geometriealgorithmen können auf die vorhandenen Typen und APIs angepasst werden. Im Original wurden dagegen inzwischen unter anderem `MapDocument`/`Map`, Bibliotheksgrenzen, Namespaces, UV-/Surface-Attribute und Ownership-Schnittstellen umgebaut. Diese Änderungen pauschal zu übernehmen wäre ein deutlich anderes Projekt.

## 7. Prüftiefe und Testlücken

Durchgeführt wurden Quellcode- und Commit-Abgleich sowie isolierte, kompilierte Proben mit der bestehenden Geometrie bzw. den vorhandenen Release-Objekten. Ausgeführt wurden der Hull-Grenzfall mit/ohne Assertions, die temporäre Hull-Korrektur in beiden Varianten, Hohlzylinder mit vier Wandstärken, Ico-Genauigkeit 0/1/2 und die Valve-UV-Übernahme.

Es gab keinen vollständigen Build des Original-Clones, keinen Performance-Benchmark und keinen neuen vollständigen End-to-End-Test aller Editorwerkzeuge. Die Clip-Indexprüfung und die größeren Feature-Ports sind Quellcodebefunde; ihre konkrete Integration bleibt zu testen.

Die lokale Datei [tst_BrushBuilder.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/test/src/Model/tst_BrushBuilder.cpp) enthält lediglich zwei Cube-Testfälle. Als Portiergrundlage bieten sich die erweiterten Upstream-Tests für [BrushBuilder](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbMdlLib/test/src/tst_BrushBuilder.cpp>), [Shape-Extensions](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/test/src/tst_DrawShapeToolExtensions.cpp>) und [Shape-Apply/Gruppierung](</Users/patricklindenberg/Development/origin trenchbroom/TrenchBroom/lib/TbAppLib/test/src/tst_DrawShapeTool.cpp>) an. Wichtig sind Achsenvarianten, kleine/degenerierte Bounds, zu große Wandstärken, Attribute, Auswahl/Undo und leere Generatorergebnisse.

Der anschließende Übernahmestand und die Validierung sind in [Umsetzung des Brush-Abgleichs](Brush-Upstream-Umsetzung.md) dokumentiert.
