# Setting up a keybind

You'll want this bound to a key rather than run manually. Each supported
compositor has its own way to bind a command to a key:

> [!NOTE]
> Make sure `miru-daemon` is already running before triggering the keybind,
> or `miructl` will fail with a connection error.

**Niri** — `~/.config/niri/config.kdl`:

````kdl
Mod+Z hotkey-overlay-title="toggle miru" { spawn-sh "/path/to/miru/build/miructl toggle"; }
Mod+Alt+Z hotkey-overlay-title="toggle miru loupe" { spawn-sh "/path/to/miru/build/miructl loupe"; }
````

**Hyprland** — `~/.config/hypr/hyprland.lua`:

````lua
hl.bind("SUPER + Z", hl.dsp.exec_cmd("/path/to/miru/build/miructl toggle"))
hl.bind("SUPER + ALT + Z", hl.dsp.exec_cmd("/path/to/miru/build/miructl loupe"))
````

**Sway** — `~/.config/sway/config`:

````config
bindsym $mod+z exec /path/to/miru/build/miructl toggle
bindsym $mod+alt+z exec /path/to/miru/build/miructl loupe
````

**Mango** — `~/.config/mango/config.conf`:

````conf
bind=SUPER,Z,spawn,/path/to/miru/build/miructl toggle
bind=SUPER,ALT,Z,spawn,/path/to/miru/build/miructl loupe
````

**Nauka** — `~/.config/nauka/nauka.con`:

````conf
keybind super z run "/path/to/miru/build/miructl toggle"
keybind super alt z run "/path/to/miru/build/miructl loupe"
````

**SomeWM** - `~/.config/somewm/rc.lua`

````lua
awful.spawn.once("miru-daemon")
awful.key({ modkey }, "z", function()
  awful.spawn("/path/to/miru/build/miructl toggle")
end),

awful.key({ modkey, "Mod1" }, "z", function()
  awful.spawn("/path/to/miru/build/miructl loupe")
end),
````

**Umbriel** - `~/.config/umbriel/config.toml`

````toml
[keybinds]
"Mod+Z" = "spawn:/path/to/miru/build/miructl toggle"
"Mod+Alt+Z" = "spawn:/path/to/miru/build/miructl loupe"
"Mod+Shift+Z" = "spawn:/path/to/miru/build/miructl spotlight"
````

Substitute the actual path to your built `miructl` binary in each case (or
wherever it ends up if installed via a package manager).

On toggle-on, the daemon captures one frame via `wlr-screencopy`, uploads it
as an OpenGL ES texture, and shows it in a fullscreen `wlr-layer-shell`
overlay (correctly scaled on HiDPI outputs) at the configured zoom factor,
centered on your cursor when possible. While active:

* **Move the mouse** to pan the zoomed view
* **`+`/`-`** or **scroll wheel** to adjust the zoom level
* **Shift+Plus / Shift+Minus** or **Ctrl+scroll** to adjust Cursor Highlight
  radius (while highlight is relevant / available)
* **Arrow keys or WASD** to pan by keyboard — press and hold for continuous
  panning at your keyboard's repeat rate
* **Tab** to toggle Cursor Highlight on/off — darkens everything except a
  soft-edged circle that follows the real pointer position across the
  screen (absolute tracking), using the `[spotlight]` config values. This
  is separate from the standalone Spotlight mode described above; see
  [What it does](./README.md#what-it-does) for the distinction.
* **Shift+A** to toggle **Annotate mode** (orange frame, pan frozen):
  * **W** — arrow tool
  * **R** — rectangle tool
  * **L** — line tool
  * **O** — ellipse tool (Shift + drag for circle)
  * **P** — freehand draw tool (Click and drag to draw)
  * **Left-click drag** — place the current tool on the frozen frame
  * **T** - text tool
  * **Click + type** - start typing text from the clicked location (indicated by
    a `_`)
  * **C** — clear all annotations
  * **Shift+A** again — leave annotate mode (shapes remain until overlay exit
    or clear)
* **B**: Toggle the status badge.
* **F**: Cycle between image filters (default, grayscale, inverted/negative,
  high contrast).
* **Shift+H** or **?** — toggle the **help panel** (keybind cheat sheet).
  **Esc** closes help if it is open; otherwise Esc exits the magnifier.
* **Esc**, or pressing the toggle keybind again, to exit back to your normal
  desktop

There's deliberately no continuous re-capture of the underlying screen while
the overlay is visible: an earlier version tried that and hit a feedback loop
where the overlay could end up capturing itself (e.g. during Alt+Tab), so the
frozen frame is captured once per toggle-on, matching `boomer`'s actual
freeze-on-demand behavior rather than a live feed. Zooming/panning/Cursor
Highlight/annotations within that one frozen frame is fully live, however.

The overlay grabs keyboard and pointer input while active (needed for
pan/zoom/Cursor Highlight/annotate to work), so clicks and most keys won't
reach whatever's underneath until you exit; that's expected for Magnifier
mode. A standalone Spotlight mode would behave differently —
click-through by design, see [What it does](./README.md#what-it-does) above.

> [!NOTE]
> **Cursor position on open:** Wayland does not expose a global pointer
> position while Miru is inactive. The view starts at the last pointer
> position from a previous overlay session in the same daemon process when
> available; otherwise it may open near the center until the first
> pointer-enter/motion. Running `miru-daemon` as a long-lived service helps
> retain that last position across toggles, but does not track the cursor
> while the overlay is off.
