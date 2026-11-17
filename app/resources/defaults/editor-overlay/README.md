# 3D context overlay

In **Preferences → View → 3D Context Overlay**, enable/disable the overlay or
choose **Create theme…** to copy these files into an empty folder. Select that
folder as the theme. Saving changes updates all 3D views automatically, including
when your editor saves via file replacement. An empty theme path uses the built-in
theme. Invalid JSON/templates keep the last valid theme and display an error.

The panel is anchored at the top right, does not intercept mouse/keyboard input,
and stays inside the viewport. Large content is clipped to the available height.
It updates after document/selection/tool changes, not on every rendered frame.

## Files and styles

`overlay.json` selects the HTML, QSS and text CSS files. `width` (160–1200) and `margin`
(0–100) use logical screen pixels, so Retina scaling is automatic.
`style.qss` is a Qt stylesheet, scoped to `QLabel#EditorContextOverlay`.
Use `rgba(r,g,b,a)` with alpha 0–255, `border-radius`, `padding`, `font-family`,
`font-size`, `color` and `border`. The dynamic attributes `tool` and `selection`
allow state-specific QSS selectors; see the examples in the stylesheet.
Use installed/application fonts. HTML supports Qt rich text (paragraphs, spans,
tables, bold, inline text styles), not a browser, JavaScript, flexbox or web CSS.
Keep panel layout/background/base font settings in QSS. `text.css` configures
each text row independently: font size/family/weight, color, background, alignment
and paragraph margins. The first row (`.tool`) contains **only the tool**, at 14 pt;
details below use 10 pt (the selection name uses 11 pt). Every row in
`default.html` has a separate CSS class. For example:

```css
.tool { font-family: "Arial"; font-size: 16pt; color: #ffffff; margin-bottom: 12px; }
.position { font-family: "Menlo"; font-size: 9pt; color: #90c8ff; }
.geometry { font-weight: bold; margin-top: 6px; }
```

Add new lines as `<p class="my-row">…</p>` and style `.my-row` in `text.css`.
Templates are HTML fragments; their shared text stylesheet is injected
automatically. Qt supports [a subset of HTML/CSS](https://doc.qt.io/qt-5/richtext-html-subset.html).

## Templates

The built-in renderer implements this **Mustache subset**:

- `{{selection.name}}`: HTML-escaped text. Map strings cannot inject markup.
- `{{#selection.group}}…{{/selection.group}}`: show if nonempty/nonzero/true.
- `{{^selection.empty}}…{{/selection.empty}}`: inverse condition.
- `{{#textures}}{{name}}<br>{{/textures}}`: repeat a list; parent fields remain visible.
- `{{.}}`: current list item. `{{! comment }}`: ignored comment.
- Sections can nest (maximum 32 levels). Missing values render as empty.

Raw HTML values, partials, lambdas and delimiter changes are not supported.
HTML is written directly in the template; only substituted values are escaped.
Templates/styles are local UTF-8 files, at most 256 KB each.

To replace the whole layout for a state, add rules to `overlay.json`. First match
wins; omitted `tool`/`selection` match any. Unmatched states use `template`:

```json
"rules": [
  {"tool": "clip", "template": "clipping.html"},
  {"selection": "entity", "template": "entity.html"},
  {"selection": "multi", "template": "multiple.html"}
]
```

You can combine both keys in one rule. Create the referenced files in the theme
folder. For example, an entity layout can include:

```html
<b>{{tool.name}} · {{entity.classname}}</b><br>
{{#entity.properties.targetname}}Name: {{entity.properties.targetname}}<br>{{/entity.properties.targetname}}
{{#entity.propertyList}}{{key}}: {{value}}<br>{{/entity.propertyList}}
```

## Available data

| Field | Meaning |
| --- | --- |
| `tool.id`, `tool.name` | Tool ID and display name |
| `tool.select`, `.create`, `.assemble`, `.clip`, `.rotate`, `.scale`, `.shear`, `.vertex`, `.edge`, `.face` | True for the active mode |
| `tool.dragging` | A tool drag is in progress |
| `tool.angle`, `tool.center` | Rotation tool angle/center, when active |
| `selection.type` | `none`, `group`, `entity`, `brush`, `patch`, `layer`, `faces`, `multi` |
| `selection.typeName`, `.name`, `.count` | Display type, name and directly selected object/face count |
| `selection.empty`, `.multiple` | Boolean flags |
| `selection.group`, `.entity`, `.brush`, `.patch`, `.layer`, `.faces`, `.multi`, `.none` | True for the corresponding selection type |
| `selection.position`, `.size`, `.min`, `.max` | Vectors with `x`, `y`, `z`, `text` |
| `selection.positionLabel` | Point entity origin; otherwise selection bounds center |
| `selection.orientation`, `.orientationLabel` | Entity `mangle`/`angles`/`angle` as stored, or a single face's normal |
| `selection.normal`, `.textureRotation` | Single face normal vector and texture rotation in degrees |
| `selection.children`, `.descendants` | Direct and recursive children of one selected node |
| `counts.groups`, `.entities`, `.brushes`, `.patches`, `.faces` | Recursive totals, including selected roots, without duplicate nodes |
| `textures` | Sorted unique list of `{name}` across selected geometry |
| `textureCount`, `textureSummary` | Count and first four names (plus remaining count) |
| `entity.classname`, `.origin`, `.properties`, `.propertyList` | Single entity; properties by key or as `{key,value}` list |
| `currentTexture`, `layer`, `group`, `mode` | Current editor texture, current layer/open group names, `objects`/`faces` mode |
| `loaded` | A map is loaded (overlay is hidden otherwise) |

Face selections count only the selected faces and their distinct parent brushes.
Groups/entities include nested contents. Brushes and groups have no intrinsic
rotation value, so none is fabricated; face normals and entity angle properties
are shown when present. Texture rotations are separate from object orientation.
The default Select mode includes contextual movement/drawing; explicit modal
tools such as Clip, Rotate and Vertex have separate IDs.

New layouts, labels, conditions, entity fields and style variants need no C++
changes. To expose entirely new editor data, extend `ContextOverlayData.cpp`;
the template renderer and widget do not need to change.
