{ pkgs ? import <nixpkgs> {}, libacars, system }:

let

in
  pkgs.stdenv.mkDerivation {
    name = "dumpvdl2";
    src = ./.;

    outputs = [ "out" ];
    enableParallelBuilding = true;

    buildInputs = with pkgs; [ 
      pkg-config
      cmake
      zlib
      libxml2
      librtlsdr
      glib
      protobuf
      libacars.defaultPackage.${system}
    ];
  }
