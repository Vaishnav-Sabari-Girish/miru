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

        # Define the derivation once so we can reference it in 'packages' and 'apps'
        miru = pkgs.stdenv.mkDerivation {
          pname = "miru";
          version = "0.2.0";
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
          ];

          shellHook = ''
            echo Entering Nix Dev Shell
          '';
        };

        packages.default = miru;

        # Defines default binary for 'nix run'
        apps = {
          default = {
            type = "app";
            program = "${miru}/bin/miru-daemon";
          };
          
          # Optional: allows running 'nix run .#miructl -- toggle'
          miructl = {
            type = "app";
            program = "${miru}/bin/miructl";
          };
        };
      }
    );
}
