# UI-Quellen

Die Dateien sind nach fachlicher Zuständigkeit gegliedert. Klassen und Namespaces
bleiben unverändert; Includes verwenden den vollständigen Pfad ab `common/src`,
zum Beispiel `#include "View/Inspector/Layer/LayerEditor.h"`.

| Verzeichnis | Zuständigkeit |
| --- | --- |
| Actions | Aktionen, Shortcuts, Menüs, Editor-Toolbars und deren Konfiguration |
| App | Editorrahmen, WelcomeWindow, zuletzt geöffnete Dokumente, App-Informationen |
| Commands | Undo-/Redo-Befehle, Transaktionen, Linked-Group-Befehle |
| Compilation | Compiler-Profile, Prozesse und deren Textausgabe |
| Dialogs | Allgemeine Dialoge, Spielstart, Export und Quelltextanzeige |
| Document | MapDocument, Selektion, Grid, Autosave und wiederholbare Operationen |
| Input | Eingabeereignisse, Pick-Anfragen und Drag-Tracker |
| Inspector | Docking, Floating und History; Unterordner für die einzelnen Fachbereiche |
| MapView | Kameras, Renderfenster und Ein-/Mehrfachansichten |
| Overlay | Kontextinformationen und deren Templates |
| Prefabs | Browser, Vorschau und Einfügen von Prefabs |
| Preferences | Einstellungsdialoge, Modelle und Auswahlhilfen |
| Theme | ThemeManager, QSSBuilder, Stilrollen, Qt-Helfer und Popup-Fallback |
| Tools | Werkzeugbasis und Unterordner Brush, Navigation, Selection, Transform, UV, Vertex |
| Widgets | Wiederverwendbare Controls, Layouts und Container; Lists und Info als Unterordner |

Die UI-Tests unter `common/test/src/View` folgen derselben Zuordnung.
`TestSourcePath.h` findet Testressourcen unabhängig von der Tiefe des Testordners.
Quellen und Header werden weiterhin explizit in `common/CMakeLists.txt` geführt.

Neue Dateien gehören in den passenden Fachordner. Ein weiterer Unterordner lohnt
sich für einen zusammenhängenden Teilbereich mit mehreren Klassen; einzelne
Dateien brauchen keinen eigenen Ordner.

Die Gestaltung der Widgets steht in `app/resources/stylesheets/base.qss`.
Die vollständige Übersicht zu Stilregeln und den bewussten Grenzen findet sich
in `doc/UI-Stylesheets.md`.
