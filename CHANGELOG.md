
## 0.1.0 - 2026-07-19







### :rocket: New features

- **(zoom)** The zoom now works

- **(ipc)** Keybind-driven toggle for the overlay, replaces always-on/continuous recapture

- **(logo)** Add ansi logo to `-v` output

- **(render)** Continuous re-capture of the overlay while displayed

- **(render)** Continuous re-capture of the overlay while displayed

- **(render)** Blit captured frame into the layer surface

- **(capture)** Add one-shot screencopy frame capture

- **(surface)** Render a fullscreen layer-shell overlay surface (bug fixes)

- **(surface)** Render a fullscreen layer-shell overlay surface

- **(core)** Bootstrap Wayland connection and registry binding

- **(initial)** Initial commit






### :bug: Bug fixes

- **(surface)** Clear configured flag on reconfigure allocation failure

- **(surface)** Real double-buffering instead of a single reused wl_buffer

- **(input)** Decouple pointer/keyboard events from rendering, add missing listener stubs

- **(ipc)** Bound accepted-client read via poll instead of unbounded/2s blocking read

- **(ipc)** Address socket/dispatch review findings

- **(surface)** Pin layer surface to the same output the capture used

- **(shm)** Guard width overflow in shm_buffer_create_stride directly

- **(capture)** Address screencopy review findings

- **(functions)** Wrong function names














### :art: Styling

- **(fmt)** Add formatting rules






### :hammer: Build

- **(tag)** Removed git commit hash from version_number

- **(version_number)** Add a `--version` flag to `miru-daemon`



### :tada: New Contributors
- @Vaishnav-Sabari-Girish made their first contribution in [#9](https://github.com/Vaishnav-Sabari-Girish/miru/pull/9)
