#!/bin/bash
# Build the Windows files in a fresh archlinux:base-devel container:
#   build-win.sh SOURCE_TARBALL OUTDIR
#     SOURCE_TARBALL  lsa-sysinfo-VER.tar.gz from `git archive --prefix=lsa-sysinfo-VER/`
#     OUTDIR          gets pkg/lsa-sysinfo-VER-setup.exe, pkg/lsa-sysinfo-VER-win64-portable.zip
#                     and contents.txt (what is inside both)
# The programs are cross-compiled with Arch's mingw-w64-gcc (`make win-installer win-zip`).
# NSIS (makensis) is not in the Arch repositories, so it is built from its source
# release first, the same way as the AUR package (3.13, 64-bit stubs), using a
# mingw zlib built from source. Both downloads are checked against fixed SHA-256 sums.
# Used the same way by build-all.sh (your PC) and .github/workflows/release.yml.
set -euo pipefail
NSIS_VER=3.13
NSIS_SHA=a8ffe024602d46b6d766f9e1ce30c324ad2a24daeacd3efc2642d436a0c157ac
ZLIB_VER=1.3.1
ZLIB_SHA=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23

if [ "${1:-}" != --inside ]; then
  PKG=$(cd "$(dirname "$0")" && pwd)
  TARBALL=$(readlink -f "${1:?usage: build-win.sh SOURCE_TARBALL OUTDIR}")
  OUT=${2:?output dir}
  VER=$(basename "$TARBALL" .tar.gz); VER=${VER#lsa-sysinfo-}
  [[ $VER =~ ^[0-9]+(\.[0-9]+)+$ ]] || { echo "cannot get the version from $TARBALL" >&2; exit 1; }
  rm -rf "$OUT"; mkdir -p "$OUT/pkg"; OUT=$(readlink -f "$OUT")
  echo "### windows: building lsa-sysinfo $VER in archlinux:base-devel (mingw-w64 + NSIS $NSIS_VER)"
  docker pull -q archlinux:base-devel >/dev/null
  exec docker run --rm --name "lsa-pkg-build-windows-$$" \
    -v "$TARBALL:/in/lsa-sysinfo-$VER.tar.gz:ro" -v "$PKG:/pkg:ro" -v "$OUT:/out" \
    archlinux:base-devel bash -c "bash /pkg/build-win.sh --inside $VER; rc=\$?; chown -R $(id -u):$(id -g) /out; exit \$rc"
fi

# ---- inside the container
VER=$2
fetch() {  # fetch URL FILE SHA256
  curl -fsSL --retry 5 --retry-delay 5 -o "$2" "$1"
  echo "$3  $2" | sha256sum -c --quiet || { echo "checksum mismatch for $1" >&2; exit 1; }
}

echo "### toolchain"
pacman -Syu --noconfirm --needed mingw-w64-gcc scons python 7zip file >/tmp/toolchain.log 2>&1 \
  || { tail -30 /tmp/toolchain.log; exit 1; }
x86_64-w64-mingw32-gcc --version | head -1

echo "### NSIS $NSIS_VER from source (zlib $ZLIB_VER for mingw first)"
mkdir -p /build && cd /build
fetch "https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.gz" zlib.tar.gz $ZLIB_SHA
fetch "https://downloads.sourceforge.net/project/nsis/NSIS%203/$NSIS_VER/nsis-$NSIS_VER-src.tar.bz2" nsis.tar.bz2 $NSIS_SHA
tar xf zlib.tar.gz && tar xf nsis.tar.bz2
(cd zlib-$ZLIB_VER && make -f win32/Makefile.gcc PREFIX=x86_64-w64-mingw32- -j"$(nproc)" \
   && make -f win32/Makefile.gcc install PREFIX=x86_64-w64-mingw32- SHARED_MODE=1 \
        BINARY_PATH=/opt/zlib-w64/bin INCLUDE_PATH=/opt/zlib-w64/include LIBRARY_PATH=/opt/zlib-w64/lib) \
  >/tmp/zlib.log 2>&1 || { tail -30 /tmp/zlib.log; exit 1; }
# serial on purpose: the NSIS docs step changes directory and breaks a parallel (-j) build
(cd nsis-$NSIS_VER-src && scons VERSION=$NSIS_VER PREFIX=/usr PREFIX_CONF=/etc SKIPUTILS='NSIS Menu' STRIP_CP=false \
   TARGET_ARCH=amd64 ZLIB_W32=/opt/zlib-w64 NSIS_MAX_STRLEN=8192 NSIS_CONFIG_LOG=yes install) \
  >/tmp/nsis.log 2>&1 || { tail -40 /tmp/nsis.log; exit 1; }
echo "makensis $(makensis -VERSION)"

echo "### make win-installer win-zip"
mkdir -p /work && cd /work
tar xzf /in/lsa-sysinfo-$VER.tar.gz
cd lsa-sysinfo-$VER
[ "$(sed -n 's/^#define LSA_VERSION "\(.*\)"/\1/p' lsa.h)" = "$VER" ] || { echo "LSA_VERSION in lsa.h is not $VER" >&2; exit 1; }
make win-installer win-zip
SETUP=dist/win/lsa-sysinfo-$VER-setup.exe ZIP=dist/win/lsa-sysinfo-$VER-win64-portable.zip

echo "### checks"
FAILS=0
fail() { echo "FAIL: $*"; FAILS=$((FAILS+1)); }
for f in dist/win/lsa.exe dist/win/lsa-gui.exe; do
  file "$f" | grep -q 'PE32+ executable.*x86-64' && echo "OK $(file -b "$f" | cut -c1-60): $f" || fail "$f is not a 64-bit Windows program"
done
grep -aq "$VER" dist/win/lsa.exe && echo "OK lsa.exe has version $VER" || fail "lsa.exe does not contain $VER"
file "$SETUP" | grep -q 'PE32' && echo "OK installer is a Windows program" || fail "$SETUP is not a Windows program"
{ echo "== $(basename "$ZIP")"; bsdtar -tvf "$ZIP"; echo; echo "== $(basename "$SETUP") (7z l)"; 7z l "$SETUP" | sed -n '/^---/,/^---/p'; } >/out/contents.txt
zipl=$(bsdtar -tf "$ZIP"); setl=$(7z l -ba "$SETUP" 2>/dev/null | awk '{print $NF}')
for f in lsa.exe lsa-gui.exe LSA-User-Guide.html README.txt CHANGELOG.txt LICENSE.txt; do
  grep -qx "$f" <<<"$zipl" && echo "OK zip has $f" || fail "zip is missing $f"
  grep -q "$f" <<<"$setl" && echo "OK installer has $f" || fail "installer is missing $f"
done
# the guide in the zip must be the real HTML guide
bsdtar -xOf "$ZIP" LSA-User-Guide.html >/tmp/zip-guide.html
grep -qi '<html' /tmp/zip-guide.html && cmp -s /tmp/zip-guide.html docs/html/LSA-User-Guide.html \
  && echo "OK zip guide is the HTML guide ($(stat -c %s /tmp/zip-guide.html) bytes)" || fail "zip LSA-User-Guide.html is not the HTML guide"
cat /out/contents.txt
[ $FAILS -eq 0 ] || { echo "RESULT: FAIL ($FAILS)"; exit 1; }
cp "$SETUP" "$ZIP" /out/pkg/
echo "### built: $(ls /out/pkg)"
