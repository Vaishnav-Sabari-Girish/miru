# Requirements

* A Wayland compositor implementing `wlr-layer-shell-unstable-v1` and
  `wlr-screencopy-unstable-v1`
* `wayland-client`, `wayland-protocols`, `wayland-scanner` (pacman: `wayland`,
  `wayland-protocols`)
* EGL + OpenGL ES 2 development packages
* CMake ≥ 3.20, Ninja (optional)
* A C11 compiler

## Compositor compatibility

Miru currently requires a compositor that exposes both `wlr-layer-shell` and
`wlr-screencopy`.

* **Niri** — supported and used for development/testing
* **Sway** — supported; both required protocols are core to the wlroots
  ecosystem Sway is built on
* **Hyprland** — supported by the required wlroots protocols
* **Mango** — supported if the required protocols are exposed
* **Nauka** — supported and tested by
  [@shadowash8](https://github.com/shadowash8)
  (<https://github.com/shadowash8/nauka>)
* **SomeWM** - supported and tested by [@yvnth](https://github.com/yvnth)
  [<https://somewm.org/>]
* **Umbriel** - supported (<https://github.com/noctalia-dev/umbriel>)
* **GNOME / Mutter** — not supported
* **KDE Plasma / KWin** — not supported

Support for compositors without these protocols may be added later through
alternative capture and overlay mechanisms.

## Installing

### Arch Linux (AUR)

````bash
# latest tagged release
paru -S miru-zoom
# or track the latest commit on main
paru -S miru-zoom-git

````

Substitute your AUR helper of choice — `yay`, `paru`, or a manual
`makepkg -si` against the PKGBUILD.

### Nix / NixOS

Miru is packaged in
[nixpkgs](https://search.nixos.org/packages?channel=unstable&query=miru&show=miru)
(attribute `miru`). Prefer that over the project flake when you want a normal
channel/package install.

> [!NOTE]
> The nixpkgs package is maintained by
> [@yvnth](https://github.com/yvnth) — thank you!
> Releases in nixpkgs can lag behind upstream (Codeberg/GitHub tags). For the
> absolute latest commit, build from source or use the development flake below.

**Run without installing** (unstable channel):

````bash
nix shell nixpkgs/nixos-unstable#miru -c miru-daemon
# control client from the same package:
nix shell nixpkgs/nixos-unstable#miru -c miructl toggle
````

Classic `nix-shell`:

````bash
nix-shell -p miru -I nixpkgs=channel:nixos-unstable --run miru-daemon
````

**Install to your user profile:**

````bash
nix profile install nixpkgs/nixos-unstable#miru
# or: nix-env -iA nixpkgs.miru -f channel:nixos-unstable
````

**NixOS** — if your system follows unstable:

````nix
environment.systemPackages = with pkgs; [
  miru
];
````

On stable, pull only this package from unstable:

````nix
{ config, pkgs, ... }:
let
  unstable = import <nixos-unstable> { config = config.nixpkgs.config; };
in
{
  environment.systemPackages = [ unstable.miru ];
}
````

````bash
sudo nix-channel --add https://nixos.org/channels/nixos-unstable nixos-unstable
sudo nix-channel --update
````

**Development / bleeding edge** (optional project flake):

````bash
nix run git+https://codeberg.org/Vaishnav-Sabari-Girish/miru
nix develop git+https://codeberg.org/Vaishnav-Sabari-Girish/miru
````

### Homebrew (Linuxbrew)

````bash
brew tap Vaishnav-Sabari-Girish/taps https://codeberg.org/Vaishnav-Sabari-Girish/homebrew-taps
brew install Vaishnav-Sabari-Girish/taps/miru
````

### Fedora (COPR)

````bash
sudo dnf copr enable vaishnavs/miru
sudo dnf install miru
````

> **Note**
>
> The package does not ship a systemd service. After installing, start
> `miru-daemon` from your compositor config or set up your own user service
> (see [As a systemd user service](./RUNNING.md#as-a-systemd-user-service)).
> Fedora's default desktop (GNOME) is not supported, so use a compositor
> that provides `wlr-layer-shell` and `wlr-screencopy`, such as Sway or Niri.
>
> **Important**
>
> The COPR package has only been tested in a Fedora container (`distrobox`),
> not on a full Fedora installation. If you run into any problems, please
> [open an issue](https://codeberg.org/Vaishnav-Sabari-Girish/miru/issues).

### Gentoo (third-party overlay)

Miru is available for Gentoo through
[`acooliodude-overlay`](https://codeberg.org/ACoolioDude/acooliodude-overlay)
(package `gui-apps/miru`).

> [!NOTE]
> The overlay and ebuild are maintained by
> [@ACoolioDude](https://codeberg.org/ACoolioDude) — thank you! The ebuild
> lives in the overlay, not in the official Gentoo tree or Guru.

1. Install `eselect-repository`:

````bash
sudo emerge --ask app-eselect/eselect-repository
````

1. Enable the overlay:

````bash
sudo eselect repository add acooliodude-overlay git https://codeberg.org/ACoolioDude/acooliodude-overlay
````

1. Sync the overlay (either command works):

````bash
sudo emerge --sync acooliodude-overlay
# or
sudo emaint sync -r acooliodude-overlay
````

1. Install Miru:

````bash
sudo emerge --ask gui-apps/miru
````

The package is a live ebuild, so to follow new commits pushed to `main` you
have to [unmask the
package](https://wiki.gentoo.org/wiki/Knowledge_Base:Unmasking_a_package) and
then update it:

````bash
sudo emerge --ask --update gui-apps/miru
````

> [!IMPORTANT]
> The ebuild does **not** ship an OpenRC or systemd service. After installing,
> add `miru-daemon` to your autostart yourself, depending on the Wayland
> compositor you use (see the compositor examples under
> [Setting up a keybind](./KEYBINDS.md#setting-up-a-keybind), or
> [As a systemd user service](./RUNNING.md#as-a-systemd-user-service) if you run
> systemd and want to write your own unit).

### From source

#### Building

````bash
# Using Ninja
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Using Make
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
````

Then run:

````bash
cmake --build build
````

Or with [Grimoire](https://github.com/Vaishnav-Sabari-Girish/grimoire):

````bash
grim cast build # Uses make to build by default
````

This builds two binaries: `miru-daemon` (the actual Wayland client) and
`miructl` (a tiny, Wayland-independent socket client used to control it).

#### Installing the built binaries

To install `miru-daemon` and `miructl` to `~/.local/bin`:

````bash
cmake --install build
````

Or with Grimoire, which also configures `CMAKE_INSTALL_PREFIX` for you:

````bash
grim cast install
````
