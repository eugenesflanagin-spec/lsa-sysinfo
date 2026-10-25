#!/bin/bash
# Fresh-container install + read-only checks for lsa-sysinfo
# (run by test-in-container.sh as: docker run -e KIND=arch|deb|rpm -e VER=x.y.z -v pkgs:/pkgs -v packaging:/pkg IMAGE bash /pkg/test.sh)
# No benchmark is run: only --help / --version, the guide lookup and the
# argument / "asks first" checks, which all stop before any test starts.
set -u
. /pkg/test-common.sh
unset LSA_TEST_NOCONFIRM   # would skip the "asks first" check below
install_pkgs
for b in lsa lsa-gui; do
  command -v $b >/dev/null && echo "OK installed: $(command -v $b)" || fail "missing $b"
done
check_ldd /usr/bin/lsa /usr/bin/lsa-gui
run_cmd "lsa --help" lsa --help
run_cmd "lsa --version" lsa --version
lsa --version 2>&1 | head -1 | grep -q "^Linux System Audit (lsa) ${VER}\$" && echo "OK version ${VER}" || fail "--version is not ${VER}"
run_cmd "lsa cachemem --help" lsa cachemem --help
# lsa-gui with arguments behaves like the CLI (no window is opened)
run_cmd "lsa-gui --help" lsa-gui --help
run_cmd "lsa-gui --version" lsa-gui --version
lsa-gui --version 2>&1 | head -1 | grep -q "^Linux System Audit (lsa) ${VER}\$" && echo "OK lsa-gui version ${VER}" || fail "lsa-gui --version is not ${VER}"

echo "--- command-line refusals (exit code 2, nothing is started)"
# Only on the version under test: older builds did not ask first and would start the benchmark.
# The timeout is a second guard, should one ever start anyway.
if lsa --version 2>&1 | head -1 | grep -q "^Linux System Audit (lsa) ${VER}\$"; then
  run_expect_rc 2 "lsa cachemem --isa bogus" "unknown --isa value 'bogus'" timeout -s KILL 10 lsa cachemem --isa bogus
  run_expect_rc 2 "lsa cachemem without a terminal and without --yes" "not started" timeout -s KILL 10 lsa cachemem
else
  fail "skipped the refusal checks: the installed lsa is not ${VER}"
fi

echo "--- user guide"
D=/usr/share/doc/lsa-sysinfo
pdf=$D/LSA-User-Guide.pdf html=$D/html/LSA-User-Guide.html
[ -f "$pdf" ] && [ "$(head -c5 "$pdf")" = "%PDF-" ] && echo "OK real PDF: $pdf ($(stat -c %s "$pdf") bytes)" || fail "$pdf missing or not a PDF"
[ -e "$pdf.gz" ] && fail "$pdf.gz exists (compressed)"
[ -f "$html" ] && grep -qi '<html' "$html" && echo "OK html: $html ($(stat -c %s "$html") bytes)" || fail "$html missing or not HTML"
[ -e "$html.gz" ] && fail "$html.gz exists (compressed)"
for f in USER-GUIDE.md RESULTS.md CLI-REFERENCE.md GLOSSARY.md README.md CHANGELOG.md EVIDENCE.md; do
  [ -f "$D/$f" ] && echo "OK $D/$f" || fail "missing $D/$f"
done
ls $D/CHECKLIST.tsv* >/dev/null 2>&1 && echo "OK $(ls $D/CHECKLIST.tsv*)" || fail "missing CHECKLIST.tsv"
n=$(ls $D/images/*.png 2>/dev/null | wc -l); [ "$n" -gt 0 ] && echo "OK images/*.png: $n" || fail "no images/*.png"
z=$(find $D -name '*.gz' | tr '\n' ' '); echo "compressed files in $D: ${z:-none}"
HEADN=40 run_cmd "lsa guide --list" lsa guide --list
lsa guide --list 2>&1 | grep -q '^cpu-fsb ' && echo "OK guide --list has cpu-fsb" || fail "guide --list does not list cpu-fsb"
run_cmd "lsa guide cpu-fsb --print" lsa guide cpu-fsb --print
lsa guide cpu-fsb --print 2>&1 | grep -E "^Command .*($D/LSA-User-Guide\.pdf|$D/html/LSA-User-Guide\.html)#results--cpu-fsb" >/dev/null \
  && echo "OK guide command points at the installed guide" || fail "guide --print does not point at $D"
[ -f /usr/share/applications/lsa-user-guide.desktop ] && echo "OK lsa-user-guide.desktop" || fail "missing lsa-user-guide.desktop"
[ -f /usr/share/applications/lsa-cachemem.desktop ] && echo "OK lsa-cachemem.desktop" || fail "missing lsa-cachemem.desktop"
[ -f /usr/share/icons/hicolor/256x256/apps/lsa.png ] && echo "OK icon" || fail "missing icon lsa.png"

echo "--- desktop-file-validate"
if [ "$KIND" = arch ]; then pacman -S --noconfirm --needed desktop-file-utils >/dev/null 2>&1
elif [ "$KIND" = deb ]; then apt-get install -y -q --no-install-recommends desktop-file-utils >/dev/null 2>&1
elif command -v dnf >/dev/null; then dnf -y -q install desktop-file-utils >/dev/null 2>&1
else zypper -n -q install --no-recommends desktop-file-utils >/dev/null 2>&1; fi
if command -v desktop-file-validate >/dev/null; then
  for f in /usr/share/applications/lsa-*.desktop; do
    out=$(desktop-file-validate "$f" 2>&1); rc=$?
    [ $rc -eq 0 ] && echo "OK desktop-file-validate $(basename $f)${out:+ ($out)}" || { fail "desktop-file-validate $f"; echo "$out"; }
  done
else fail "desktop-file-validate not available"; fi
finish
