# Prefabs

Das Werkzeug **Prefab** ist über **View → Show Prefab Browser** erreichbar. Es lässt sich wie die anderen Inspector-Werkzeuge andocken, in einen Tab aufnehmen und als Fenster auslagern.

## Bedienung

- **Add** speichert die Editor-Auswahl. Bei ausgewählten Faces wird der zugehörige Brush übernommen. Gruppen- und Entity-Namen dienen als editierbarer Vorschlag. Der Name kann einen Pfad enthalten, beispielsweise `outside/trees/SmallOak`.
- Im Speicherdialog lässt sich die Vorschau mit gedrückter linker Maustaste drehen. **Reset View** oder ein Doppelklick setzt die Ansicht zurück. Der gewählte Blickwinkel und das daraus erzeugte Vorschaubild werden gespeichert; die Editor-Auswahl und deren Geometrie bleiben unverändert.
- **Remove** entfernt den gewählten Bibliothekseintrag. Bereits eingefügte Objekte und ihre importierten Ressourcen bleiben erhalten.
- **Insert**, Doppelklick oder Ziehen in eine Renderansicht fügen ein Prefab ein. Der Vorgang einschließlich Ressourcenregistrierung und Positionierung lässt sich gemeinsam rückgängig machen.
- **Copy** erzeugt unabhängige Gruppen. **Link** erzeugt Instanzen, die Änderungen innerhalb der aktuellen Map teilen. Weitere Link-Einfügungen verwenden die bestehende Map-Instanz einschließlich ihrer Änderungen. Die Bibliotheksvorlage wird dabei nicht geändert. Die Verknüpfung bleibt beim Speichern und erneuten Laden der Map erhalten.
- Die Ansichts-Icons wechseln zwischen Baum und Kacheln. Die Kacheln zeigen nur den aktuellen Ordner. Ein Doppelklick auf einen Ordner öffnet ihn; der Pfeil nach oben, `Alt+Up` und die anklickbare Pfadleiste wechseln zum übergeordneten Ordner. Ansicht und Ordner werden gespeichert.
- Zoom verändert die Vorschaugröße. Die Suche gilt in der Kachelansicht für den aktuellen Ordner, in der Baumansicht für die Bibliothek.

Auch die Kontextmenüs im Editor und im Layerbaum bieten **Save Selection as Prefab…** an. Bei abgeschalteter oder zurückgestellter Layer-Synchronisation verwendet dieser Befehl die Baum-Auswahl, ohne zuvor die Editor-Auswahl zu verändern.

## Ablage

Die Bibliothek liegt unter `prefabs` im Verzeichnis der Benutzereinstellungen. Jeder Eintrag besitzt ein eigenes Verzeichnis, zum Beispiel:

```text
prefabs/outside/trees/SmallOak/
    prefab.json
    fragment.map
    preview.png
    assets/…
```

`prefab.json` enthält Version, ID, Label, Pfad, Beschreibung, Spielkonfiguration, Map-Format, Maße, Vorschauwinkel und Ressourcenverzeichnis samt Prüfsummen. Beim Speichern werden nur vollständig geschriebene Verzeichnisse veröffentlicht. Bestehende Einträge werden nicht überschrieben.

Die Vorschau wird einmalig aus der gewählten Ansicht als Rasterbild erzeugt. Sie zeigt Brush- und Patch-Geometrie mit gemittelten Texturfarben; Punktentitäten erscheinen als Begrenzungskörper. Die interaktive Ansicht arbeitet mit einer Geometriekopie und benötigt keinen OpenGL-Kontext. Der Browser verwendet weiterhin das gespeicherte Bild.

Erfasst werden verwendete WADs, verfügbare Quake-Paletten, Texturdateien, deklarierte Entity-Dateipfade, Entity-Modelle sowie erkannte Shader-, Bild- und Skin-Verweise. Dateien können auch aus Spielarchiven gelesen werden. Nicht auflösbare Ressourcen werden beim Anlegen angezeigt und unter `MissingAssets` dokumentiert.

WADs liegen gemeinsam unter `prefabs/.wad-cache/<SHA256>/<Dateiname>.wad`. Identischer Inhalt wird auch bei verschiedenen Quelldateinamen nur einmal gespeichert. Prefabs und neu eingefügte Maps verweisen direkt auf diese Datei. Die Archive bleiben unverändert, einschließlich der darin enthaltenen Texturserien. Neue Prefabs benötigen daher keine eigene WAD-Kopie und beim Einfügen entsteht keine weitere Kopie.

Ältere Prefab-Verzeichnisse werden beim Laden auf die gemeinsame Ablage umgestellt. Die bisherigen WAD-Pfade unter `prefabs/.assets` bleiben für bereits gespeicherte Maps erhalten. Wo das Dateisystem Hardlinks unterstützt, teilen auch diese bisherigen Pfade denselben Speicherplatz mit der Cachedatei. Andere Ressourcen werden weiterhin im Importbestand `prefabs/.assets/<Prefab-ID>/resources` bereitgestellt und über einen zusätzlichen Suchpfad registriert.

Die gemeinsamen Dateien bleiben nach dem Entfernen eines Bibliothekseintrags erhalten, damit vorhandene Maps weiterhin funktionieren. Eine Sicherung der Bibliothek muss auch die verborgenen Verzeichnisse enthalten. Beim Weitergeben einer Map müssen ihre externen Ressourcen wie andere Map-Abhängigkeiten mitgeliefert werden.

## Gestaltung

Die Regeln für `PrefabTree`, `PrefabTiles`, `PrefabNavigation`, `PrefabLocation`, `PrefabCard`, `PrefabThumbnail`, `PrefabName`, `PrefabDimensions` und `PrefabStatus` stehen in `app/resources/stylesheets/base.qss`. Karten besitzen zusätzlich die Eigenschaften `layout="tree"/"tiles"`, `folder` und `selected`. Schriftgrößen und Detaildeckkraft verwenden dieselben konfigurierbaren Werte wie die übrige Oberfläche.

Der Speicherdialog ergänzt die Regeln `PrefabPreview` und `PrefabPreviewHint` für Hintergrund, Rahmen und Hilfstext der interaktiven Vorschau.
