{
  description = "A VDL Mode 2 message decoder and protocol analyzer";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-22.11";
    flake-utils.url = "github:numtide/flake-utils";
    flake-utils.inputs.nixpkgs.follows = "nixpkgs";

    libacars.url = "github:skydive/libacars";
  };

  outputs = { self, nixpkgs, flake-utils, libacars, ... }:
    flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
      dumpvdl2 = pkgs.callPackage ./default.nix { inherit pkgs libacars; };
    in {
      defaultPackage = dumpvdl2;
      devShell = dumpvdl2;
    });
}
