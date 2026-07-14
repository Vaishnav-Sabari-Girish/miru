# Miru

A Wayland-native screen magnifier and cursor spotlight tool for streamers, built
for [Niri](https://github.com/YaLTeR/niri). (Tested on `niri` so unless tested
in other WM's I cannot guarantee it'll work)

Inspired by [boomer](https://github.com/tsoding/boomer), but for Wayland —
written in C, keybind-driven, no GUI, no mouse-required config.

> **Status: early development.** Wayland connection, registry discovery, a
> fullscreen `wlr-layer-shell` overlay surface, and one-shot screen capture via
> `wlr-screencopy` all work. The capture isn't rendered into the overlay yet,
> and magnifier/spotlight mode aren't built. See [Roadmap](#roadmap).

## What it does (planned)

Two toggleable modes, each bound to a Niri keybind:

- **Magnifier mode** — press a key, the screen freezes into a zoomed-in
  fullscreen view centered on your cursor. Move the mouse to pan, scroll or
  press keys to adjust zoom, press again (or Esc) to exit. Like `boomer`, but
  native Wayland.
- **Spotlight mode** — a click-through overlay that darkens the whole screen
  except a soft-edged circle following your cursor, while you keep working
  normally underneath. Useful for drawing viewer attention during
  streams/recordings.

## Why

Most screen magnifiers either don't exist for Wayland, or route through XWayland
with visible artifacts and no compositor integration. Miru talks to Niri's own
protocols directly (`wlr-layer-shell`, `wlr-screencopy`) instead.

## Requirements

- A Wayland compositor implementing `wlr-layer-shell-unstable-v1` and
  `wlr-screencopy-unstable-v1` (developed against Niri)
- `wayland-client`, `wayland-protocols`, `wayland-scanner` (pacman: `wayland`,
  `wayland-protocols`)
- CMake ≥ 3.20, Ninja
- A C11 compiler

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Or with [Grimoire](https://github.com/Vaishnav-Sabari-Girish/grimoire):

```bash
grim cast build
```

Run the daemon in the foreground:

```bash
./build/miru-daemon
# or
grim cast run-daemon
```

Right now this connects to the compositor, logs every advertised protocol,
captures one still frame of your screen via `wlr-screencopy` (logged with its
dimensions/stride/format), and covers the entire screen (including panels/bars)
with a translucent grey overlay on the `wlr-layer-shell` overlay layer — proof
the connection, capture, and surface pipelines all work independently. The
captured frame isn't drawn anywhere yet; the overlay is still a flat color.
The overlay also currently blocks clicks to whatever's underneath (keyboard
input still passes through); that click-through gap is expected at this stage
and gets addressed once Spotlight mode sets an explicit empty input region.
Ctrl+C to exit and restore your screen.

## Project structure

```text
.
├── CMakeLists.txt
├── cmake/
│   └── WaylandScanner.cmake   # wraps wayland-scanner as CMake custom commands
├── protocol/                  # vendored protocol XML (not shipped by wayland-protocols)
│   ├── wlr-layer-shell-unstable-v1.xml
│   └── wlr-screencopy-unstable-v1.xml
├── src/
│   ├── main.c                 # thin entrypoint, wires the modules together
│   ├── wayland_state.h/.c     # connection, registry, poll-based event loop
│   ├── layer_surface.h/.c     # fullscreen wlr-layer-shell overlay surface
│   ├── capture.h/.c           # one-shot screen capture via wlr-screencopy
│   └── shm_buffer.h/.c        # shared-memory pixel buffer allocation helper
└── Grimoire.toml              # dev task runner (build/run/install/clean)
```

## Roadmap

- [x] Wayland connection, registry discovery, manual poll-based event loop
- [x] Fullscreen `wlr-layer-shell` overlay surface (solid color, no capture yet)
- [x] Screen capture via `wlr-screencopy` (verified working, not yet rendered)
- [ ] Render the captured frame into the overlay surface
- [ ] Magnifier mode: zoom + pan around cursor, keybind toggle
- [ ] `miructl` control client + Unix socket IPC, daemon/client split
- [ ] Spotlight mode: darken + feathered cursor cutout, click-through
- [ ] Cursor tracking for spotlight mode without stealing input (Niri IPC)
- [ ] Multi-monitor support, config file, smooth zoom animation

## License

See [LICENSE](./LICENSE).
