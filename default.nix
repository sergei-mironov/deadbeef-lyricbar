with import <nixpkgs> {}; {
  lyricbar = stdenv.mkDerivation {
    name = "deadbeef-lyricbar";
    src = ./.;
    buildInputs = [
      stdenv
      pkg-config
      gettext
      deadbeef
      gtkmm3
      libxmlxx3
    ];
  };
}
