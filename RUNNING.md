# Running

You can run `miru-daemon` directly in the foreground, or set it up as a
systemd user service so it starts automatically with your graphical session.

## Running directly

````bash
# Normal mode
./build/miru-daemon
# or
grim cast run-daemon

# Debug mode
MIRU_DEBUG=1 ./build/miru-daemon
````

## As a systemd user service

Create `~/.config/systemd/user/miru.service`:

````systemd
[Unit]
Description=Miru Zooming Daemon
PartOf=graphical-session.target
After=graphical-session.target
ConditionEnvironment=WAYLAND_DISPLAY
ConditionPathExists=%h/.local/bin/miru-daemon

[Service]
ExecStart=%h/.local/bin/miru-daemon
Restart=on-failure
RestartSec=1

[Install]
WantedBy=graphical-session.target
````

This assumes `miru-daemon` has been installed to `~/.local/bin` (see
[Installing the built binaries](./INSTALL.md#installing-the-built-binaries)
above) — adjust `ExecStart`/`ConditionPathExists` if yours lives elsewhere.

Then enable and start it:

````bash
systemctl --user enable --now miru.service
````

Either way, once running, `miru-daemon` connects to the compositor, logs every
advertised protocol, opens a Unix socket at `$XDG_RUNTIME_DIR/miru.sock`, and
then idles — no overlay is shown until told to toggle. Nothing else happens
until a toggle command arrives (see [Performance](./README.md#performance) above
for what that idling actually costs).

Toggle the overlay on/off:

````bash
./build/miru-daemon --version    # prints version info + an ASCII logo, exits immediately
./build/miru-daemon --gen-config # walk through options and write $XDG_CONFIG_HOME/miru/config.toml. 
./build/miructl toggle           # freezes + zooms the screen / returns it to normal
./build/miructl loupe            # region loupe: drag a rectangle, then zoom that region only
./build/miructl spotlight        # standalone dim-around-cursor (click-through; cursor follow is a WIP)
./build/miructl quit             # tells the daemon to shut down
````

## Man pages

`miru-daemon` and `miructl` each have their own `man` page. How you access
them depends on how you installed Miru:

* **Installed via `miru-zoom-git` (AUR), `cmake --install build`, or
  `grim cast build`/`grim cast install`** — no extra step, `man miru-daemon`
  and `man miructl` work immediately.
* **Homebrew** — not wired up yet, coming soon.
* **Built from source but not installed to `$PATH`** — point `man` at the
  page directly from the repo root:

````bash
man ./miru-daemon.1
man ./miructl.1
````
