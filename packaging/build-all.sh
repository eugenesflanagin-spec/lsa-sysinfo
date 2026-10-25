#!/bin/bash
# Build + install-test every release file on your own PC, the same way the
# GitHub workflow (.github/workflows/release.yml) does: same images, same scripts.
#
#   packaging/build-all.sh [-o OUTDIR] [SOURCE_TARBALL] [TARGET...]
#     SOURCE_TARBALL  lsa-sysinfo-VER.tar.gz (git archive, prefix lsa-sysinfo-VER/).
#                     Default: made from the current git HEAD (committed files only).
#     TARGET          any of: arch debian-13 ubuntu-24.04 fedora-44 opensuse-tumbleweed windows
#                     (default: all of them)
#     OUTDIR          default: dist/release-VER in the repo (dist/ is git-ignored)
#
# Result:
#   OUTDIR/<distro>/pkg/<package>   OUTDIR/<distro>/{build.log,test.log,lintian.txt|rpmlint.txt,...}
#   OUTDIR/windows/pkg/             the Windows installer and portable zip, OUTDIR/windows/build.log
#   OUTDIR/release/                 exactly the files a release gets, plus SHA256SUMS and release-notes.md
# Needs docker. Containers are --rm and named lsa-pkg-*; nothing is pushed or uploaded.
set -euo pipefail
PKG=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$PKG")
. "$PKG/distros.sh"
OUT=
if [ "${1:-}" = -o ]; then OUT=$2; shift 2; fi

if [ $# -gt 0 ] && [[ $1 == *.tar.gz ]]; then
  TARBALL=$(readlink -f "$1"); shift
  VER=$(basename "$TARBALL" .tar.gz); VER=${VER#lsa-sysinfo-}
else
  VER=$("$PKG/release-info.sh" version)
  TARBALL=
fi
[[ $VER =~ ^[0-9]+(\.[0-9]+)+$ ]] || { echo "cannot get the version (got '$VER')"; exit 1; }
OUT=$(mkdir -p "${OUT:-$ROOT/dist/release-$VER}" && readlink -f "${OUT:-$ROOT/dist/release-$VER}")
if [ -z "$TARBALL" ]; then
  [ -n "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ] && echo "note: uncommitted changes are NOT in the build (git archive uses HEAD)"
  TARBALL=$OUT/lsa-sysinfo-$VER.tar.gz
  git -C "$ROOT" archive --format=tar.gz --prefix="lsa-sysinfo-$VER/" -o "$TARBALL" HEAD
  echo "source: git archive of $(git -C "$ROOT" rev-parse --short HEAD) -> $TARBALL"
fi
DISTROS=() WIN=0
if [ $# -eq 0 ]; then DISTROS=("${ALL_DISTROS[@]}"); WIN=1; fi
for d in "$@"; do
  if [ "$d" = windows ]; then WIN=1; else known_distro "$d"; DISTROS+=("$d"); fi
done

declare -A RESULT
for d in "${DISTROS[@]}"; do
  echo "===== $d"
  mkdir -p "$OUT/$d"
  if "$PKG/build.sh" "$d" "$TARBALL" "$OUT/$d" >"$OUT/$d.build.log" 2>&1; then
    mv "$OUT/$d.build.log" "$OUT/$d/build.log"
    echo "build OK: $(ls "$OUT/$d/pkg")"
  else
    mkdir -p "$OUT/$d"; mv "$OUT/$d.build.log" "$OUT/$d/build.log"
    echo "BUILD FAILED ($d), tail of $OUT/$d/build.log:"; tail -25 "$OUT/$d/build.log"; RESULT[$d]="build FAIL"; continue
  fi
  if "$PKG/test-in-container.sh" "$d" "$OUT/$d/pkg" "$VER" >"$OUT/$d/test.log" 2>&1; then
    RESULT[$d]="build OK, test PASS"
  else
    RESULT[$d]="build OK, test FAIL"
  fi
  tail -1 "$OUT/$d/test.log"
done

if [ $WIN = 1 ]; then
  echo "===== windows"
  mkdir -p "$OUT/windows"
  if "$PKG/build-win.sh" "$TARBALL" "$OUT/windows" >"$OUT/windows.build.log" 2>&1; then
    mv "$OUT/windows.build.log" "$OUT/windows/build.log"
    echo "build OK: $(ls "$OUT/windows/pkg")"; RESULT[windows]="OK"
  else
    mkdir -p "$OUT/windows"; mv "$OUT/windows.build.log" "$OUT/windows/build.log"
    echo "BUILD FAILED (windows), tail of $OUT/windows/build.log:"; tail -25 "$OUT/windows/build.log"; RESULT[windows]="FAIL"
  fi
fi

# generic tarball + release folder (only when the Ubuntu 24.04 package was built)
REL=$OUT/release
if [ -f "$OUT/ubuntu-24.04/pkg/lsa-sysinfo_$VER-1.ubuntu24.04_amd64.deb" ]; then
  echo "===== generic tarball"
  if "$PKG/make-tarball.sh" "$OUT/ubuntu-24.04/pkg/lsa-sysinfo_$VER-1.ubuntu24.04_amd64.deb" "$TARBALL" "$OUT/tarball" >"$OUT/tarball.log" 2>&1; then
    tail -1 "$OUT/tarball.log"; RESULT[tarball]="OK"
  else
    tail -20 "$OUT/tarball.log"; RESULT[tarball]="FAIL"
  fi
  rm -rf "$REL"; mkdir -p "$REL"
  for d in "${DISTROS[@]}"; do [ -d "$OUT/$d/pkg" ] && cp "$OUT/$d"/pkg/* "$REL/"; done
  [ $WIN = 1 ] && [ -d "$OUT/windows/pkg" ] && cp "$OUT"/windows/pkg/* "$REL/"
  cp "$OUT"/tarball/*.tar.gz "$REL/" 2>/dev/null || true
  tar xzOf "$TARBALL" "lsa-sysinfo-$VER/docs/LSA-User-Guide.pdf" >"$REL/LSA-User-Guide.pdf"
  (cd "$REL" && sha256sum -- * >SHA256SUMS)
  "$PKG/release-info.sh" notes "v$VER" >"$OUT/release-notes.md"
  echo "release files in $REL:"; (cd "$REL" && ls -l)
fi

echo "===== summary ($VER)"
for d in "${DISTROS[@]}" ${RESULT[windows]:+windows} ${RESULT[tarball]:+tarball}; do printf '%-22s %s\n' "$d" "${RESULT[$d]}"; done
for d in "${DISTROS[@]}" ${RESULT[windows]:+windows} ${RESULT[tarball]:+tarball}; do [[ ${RESULT[$d]} == *PASS || ${RESULT[$d]} == OK ]] || exit 1; done
