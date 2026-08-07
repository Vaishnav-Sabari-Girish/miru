MIRU-DAEMON(1) "miru-daemon Manual" "Miru" 

# NAME

_miru-daemon_ - Wayland-native zooming daemon

# USAGE

To use _miru-daemon_, you need to use _miructl_(1). 

miructl exposes the toggle/quit commands which toggle the daemon to take a screenshot of the current screen state and overlay it over the current screen and quit to quit the zoom mode.

You can also put the daemon in you systemctl service files to make it run on startup.


# Environment Variables 

_miru-daemon_ allows the usage of one environment variable *MIRU_DEBUG* which is to be set 0 or 1 to use _miru-daemon_ in debug mode or in normal mode. 

*MIRU_DEBUG=1* gives more detailed and verbose logs and *MIRU_DEBUG=0* gives the normal logs.


# Config file

_miru-daemon_ has a config file to configure _miru-daemon_ as you see fit. 

The config file is present at $XDG_CONFIG_HOME/miru/config.toml. 

In case $XDG_CONFIG_HOME is not set, it is present at $HOME/.config/miru/config.toml

See:
https://codeberg.org/Vaishnav-Sabari-Girish/miru#configuration


# Setting up a keybind

_miru-daemon_ can be toggled using a compositor-level keybind (Niri, Hyprland, MangoWM etc) and pressing that keybind should rum miructl toggle

See:
https://codeberg.org/Vaishnav-Sabari-Girish/miru#setting-up-a-keybind

# See also 

_miructl_(1)

# AUTHORS 

Maintained by Vaishnav-Sabari-Girish <vaishnav.sabari.girish@gmail.com>. Up-to-date sources can be found at:
- https://codeberg.org/Vaishnav-Sabari-Girish/miru
- https://github.com/Vaishnav-Sabari-Girish/miru. 
Please submit any issues and PR's to 
https://codeberg.org/Vaishnav-Sabari-Girish/miru.
