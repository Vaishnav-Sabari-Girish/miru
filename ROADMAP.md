# Roadmap

* [x] Wayland connection, registry discovery, manual poll-based event loop
* [x] Fullscreen `wlr-layer-shell` overlay surface
* [x] Screen capture via `wlr-screencopy`
* [x] Render the captured frame into the overlay (OpenGL ES + EGL, scale-aware)
* [x] `miructl` control client + Unix socket IPC, daemon/client split
* [x] Keybind-driven toggle: capture + show on activate, tear down on
  deactivate, no continuous re-capture while visible
* [x] Magnifier mode: cursor-centered zoom + live pan, mouse/keyboard/WASD/
  scroll zoom controls, proper multi-key repeat
* [x] TOML configuration with XDG config directory support and configurable
  zoom/spotlight behavior
* [x] Cursor Highlight (Tab): darken + feathered cursor cutout that follows
  the real pointer position (absolute tracking) even while zoomed
* [x] Animated Cursor Highlight entry/exit; live radius adjust (Shift+/- /
  Ctrl+scroll); `animation_speed` / `radius_step` config
* [x] Shape annotations on the frozen frame (Shift+A: arrow / rectangle / line /
      ellipse / circle)
* [x] In-overlay help panel (Shift+H / ?)
* [x] systemd user service + `cmake --install`/`grim cast install` support
* [x] Hot-reloading of the config while `miru-daemon` is running
* [x] `man` pages for `miru-daemon` and `miructl`
* [x] Optional smooth interpolation for zoom/pan (`zoom.smooth`)
* [x] Text annotations (typed labels on the frozen frame)
* [x] Loupe mode (`miructl loupe`): region select, fixed-size window,
      cursor follow, content zoom inside the selection. Shape is configurable.
* [x] Frame refresh
* [x] Configurable capture upscale (`zoom.upscale` `linear/nearest/bicubic`)
* [x] Export current view to an image making it easier to share to others or add
      to presentations (Check out [`test_export.png`](./test_export.png) and
      [`test_export.bmp`](./test_export.bmp))
* [x] Configurable annotation defaults (`[annotate]` in `config.toml`)
* [x] Configurable export format
* [x] Image filters
* [x] Add a Status badge
* [x] Multi-monitor support
      [#136](https://codeberg.org/Vaishnav-Sabari-Girish/miru/pulls/136)
      (focused output) — thank you [@hosaka](https://codeberg.org/hosaka)
* [ ] 🚧 Spotlight mode: standalone, click-through overlay (no Magnifier
  freeze, works alongside normal desktop use)
* [ ] Cursor tracking for Spotlight mode without stealing input (likely
  Niri IPC or similar)
* [ ] Support compositors without `wlr-screencopy` / `wlr-layer-shell`

> [!NOTE]
> Multi-monitor support has only been tested on **niri**. I cannot verify other
> compositors myself — please open an
> [issue](https://codeberg.org/Vaishnav-Sabari-Girish/miru/issues/new) if it
> misbehaves on yours.
> This is a demo of Multi-monitor support <https://youtu.be/DDdnZMdoj84>
