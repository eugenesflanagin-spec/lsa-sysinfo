#!/bin/bash
# Make the generic Linux tarball lsa-sysinfo-VER-linux-x86_64.tar.gz from a
# built package (normally the Ubuntu 24.04 .deb) plus INSTALL.txt, LICENSE, README.md.
#   make-tarball.sh PACKAGE SOURCE_TARBALL OUTDIR
#     PACKAGE  lsa-sysinfo_VER-1.ubuntu24.04_amd64.deb (older glibc, so the tarball
#              runs on more distributions) or lsa-sysinfo-VER-1-x86_64.pkg.tar.zst
# Runs its work in a fresh archlinux:base-devel container (same on your PC and
# on GitHub), so the host only needs docker. The result is reproducible: sorted
# file names, owner root, all dates = the source's commit date.
set -euo pipefail
if [ "${1:-}" != --inside ]; then
  PKG=$(cd "$(dirname "$0")" && pwd)
  P=$(readlink -f "${1:?usage: make-tarball.sh PACKAGE SOURCE_TARBALL OUTDIR}")
  S=$(readlink -f "${2:?source tarball}")
  mkdir -p "${3:?output dir}"; OUT=$(readlink -f "$3")
  docker pull -q archlinux:base-devel >/dev/null
  exec docker run --rm --name "lsa-pkg-tarball-$$" \
    -v "$P:/in/$(basename "$P"):ro" -v "$S:/in/$(basename "$S"):ro" -v "$PKG:/pkg:ro" -v "$OUT:/out" \
    archlinux:base-devel bash /pkg/make-tarball.sh --inside "/in/$(basename "$P")" "/in/$(basename "$S")" "$(id -u):$(id -g)"
fi

# ---- inside the container
P=$2 S=$3 OWNER=$4
VER=$(basename "$S" .tar.gz); VER=${VER#lsa-sysinfo-}
TOP=lsa-sysinfo-$VER-linux-x86_64
W=/work; mkdir -p $W/src $W/$TOP && cd $W
tar xzf "$S" -C src
SRC=src/lsa-sysinfo-$VER
case $P in
  *.pkg.tar.zst) bsdtar -xf "$P" -C $TOP usr; FROM="the Arch Linux package" ;;
  *.deb)         bsdtar -xOf "$P" 'data.tar.*' | bsdtar -xf - -C $TOP ./usr
                 FROM="the $(bsdtar -xOf "$P" 'control.tar.*' | bsdtar -xOf - ./control | sed -n 's/^Version: //p') .deb package" ;;
  *) echo "unknown package type: $P" >&2; exit 1 ;;
esac
# lowest glibc the programs need = the highest GLIBC_x.y symbol version they use
GLIBC=$(objdump -T $TOP/usr/bin/lsa $TOP/usr/bin/lsa-gui | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -1)
title="Linux System Audit (lsa-sysinfo) $VER - generic Linux x86_64 build"
sed -e "s/@VERSION@/$VER/g" -e "s/@GLIBC@/$GLIBC/g" -e "s/@BUILT_FROM@/$FROM/g" \
    -e "s/@UNDERLINE@/$(printf '%*s' ${#title} '' | tr ' ' '=')/" /pkg/INSTALL.txt.in >$TOP/INSTALL.txt
grep -q '@[A-Z_]*@' $TOP/INSTALL.txt && { echo "INSTALL.txt still has a placeholder" >&2; exit 1; }
cp $SRC/LICENSE $SRC/README.md $TOP/
EPOCH=$(stat -c %Y $SRC/CHANGELOG.md)   # git archive stamps files with the commit date
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@"$EPOCH" -cf - $TOP | gzip -9n >/out/$TOP.tar.gz

# quick self-check of the result
mkdir $W/check && tar xzf /out/$TOP.tar.gz -C $W/check
for f in INSTALL.txt LICENSE README.md usr/bin/lsa usr/bin/lsa-gui \
         usr/share/doc/lsa-sysinfo/LSA-User-Guide.pdf usr/share/doc/lsa-sysinfo/html/LSA-User-Guide.html \
         usr/share/applications/lsa-cachemem.desktop usr/share/applications/lsa-user-guide.desktop; do
  [ -e "$W/check/$TOP/$f" ] || { echo "FAIL: $f missing from the tarball" >&2; exit 1; }
done
# --version only (no benchmark is run here)
"$W/check/$TOP/usr/bin/lsa" --version | head -1 | grep -qx "Linux System Audit (lsa) $VER" \
  || { echo "FAIL: lsa --version from the tarball is not $VER" >&2; exit 1; }
chown "$OWNER" /out/$TOP.tar.gz
echo "made $TOP.tar.gz from $(basename "$P") (needs glibc $GLIBC+), $(tar tzf /out/$TOP.tar.gz | wc -l) entries"
