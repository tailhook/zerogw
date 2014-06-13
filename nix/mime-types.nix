{pkgs, stdenv, fetchurl}:

stdenv.mkDerivation rec {
  version="9";
  name="mime-types-${version}";

  src = fetchurl {
    url = "http://mirrors.kernel.org/gentoo/distfiles/${name}.tar.bz2";
    sha256 = "0pib8v0f5xwwm3xj2ygdi2dlxxvbq6p95l3fah5f66qj9xrqlqxl";
  };

  installPhase = ''
    install -Dm644 mime.types $out/share/mime.types
    '';

  meta = {
    description = "Provides mime-types file";
    homepage = "http://packages.gentoo.org/package/app-misc/mime-types";
    license = stdenv.lib.licenses.mit;
    platforms = stdenv.lib.platforms.linux;
  };
}

