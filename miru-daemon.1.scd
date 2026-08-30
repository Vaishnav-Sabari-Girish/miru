MIRU-DAEMON(1) "Miru" "miru-daemon Manual"

# NAME

miru-daemon - Wayland-native screen magnifier and cursor spotlight daemon

# SYNOPSIS

*miru-daemon* [options]

# DESCRIPTION

*miru-daemon* is a long-running Wayland client. It connects to the compositor,
listens on a Unix socket, and stays idle until *miructl*(1) sends a command.

On *toggle*, it captures one frame via *wlr-screencopy*, shows it in a
fullscreen *wlr-layer-shell* overlay, and lets you zoom, pan, highlight the
cursor, draw simple annotations, and open an on-screen help panel. A second
*toggle* (or *Esc* when help is closed) tears the overlay down. The underlying
desktop is not continuously re-captured while the overlay is visible.

Control is only through *miructl*(1) (*toggle* / *quit*). Global hotkeys must
be bound in the compositor to run *miructl toggle*.

# OPTIONS

*-h*, *--help*
	Print usage and exit.

*-v*, *--version*
	Print version and ASCII logo, then exit.

# COMMANDS (via miructl)

See *miructl*(1). Typical usage:

	miructl toggle
	miructl quit

# ENVIRONMENT

*MIRU_DEBUG*
	If set to a non-zero value (e.g. *1*), print verbose diagnostics (input,
	zoom, texture upload, annotate/help toggles, and similar). Unset or *0*
	for normal logging.

*WAYLAND_DISPLAY*, *XDG_RUNTIME_DIR*
	Standard Wayland / runtime directory variables. The control socket is
	*$XDG_RUNTIME_DIR/miru.sock*.

*XDG_CONFIG_HOME*
	If set, config is read from *$XDG_CONFIG_HOME/miru/config.toml*;
	otherwise *$HOME/.config/miru/config.toml*.

# CONFIGURATION

Config path:

	$XDG_CONFIG_HOME/miru/config.toml
	# or
	$HOME/.config/miru/config.toml

The directory and a default file are created on first launch. The file is
watched; most options hot-reload without restarting the daemon.

Default:

```
[zoom]
factor = 2.0
increment = 0.25
max_factor = 10.0
smooth = false

[spotlight]
radius = 250
dim = 0.65
softness = 20
animation_speed = 14.0
radius_step = 20.0

[general]
show_cursor = true
```

*[zoom]*
	*factor* — initial zoom on each toggle-on (min 1.0).  
	*increment* — zoom step for keys/scroll.  
	*max_factor* — maximum zoom.  
	*smooth* — interpolate zoom/pan when true.

*[spotlight]* (Cursor Highlight while magnifier is active, toggled with Tab)
	*radius*, *dim*, *softness* — circle size, outside darkness, edge feather.  
	*animation_speed* — ease in/out speed for Tab.  
	*radius_step* — step for Shift+Plus/Minus and Ctrl+scroll.

*[general]*
	*show_cursor* — hide the hardware cursor while the overlay is active when
	false (compositor-dependent).

Full detail:
https://codeberg.org/Vaishnav-Sabari-Girish/miru#configuration

# CONTROLS (while overlay is active)

*Mouse move*
	Pan.

*Scroll* or *+* / *-*
	Zoom.

*WASD* / *arrow keys*
	Pan (key repeat supported).

*Tab*
	Toggle Cursor Highlight.

*Shift+Plus* / *Shift+Minus* or *Ctrl+scroll*
	Adjust highlight radius.

*Shift+A*
	Toggle annotate mode (pan frozen; orange frame).  
	*W* — arrow tool; *R* — rectangle; drag LMB — place; *C* — clear.

*Shift+H* or *?*
	Toggle help panel. *Esc* closes help first if open.

*Esc* or compositor toggle keybind (*miructl toggle*)
	Exit magnifier (after closing help if needed).

# SYSTEMD

Example user unit *~/.config/systemd/user/miru.service*:

```
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
```

	systemctl --user enable --now miru.service

Adjust paths if binaries are installed elsewhere.

# KEYBINDS (compositor)

Bind a key to *miructl toggle* in your compositor (Niri, Hyprland, Sway,
Mango, etc.). The daemon must already be running.

See:
https://codeberg.org/Vaishnav-Sabari-Girish/miru#setting-up-a-keybind

# NOTES

Wayland does not expose a global pointer position while the overlay is
inactive. The zoom view prefers the last pointer position from a previous
session in the same daemon process when available.

Requires *wlr-layer-shell-unstable-v1* and *wlr-screencopy-unstable-v1*.
GNOME (Mutter) and KDE Plasma (KWin) are not supported.

# SEE ALSO

*miructl*(1)

# AUTHORS

Maintained by Vaishnav Sabari Girish <vaishnav.sabari.girish@gmail.com>.

Sources:

- https://codeberg.org/Vaishnav-Sabari-Girish/miru
- https://github.com/Vaishnav-Sabari-Girish/miru

Issues and pull requests:
https://codeberg.org/Vaishnav-Sabari-Girish/miru
