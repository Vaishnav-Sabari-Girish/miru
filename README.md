# Miru

A Wayland-native screen magnifier and cursor spotlight tool for streamers, built
for [Niri](https://github.com/YaLTeR/niri).

Inspired by [boomer](https://github.com/tsoding/boomer), but for Wayland —
written in C, keybind-driven, no GUI, no mouse-required config.

> **Status: early development.** Core Wayland plumbing (connection, registry,
> event loop) works. Rendering, magnifier mode, and spotlight mode are not built
> yet. See [Roadmap](#roadmap).

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

Run the daemon in the foreground (nothing renders yet, it just logs the
compositor's advertised protocols and idles):

```bash
./build/miru-daemon

# Or 

grim cast run-daemon
```

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
│   ├── main.c                 # daemon entrypoint, Wayland connection + event loop
│   └── wayland_state.h        # shared connection/registry state
└── Grimoire.toml              # dev task runner (build/run/install/clean)
```

## Roadmap

- [x] Wayland connection, registry discovery, manual poll-based event loop
- [ ] Fullscreen `wlr-layer-shell` overlay surface (solid color, no capture yet)
- [ ] Screen capture via `wlr-screencopy` → render as a texture
- [ ] Magnifier mode: zoom + pan around cursor, keybind toggle
- [ ] `miructl` control client + Unix socket IPC, daemon/client split
- [ ] Spotlight mode: darken + feathered cursor cutout, click-through
- [ ] Cursor tracking for spotlight mode without stealing input (Niri IPC)
- [ ] Multi-monitor support, config file, smooth zoom animation

## License

See [LICENSE](./LICENSE).
