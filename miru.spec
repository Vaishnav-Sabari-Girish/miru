Name:           miru
Version:        0.14.0
Release:        1%{?dist}
Summary:        Wayland-native screen magnifier and zooming daemon

License:        MIT
URL:            https://codeberg.org/Vaishnav-Sabari-Girish/miru
Source0:        %{url}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  cmake >= 3.20
BuildRequires:  make
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-egl)
BuildRequires:  pkgconfig(wayland-scanner)
# Needs the staging protocols ext-image-capture-source and ext-foreign-toplevel-list
BuildRequires:  pkgconfig(wayland-protocols) >= 1.37
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(libnotify)

%description
Miru is a Wayland-native screen magnifier and cursor spotlight tool for
streamers and presenters. It consists of miru-daemon, which draws the zoomed
overlay, and miructl, a small client used to control it from compositor
keybinds.

Miru requires a compositor that implements wlr-layer-shell and
wlr-screencopy (for example Niri, Sway or Hyprland). GNOME (Mutter) and
KDE Plasma (KWin) are not supported.

%prep
%autosetup -n %{name}

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/miru-daemon
%{_bindir}/miructl
%{_mandir}/man1/miru-daemon.1*
%{_mandir}/man1/miructl.1*

%changelog
* Wed Oct 28 2026 Vaishnav Sabari Girish <vaishnav.sabari.girish@gmail.com> - 0.14.0-1
- Update to 0.14.0 

* Thu Oct 22 2026 Vaishnav Sabari Girish <vaishnav.sabari.girish@gmail.com> - 0.13.0-1
- Initial COPR package
