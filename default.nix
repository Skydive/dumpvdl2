{ pkgs ? import <nixpkgs> {}, libacars, system }:

let

in
  pkgs.stdenv.mkDerivation {
    name = "dumpvdl2";
    src = ./.;

    outputs = [ "out" ];
    enableParallelBuilding = true;

    nativeBuildInputs = with pkgs; [ 
      pkg-config
      cmake
    ];

    buildInputs = with pkgs; [
      zlib
      libxml2
      librtlsdr
      glib
      protobufc
      libacars.defaultPackage.${system}
    ];

    cmakeFlags = [ "-DRAW_BINARY_FORMAT=ON" ];
  }
