MIRUCTL(1) "Miru" "miructl Manual"

# NAME

miructl - control utility for miru-daemon(1)

# SYNOPSIS

*miructl* <command>

# DESCRIPTION

*miructl* is a small, Wayland-independent client. It connects to the Unix
socket created by *miru-daemon*(1) (*$XDG_RUNTIME_DIR/miru.sock*) and sends a
single command.

The daemon must already be running; otherwise *miructl* fails with a
connection error.

# COMMANDS

*toggle*
	Show or hide the magnifier overlay. On first toggle the daemon captures
	one frame and displays the zoomed view; a second toggle tears it down
	(same as exiting with *Esc* when the in-overlay help panel is closed).

*quit*
	Ask *miru-daemon* to shut down.

# OPTIONS

*-h*, *--help*
	Print usage and exit.

*-v*, *--version*
	Print version information and exit (if supported by your build).

# ENVIRONMENT

*XDG_RUNTIME_DIR*
	Directory containing *miru.sock* (required for a normal session).

# EXAMPLES

	# start the daemon (once per session), then:
	miructl toggle
	miructl quit

# SETTING UP A KEYBIND

Bind a compositor key to *miructl toggle* (Niri, Hyprland, Sway, Mango, etc.).
Ensure *miru-daemon* is already running (e.g. as a systemd user service).

Examples and paths:
https://codeberg.org/Vaishnav-Sabari-Girish/miru#setting-up-a-keybind

# SEE ALSO

*miru-daemon*(1)

# AUTHORS

Maintained by Vaishnav Sabari Girish <vaishnav.sabari.girish@gmail.com>.

Sources:

- https://codeberg.org/Vaishnav-Sabari-Girish/miru
- https://github.com/Vaishnav-Sabari-Girish/miru

Issues and pull requests:
https://codeberg.org/Vaishnav-Sabari-Girish/miru
