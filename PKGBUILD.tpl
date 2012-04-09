# Maintainer: Paul Colomiets <pc@gafol.net>

pkgname=zerogw
pkgver=${VERSION}
pkgrel=1
pkgdesc="A http/zeromq gateway"
arch=('i686' 'x86_64')
url="http://github.com/tailhook/zerogw"
license=('MIT')
depends=('zeromq' 'libyaml' 'openssl' 'libev' 'mime-types')
makedepends=('coyaml' 'libwebsite>=0.2.20' 'python-pyzmq>=2.1.9' 'mime-types')
backup=("etc/zerogw.yaml")
source=(https://github.com/downloads/tailhook/zerogw/$pkgname-$pkgver.tar.bz2)
md5sums=('${DIST_MD5}')

build() {
  cd $srcdir/$pkgname-$pkgver
  LDFLAGS="$LDFLAGS -Wl,--no-as-needed" ./waf configure --prefix=/usr
  ./waf build
}

check() {
  cd $srcdir/$pkgname-$pkgver
  ./waf test
}

package() {
  cd $srcdir/$pkgname-$pkgver
  ./waf install --destdir=$pkgdir
  install -D -m644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
