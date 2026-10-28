{
  description = "A zooming daemon for wayland";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        nativeDeps = with pkgs; [
          cmake
          ninja
          pkg-config
          wayland-scanner
        ];

        runtimeDeps = with pkgs; [
          wayland
          wayland-protocols
          libffi
          mesa
          libglvnd      # egl.pc, glesv2.pc
          libpng
          libnotify
          glib
          gdk-pixbuf
        ];

        miru = pkgs.stdenv.mkDerivation {
          pname = "miru";
          version = "0.14.0";
          src = ./.;

          nativeBuildInputs = nativeDeps;
          buildInputs = runtimeDeps;

          meta = with pkgs.lib; {
            description = "A zooming daemon for wayland";
            platforms = platforms.linux;
            mainProgram = "miru-daemon";
          };
        };
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = nativeDeps;
          buildInputs = runtimeDeps;
          shellHook = ''
            echo "Entering Nix Dev Shell for Miru"
          '';
        };

        packages.default = miru;

        apps = {
          default = {
            type = "app";
            program = "${miru}/bin/miru-daemon";
          };
          miructl = {
            type = "app";
            program = "${miru}/bin/miructl";
          };
        };
      }
    );
}
