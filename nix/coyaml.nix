{pkgs, stdenv, fetchgit, libyaml}:

let python = pkgs.python33;
in stdenv.mkDerivation rec {
  version="0.3.14";
  name="coyaml-${version}";

  src = fetchgit {
    url = "git://github.com/tailhook/coyaml";
    rev = "fd16400";
    sha256 = "18feb32591ff013a99ce759d12d0b3fb91c27a3f1adf40bf7e7d1b1781c0ff6f";
  };

  buildInputs = with pkgs; [ python libyaml ];

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
    description = "Configuration file parser generator, that uses YAML for configuration files";
    homepage = "http://github.com/tailhook/coyaml";
    license = stdenv.lib.licenses.mit;
  };
}

