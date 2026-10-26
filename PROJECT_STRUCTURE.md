# Project structure

````text
.
├── CMakeLists.txt
├── cmake/
│   └── WaylandScanner.cmake          # wraps wayland-scanner as CMake custom commands
├── protocol/                         # vendored protocol XML (not shipped by wayland-protocols)
│   ├── wlr-layer-shell-unstable-v1.xml
│   └── wlr-screencopy-unstable-v1.xml
├── src/
│   ├── main.c                        # daemon entrypoint, IPC-driven toggle loop
│   ├── wayland_state.h/.c            # connection, registry, seat/output tracking, poll-based event loop
│   ├── layer_surface.h/.c            # wlr-layer-shell overlay, zoom/pan, Cursor Highlight, GL draw
│   ├── annotations.h/.c              # shape annotation state (arrow/rect), screen↔buffer mapping
│   ├── help.h/.c                     # help panel copy (keybind list)
│   ├── font8x8_basic.h               # public-domain 8x8 bitmap font for the help panel
│   ├── capture.h/.c                  # one-shot screen capture via wlr-screencopy
│   ├── shm_buffer.h/.c               # shared-memory pixel buffer allocation helper
│   ├── egl_context.h/.c              # EGL display / context / window-surface setup
│   ├── gl_renderer.h/.c              # OpenGL ES 2 shaders, texture upload, spotlight, annotations, help
│   ├── ipc_server.h/.c               # Unix socket server, parses toggle/loupe/quit commands
│   ├── input.h/.c                    # pointer/keyboard: pan, zoom, Tab, annotate, help, key-repeat, Esc
│   ├── config.h/.c                   # config discovery, defaults, validation and loading
│   ├── config_watch.h/.c             # inotify-based watch on the config directory, drives hot-reload
│   ├── toml.h/.c                     # minimal TOML parser used by the config loader
│   ├── version.h.in                  # CMake-configured version string (git describe)
│   ├── logo.h                        # ASCII logo module interface
│   └── logo.c                        # ASCII logo data and printing implementation
├── ctl/
│   └── miructl.c                     # thin socket client, no Wayland dependency
└── Grimoire.toml                     # dev task runner (build/run/install/clean)
````
