{
  description = "A VDL Mode 2 message decoder and protocol analyzer";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flake-utils.inputs.nixpkgs.follows = "nixpkgs";

    libacars.url = "github:skydive/libacars";
    libacars.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, flake-utils, libacars, ... }:
    flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };

    in {
      defaultPackage = pkgs.callPackage ./default.nix { inherit pkgs libacars; };
      devShell = pkgs.mkShell {
        name = "dumpvdl2-shell";
        buildInputs = with pkgs; [
          pkg-config
          zlib
          cmake
          libxml2
          librtlsdr
          glib
          libacars.defaultPackage.${system}
        ];

        propagatedBuildInputs = [
        ];
      };
    });
}
