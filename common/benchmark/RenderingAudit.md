Rendering-Prüfung vom 26. September 2026

Nachtrag vom 30. September: Die direkte Lichtvorschau wurde für Quake I überarbeitet. Die frühere Auswahl von zwei Lichtquellen pro Fragment wurde ersetzt; Details und Grenzen stehen in [LightingPreview.md](LightingPreview.md). Die folgenden Messungen beschreiben weiterhin den damaligen Stand.

Nachtrag vom 27. September: `Renderer/Maximum FPS` begrenzt jetzt das Rendering je Ansicht (Standard 120, 0 unbegrenzt), direkt einstellbar unter View → Map Views → Maximum FPS. Die Begrenzung berücksichtigt auch direkte Paint-Anforderungen und verwendet einen Timer ohne Warteschleife. Ansichtsrahmen und Auswahlvolumen verwenden wiederverwendbare Geometriepuffer; der Rahmen wird bei Größenänderungen erneuert, das Auswahlvolumen verwendet einen transformierten Einheitswürfel. Für diesen Nachtrag wurde auf Wunsch ausschließlich der Build geprüft. Die GPU-Prüfungen weiter unten gehören zum Audit vom 26. September.

Geprüft wurden Bildaktualisierung, Brush-/Patch-Rendering, Beleuchtung, Transparenz, Grid sowie die neuen Kanten und Selection-Guides. Die folgenden Korrekturen sind im aktuellen Build enthalten. Die Prüfung kombiniert Quellcodeanalyse und gezielte GPU-Tests; sie ist kein vollständiges Profiling einer großen Map.

| Befund | Auswirkung und Korrektur |
| --- | --- |
| Falscher Typ für `AmbientLight` | Ein `vec3`-Uniform erhielt vier Komponenten. OpenGL verwirft diesen Aufruf. Jetzt werden drei Komponenten übertragen. |
| Double-Aufruf für Float-Uniforms | `glUniform1d` passt weder zu den Float-Uniforms der GLSL-1.20-Shader noch zum verwendeten macOS-Kompatibilitätskontext. Double-Eingaben werden vor der Übertragung in Float umgewandelt. |
| Unvollständige Patch-Einstellungen | Patches übernahmen Licht-, Nebel-, Schattierungs- und Helligkeitsparameter vom vorherigen Brush-Durchlauf. Beide Renderer initialisieren diese Einstellungen jetzt über dieselbe Funktion. |
| Patch-Transparenz ignoriert | Der gespeicherte Alpha-Wert wurde nicht verwendet. Er wird jetzt übertragen; transparente Durchläufe schreiben keine Tiefe und stellen den vorherigen Zustand wieder her. Maskierte Patch-Texturen erhalten ebenfalls die passende Shader-Einstellung. |
| Helligkeitskorrektur verändert Alpha | Die Korrektur multiplizierte alle vier Farbkanäle. Sie verändert jetzt nur RGB. Schwarze Auswahlfarben lösen außerdem keine Division durch null mehr aus. |
| Veraltete Lichtauswahl nach Kamerabewegung | Die Liste der nächsten Lichter wurde nur bei Dokumentänderungen erneuert. Sie berücksichtigt jetzt auch Positionsänderungen der Kamera. |
| Fehler beim Aufbau der Lichtliste | Wiederholte Aufrufe hängten Einträge an; Einträge ohne Lichtwert verbrauchten das Limit. Die Liste wird jetzt ersetzt und nur nutzbare Einträge zählen. Fehlerhafte Farben erhalten einen gültigen Ersatzwert. |
| Unvollständige Begrenzung der Lichtzahl | CPU-Übertragung und Shader-Schleife begrenzen jetzt beide auf 150. Der normale Aufrufer begrenzte bereits vorher; ein Überlauf im bisherigen normalen Ablauf wurde nicht nachgewiesen. |
| OpenGL-Abfrage im Statistik-Timer | Der Timer garantiert keinen aktuellen OpenGL-Kontext. Hardwaregrenzen werden jetzt bei der Initialisierung gespeichert und später ohne OpenGL-Abfrage gelesen. |
| Bildaktualisierung und Statistik | `autoRefresh=false` stoppte die kontinuierlichen Updates nicht. Der Schalter wird jetzt berücksichtigt; manuelle Updates bleiben möglich. Bildzähler und Größen sind initialisiert, die FPS-Berechnung ist gegen Division durch null geschützt. |

