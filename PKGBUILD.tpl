# Maintainer: Paul Colomiets <pc@gafol.net>

pkgname=zerogw
pkgver=${VERSION}
pkgrel=1
pkgdesc="A http/zeromq gateway"
arch=('i686' 'x86_64')
url="http://github.com/tailhook/zerogw"
license=('GPL')
depends=('zeromq' 'libyaml')
makedepends=('coyaml>=0.3.2' 'libwebsite>=0.2.7')
backup=("etc/zerogw.yaml")
source=(https://github.com/downloads/tailhook/zerogw/$pkgname-$pkgver.tar.bz2)
md5sums=('${DIST_MD5}')

build() {
  cd $srcdir/$pkgname-$pkgver
  LDFLAGS="$LDFLAGS -Wl,--no-as-needed" ./waf configure --prefix=/usr
  ./waf build
}
package() {
  cd $srcdir/$pkgname-$pkgver
  ./waf install --destdir=$pkgdir
}
