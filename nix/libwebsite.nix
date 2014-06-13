{pkgs, stdenv, fetchgit, libev}:

let python = pkgs.python33;
in stdenv.mkDerivation rec {
  version="0.2.22-7-g10118cc";
  name="libwebsite-${version}";

  src = fetchgit {
    url = "git://github.com/tailhook/libwebsite";
    rev = "10118cc";
    sha256 = "b05a2c44ddd576825a433f61b9abcafbc312dffb5bc0b1af15a13905c71a20f3";
  };

  buildInputs = with pkgs; [ libev openssl ];

  configurePhase = ''
    ${python}/bin/python3 ./waf configure --prefix=$out
  '';

  buildPhase = ''
    ${python}/bin/python3 ./waf build
  '';

  installPhase = ''
    ${python}/bin/python3 ./waf install
    '';

  meta = {
    description = "An HTTP and websocket protocol implementation for libev event loop";
    homepage = "http://github.com/tailhook/libwebsite";
    license = stdenv.lib.licenses.mit;
    platforms = stdenv.lib.platforms.linux;
  };
}

