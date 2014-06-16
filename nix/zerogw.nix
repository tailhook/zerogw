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

  src = fetchgit {
    url = "git://github.com/tailhook/zerogw";
    rev = "4e50525";
    sha256 = "d9ae9fc54def7478070977cae0fbf5898a91d9b90eec4d52390cd90c8f0df65a";
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

