# SVG icon loading and cache

`IO::VectorIcon` is the shared entry point behind `loadSVGIcon` and SVG requests
through `loadPixmapResource`. Its QIcon engine keeps the parsed SVG source and
renders at the physical pixel size required by the widget or painter. Widget
sizes remain logical pixels. No fixed set of 1x/2x bitmaps, extra sharpening or
brightness normalization is applied.

## Ownership and limits

- `IconCache` holds at most 256 recently used icons (including failed requests).
  Its capacity is adjustable with `setMaxEntries`; zero disables source caching.
- Icons returned to widgets are values with shared ownership. Eviction and
  `resetCache()` cannot invalidate them. A held icon retains its parsed snapshot;
  reset affects subsequent loads, including retries of missing/edited files.
- Qt's bounded `QPixmapCache` owns raster results. Keys include source identity,
  pixel dimensions, device pixel ratio, mode, checked state, application palette
  and style. This cache's existing global memory limit is respected, not enlarged.
- Paths are normalized lexically; resource lookups and parsing occur only on
  source-cache misses. All cache and rendering operations use the GUI thread.
- On/off variants represent checked state. Disabled/selected appearances use the
  current Qt style. Missing variants fall back to the base SVG or surviving state.

A small source cache is worthwhile because tree delegates and toolbars request
the same assets repeatedly. Using Qt's raster cache avoids a second unbounded
collection of images at every size and density.

## Validation

`common/test/src/IO/tst_VectorIcon.cpp` provides the `[VectorIcon]` tests and an
opt-in `[VectorIconBenchmark]` timing comparison. Pixel comparisons render sizes
12–96 at densities 1, 1.25, 1.5, 2 and 3 against direct QSvgRenderer output. Other
checks cover aspect ratio, original colors/alpha, painter transforms, state
fallback, palette changes, cache eviction/reset, detached QIcons, invalid files,
real application icons, and bitmap callers.

In a local macOS ARM64 Release run (Qt 5.15.18), loading and painting a 24-logical-
pixel test icon at 2x took approximately 44 microseconds without reuse and 3.2
microseconds with warm caches (about 14x faster). The comparison includes parsing,
resource/variant lookup, rasterization and composition; it is a small isolated
measurement, not an application FPS benchmark. Source caching was reset before
each cold iteration; each new source identity also causes a raster-cache miss.
