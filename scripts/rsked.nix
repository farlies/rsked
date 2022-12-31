{ pkgs ? import <nixpkgs> {} # import nixpkgs set
}:
pkgs.mkShell {  # helper function
  name="rsked-dev-environment";
  buildInputs = [
     pkgs.git
     pkgs.cmake
     pkgs.gcc
     pkgs.meson
     pkgs.ninja
     pkgs.libpulseaudio
     pkgs.libao
     pkgs.boost
     pkgs.libgpiod
     pkgs.libusb1
     pkgs.libmpdclient
  ];
  shellHook = ''
    echo "start developing rsked..."
  '';
}
