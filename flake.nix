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

        miru = pkgs.stdenv.mkDerivation {
          pname = "miru";
          version = "0.8.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            wayland
            wayland-protocols
            libffi
            mesa
            libGL
          ];
        };
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            wayland
            wayland-protocols
            libffi
            mesa
            libGL
          ];

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
