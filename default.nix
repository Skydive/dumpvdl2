{ pkgs ? import <nixpkgs> {}, libacars }:

let
  version = "";
in
  pkgs.stdenv.mkDerivation {
    name = "dumpvdl2-${version}";
    src = ./.;

    outputs = [ "out" ];
    enableParallelBuilding = true;

    buildInputs = with pkgs; [ 
      pkgconfig
      cmake
      libacars
    ];

    propagatedBuildInputs = with pkgs; [
      zlib
      libxml2
    ];


    # preInstallPhases = preInstallPhases ++ ''
      
      # rm libacars/libacars-2.pc
      # rm $out/lib/pkgconfig/
    # '';
  }