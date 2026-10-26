# Configuration

Miru uses a TOML configuration file located at:

````text
$XDG_CONFIG_HOME/miru/config.toml
````

If `XDG_CONFIG_HOME` is not set, Miru follows the XDG fallback and uses:

````text
$HOME/.config/miru/config.toml
````

The directory and default configuration file are created automatically on
first launch.

The default configuration is:

````toml
[zoom]
factor = 2.0
increment = 0.25
max_factor = 10.0
smooth = false
upscale = "bicubic"

[spotlight]
radius = 250
dim = 0.65
softness = 20
animation_speed = 14.0
radius_step = 20.0

[general]
show_cursor = true
status_badge = true

[annotate]
default = "rect"
thickness = 4.0
color = "ff3333"
text_color = "ffd933"
text_size = 1.0

[loupe]
shape = "rect"

[export]
format = "png"

[filter]
default = "none"
````

The currently active options are:

* `zoom.factor` — initial zoom level applied on each toggle-on. Must be at
  least `1.0`.
* `zoom.increment` — amount the zoom changes per key/scroll input. Must be
  greater than `0`.
* `zoom.max_factor` — maximum zoom level. Must be at least `1.0`.
* `zoom.smooth` — when `true`, zoom level and pan position are smoothly
  interpolated toward their targets instead of snapping.
* `zoom.upscale` - how the frozen capture is re-sampled when zoomed:
  * `"nearest"` - sharp, block (GL nearest)
  * `"linear"` - bilinear (default hardware filter)
  * `"bicubic"` - Catmull-Rom bicubic in the fragment shader (default)
* `spotlight.radius` — radius, in pixels, of the fully-bright circle around
  the cursor.
* `spotlight.dim` — how much darker the dimmed area gets, from `0.0` (no
  effect) to `1.0` (fully black).
* `spotlight.softness` — width, in pixels, of the feathered transition
  between the bright circle and the dimmed area.
* `spotlight.animation_speed` — how quickly Cursor Highlight radius/dim ease
  in and out when toggling Tab. Higher is faster.
* `spotlight.radius_step` — step size when adjusting the highlight radius
  with Shift+Plus/Minus or Ctrl+scroll.
* `general.show_cursor` — when `false`, hides the hardware cursor while the
  overlay is active (restoring a themed cursor when turning it back on may
  be limited depending on the compositor).
* `general.status_badge` - shows a status badge which displays the zoom factor
  and in annotate mode, it shows the annotation tool chosen. Works even in
  `loupe` mode.
* `annotate.default` - default tool when entering annotate mode:
  * `"arrow", "rect/rectangle", "line", "ellipse", or "text"`
* `annotate.thickness` - stroke width for shapes (value ranges from `1` to `8`)
* `annotate.color` - has RGB hex for shapes (default value is `ff3333`)
* `annotate.text_color` - has RGB hex for text (default value is `ffd933`)
* `annotate.text_size` - scale for annotation text (`0.25` - `8.0`; default is
  `1.0`)
* `loupe.shape` - the shape to use when in `loupe` mode:
  * `"rect"/"rectangle", "ellipse", "circle"`
* `export.format` - File format for exporting
  * `"png", "bmp"`
* `filter.default` - the default filter to add when zooming:
  * `"none", "invert/inverted/negative", "contrast/high-contrast", "gray/grey/grayscale"`

Invalid numeric values, including malformed, overflowing, non-finite, and
non-positive values where applicable, fall back to safe defaults. `zoom.factor`
is clamped to `zoom.max_factor` when necessary.

> [!NOTE]
> `[spotlight]` values are live — they control the Cursor Highlight effect
> toggled with Tab while Magnifier mode is active. They're named `[spotlight]`
> in the config because they'll be shared with standalone Spotlight mode once
> that's built, not because Cursor Highlight and Spotlight mode are the same
> feature.

The config file is watched while `miru-daemon` is running — saving changes
takes effect immediately, no restart needed. `zoom.max_factor`, `zoom.smooth`
and every `[spotlight]` value update live, including on an already-active
overlay; `zoom.factor` (the *initial* zoom on toggle-on) takes effect starting
with the next toggle, since retroactively snapping an in-progress session to a
different zoom level would be jarring rather than useful.

`[annotate]` changes apply to **new** strokes after reload (or the next
toggle-on). Existing annotations keep the color/size they were drawn with.

Additional input, zoom and texture-upload diagnostics can be enabled by
setting `MIRU_DEBUG` to a non-zero value:

````bash
MIRU_DEBUG=1 ./build/miru-daemon
````
