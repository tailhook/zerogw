{pkgs, stdenv, fetchgit, zeromq, coyaml, libev, libwebsite, mime-types}:

let python = pkgs.python33;
    pypkg = pkgs.python33Packages;
    pyyaml = pypkg.pyyaml;
    pyzmq = pkgs.callPackage ./pyzmq.nix {
      zeromq=zeromq;
      buildPythonPackage = pypkg.buildPythonPackage;
    };
in stdenv.mkDerivation rec {
  version="0.6.2-dev";
  name="zerogw-${version}";

  srcs = fetchgit {
    url = "git://github.com/tailhook/zerogw";
    rev = "d85a0ce";
    sha256 = "372562e3d90401dae281e2a0b78eacd272681d6c14b86fcc3599689cfab1d67f";
  };

  buildInputs = with pkgs; [
    # build tools (some should be propagated ?)
    python coyaml pyyaml libev zeromq libwebsite openssl
    # for tests
    pyzmq
    ];
  propagatedBuildInputs = [ mime-types ];

  configurePhase = ''
    ${python}/bin/python3 ./waf configure --prefix=$out
  '';

  patchPhase = ''
    sed -i "s@/etc/mime.types@${mime-types}/share/mime.types@" src/config.yaml
  '';

  buildPhase = ''
    ${python}/bin/python3 ./waf build
  '';

  installPhase = ''
    ${python}/bin/python3 ./waf install
    '';

  doCheck = true;
  checkPhase = ''
    ${python}/bin/python3 -m unittest discover -s test -p "*.py" -t . -v
    '';

  meta = {
    description = "A fast HTTP/WebSocket to zeromq gateway";
    homepage = "http://zerogw.com";
    license = stdenv.lib.licenses.mit;
    platforms = stdenv.lib.platforms.linux;
  };
}

