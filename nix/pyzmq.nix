{fetchurl, zeromq, buildPythonPackage}:

buildPythonPackage rec {
  name = "pyzmq-13.0.0";
  src = fetchurl {
    url = "http://pypi.python.org/packages/source/p/pyzmq/pyzmq-13.0.0.zip";
    md5 = "fa2199022e54a393052d380c6e1a0934";
  };
  buildInputs = [ zeromq ];
  doCheck = false;
}
