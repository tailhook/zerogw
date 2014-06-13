let
  pkgs = import <nixpkgs> { } // {
    coyaml = callPackage ./coyaml.nix { };
    libwebsite = callPackage ./libwebsite.nix { };
    mime-types = callPackage ./mime-types.nix { };
  };
  callPackage = pkgs.lib.callPackageWith pkgs;
in with pkgs; rec {
  zerogw-zeromq2 = callPackage ./zerogw.nix { zeromq=zeromq2; };
  zerogw-zeromq3 = callPackage ./zerogw.nix { zeromq=zeromq3; };
  zerogw-zeromq4 = callPackage ./zerogw.nix { zeromq=zeromq4; };
  zerogw = zerogw-zeromq2;
}