Die Implementierung liegt in [FaceRenderer.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/src/Renderer/FaceRenderer.cpp), [PatchRenderer.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/src/Renderer/PatchRenderer.cpp), [Face.fragsh](/Users/patricklindenberg/Development/TrenchBroom/app/resources/shader/Face.fragsh), [RenderContext.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/src/Renderer/RenderContext.cpp), [ShaderProgram.cpp](/Users/patricklindenberg/Development/TrenchBroom/common/src/Renderer/ShaderProgram.cpp) sowie den View-Klassen.

Bereits umgesetzt sind folgende Einsparungen:

- Keine Suche im Entity-Index bei unveränderter Kamera und gültigem Lichtcache; bei deaktivierter Beleuchtung entfällt die Aufbereitung vollständig.
- Keine zusätzliche Kopie der Lichtliste im FaceRenderer. Die bis zu 750 Uniform-Namen pro Durchlauf werden einmalig erstellt.
- Bei der Suche nach den nächsten Lichtern verwendet der Fragmentshader quadrierte Entfernungen. Statt bis zu 150 Quadratwurzeln für die Suche werden nur noch zwei für die anschließende Beleuchtung benötigt.
- Leere Patch-Flächendurchläufe werden nicht mehr in den RenderBatch aufgenommen.

Für größere Optimierungen ergibt sich aus dem Code diese Reihenfolge. Der jeweilige FPS-Gewinn muss an einer repräsentativen Map gemessen werden:

| Priorität | Ansatz | Nachweis im aktuellen Code |
| --- | --- | --- |
| 1 | Statische Ansichten nur bei Änderungen aktualisieren; Animationen gezielt takten | `RenderView::frameSwappedEvent` plant standardmäßig weiterhin jedes nächste Bild. Die Guide-Animation und Kamerabewegung müssen bei einer Umstellung weiterlaufen. |
| 2 | Geometriepuffer für Rahmen und Auswahlvolumen wiederverwenden | Am 27. September umgesetzt; siehe Nachtrag. |
| 3 | Kantenpuffer nur teilweise aktualisieren und kompakter speichern | `EdgeRendererMesh::prepare` baut bei einer Revision den gesamten Dreieckspuffer neu. Pro Kante werden sechs Vertices mit je 48 Byte gespeichert. Auch Änderungen anderer Vertexdaten können diesen Neuaufbau auslösen. |
| 4 | Kleinere räumliche Lichtlisten verwenden | Trotz nur zwei ausgewerteten Lichtquellen durchsucht jedes Fragment weiterhin bis zu 150 Kandidaten. Eine räumliche Vorauswahl könnte diese Arbeit reduzieren, muss aber gegen zusätzliche Renderdurchläufe abgewogen werden. |

Validierung: Release-Build erfolgreich, vier gezielte GPU-Tests mit 58 Prüfungen erfolgreich. Geprüft wurden Uniform-Typen und Lichtlimit, wiederholter Lichtaufbau mit ungültigen Farben, ein Patch-Durchlauf mit verändertem vorherigem Shader-/Tiefenzustand sowie tatsächlich gerenderte Transparenzwerte. Der unveränderte alte Fragmentshader verfehlte im Vergleichstest sechs Prüfungen: 25 % Deckkraft wurden je nach Auswahlfarbe zu ungefähr 41 %, 83 % oder 100 %. Mit dem korrigierten Shader bleiben es 25 %.

Die Tests liefen separat ohne Bedienung des Editors. Eine vollständige visuelle Prüfung der Map sowie Messungen der CPU- und GPU-Bildzeiten sind noch offen. Die angezeigte bisherige Render-Time misst CPU-Zeit für den Renderaufruf, keine abgeschlossene GPU-Bildzeit; daraus lässt sich kein GPU-Leistungsgewinn ableiten.
