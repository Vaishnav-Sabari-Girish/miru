
## v0.4.0 - 2026-08-05







### :rocket: New features

- **(config_hot_reload)** Add hot-reloading during config changes






### :bug: Bug fixes

- **(color)** Color inversion in MangoWM

- **(conversion)** Conversion is kept within allocated rows

- **(mangowm)** Pixel error

- **(config)** Config removal/recreation fixed

- **(config_watch)** Fix watch retry

- **(periodic)** Try re-add periodically in case the watch is removed

- **(overflow)** Overlow is now a reload trigger

- **(out-of-range)** Fixed out-of-range conditions

- **(ipc)** Harden the lockfile fallback against symlink and pre-owned-file attacks

- **(ipc)** Remove redundant, buggy connect-probe now that flock covers it

- **(ipc)** Scope the lockfile fallback by UID instead of a fixed /tmp path

- **(ipc)** Atomic single-instance lock, detect `POLLHUP/POLLERR` as fatal

- **(daemon)** Exit nonzero on lost Wayland connection, refuse second instance

- **(mangowm)** Unknown pixel error










### :recycle: Refactoring

- **(types)** Replace int-as-boolean fields with real bool across the codebase











## v0.3.0 - 2026-07-30







### :rocket: New features

- **(spotlight_mode)** Add spotlight mode

- **(config)** Add robust TOML configuration support

- **(config)** Add TOML configuration support






### :bug: Bug fixes

- **(range)** Out of range channel conversion fixed

- **(spotlight)** Fixed the lag and weird movements.

- **(toml)** Clear current_section to avoid malformed headers



















### :tada: New Contributors
- @yvnth made their first contribution
## v0.2.0 - 2026-07-26







### :rocket: New features

- **(wasd)** W/a/s/d for panning as alternatives to arrow keys

- **(continuous)** Long press  continuous panning/zooming added

- **(input)** Add continuous key presses for zoom and panning

- **(arrow)** Add arrow key support for panning

- **(nix)** Tested flake.nix

- **(logo)** Optimize logo rendering

- **(help)** Help for miructl too added

- **(help)** Add help output






### :bug: Bug fixes

- **(interval)** Rounding up interval upto 1ms

- **(input)** Make arrow panning zoom-aware and clamp viewport bounds

- **(capture)** Support ABGR8888 and XBGR8888 screencopy formats

- **(make)** You can now use `make` to also compile



















## v0.1.0 - 2026-07-19







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
- @Vaishnav-Sabari-Girish made their first contribution
