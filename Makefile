# Linux System Audit - clean-room Linux implementation of AIDA64 Business features.
CC ?= gcc
PREFIX ?= $(HOME)/.local
SYSPREFIX ?= /usr
DESTDIR ?=
WFLAGS = -std=c11 -Wall -Wextra -Wno-format-truncation -pthread
BASE = $(WFLAGS) $(CFLAGS)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
CORE = lsa.c cachemem.c cminfo.c guide.c

native: dist/native/lsa dist/native/lsa-gui

dist/native/lsa: $(CORE) cachemem.h lsa.h guide_map.inc
	@mkdir -p dist/native
	$(CC) $(BASE) $(LDFLAGS) -o $@ $(CORE) -lm

dist/native/lsa-gui: lsa_gui.c $(CORE) cachemem.h lsa.h guide_map.inc
	@mkdir -p dist/native
	$(CC) $(BASE) -DLSA_NO_MAIN $(GTK_CFLAGS) $(LDFLAGS) -o $@ lsa_gui.c $(CORE) $(GTK_LIBS) -lm

# ---- Windows (x86-64, static, no runtime needed): make win-installer / win-zip
MINGW64 ?= x86_64-w64-mingw32-gcc
WINDRES64 ?= x86_64-w64-mingw32-windres
WINFLAGS = -std=c11 -O2 -Wall -Wextra -Wno-format-truncation -static
VERSION = $(shell sed -n 's/^#define LSA_VERSION "\(.*\)"/\1/p' lsa.h)
WIN_CORE = lsa.c cachemem.c cminfo_win.c guide.c

win: dist/win/lsa.exe dist/win/lsa-gui.exe

dist/win/lsa_res.o: lsa.rc lsa.ico lsa.manifest
	@mkdir -p dist/win
	$(WINDRES64) lsa.rc -O coff -o $@

dist/win/lsa.exe: $(WIN_CORE) cachemem.h lsa.h guide_map.inc dist/win/lsa_res.o
	$(MINGW64) $(WINFLAGS) -o $@ $(WIN_CORE) dist/win/lsa_res.o -lpthread -lshell32

dist/win/lsa-gui.exe: lsa_win.c $(WIN_CORE) cachemem.h lsa.h guide_map.inc dist/win/lsa_res.o
	$(MINGW64) $(WINFLAGS) -municode -mwindows -DLSA_NO_MAIN -o $@ lsa_win.c $(WIN_CORE) dist/win/lsa_res.o -lpthread -lcomdlg32 -lgdi32 -lshell32

win-installer: win
	cp docs/html/LSA-User-Guide.html dist/win/LSA-User-Guide.html
	makensis -V2 -DVERSION=$(VERSION) -DDIST=dist/win lsa-setup.nsi

win-zip: win
	rm -f dist/win/lsa-sysinfo-$(VERSION)-win64-portable.zip
	cd dist/win && python3 -c "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED); [z.write(f) for f in ('lsa-gui.exe','lsa.exe')]; [z.write('../../'+a, b) for a,b in (('README.md','README.txt'),('CHANGELOG.md','CHANGELOG.txt'),('LICENSE','LICENSE.txt'),('docs/html/LSA-User-Guide.html','LSA-User-Guide.html'))]" lsa-sysinfo-$(VERSION)-win64-portable.zip

install-system: native
	install -Dm755 dist/native/lsa $(DESTDIR)$(SYSPREFIX)/bin/lsa
	install -Dm755 dist/native/lsa-gui $(DESTDIR)$(SYSPREFIX)/bin/lsa-gui
	install -Dm644 lsa-cachemem.desktop $(DESTDIR)$(SYSPREFIX)/share/applications/lsa-cachemem.desktop
	install -Dm644 lsa.png $(DESTDIR)$(SYSPREFIX)/share/icons/hicolor/256x256/apps/lsa.png
	install -Dm644 README.md $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/README.md
	install -Dm644 CHANGELOG.md $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/CHANGELOG.md
	install -Dm644 EVIDENCE.md $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/EVIDENCE.md
	install -Dm644 CHECKLIST.tsv $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/CHECKLIST.tsv
	install -Dm644 LICENSE $(DESTDIR)$(SYSPREFIX)/share/licenses/lsa-sysinfo/LICENSE
	install -Dm644 lsa-user-guide.desktop $(DESTDIR)$(SYSPREFIX)/share/applications/lsa-user-guide.desktop
	install -Dm644 docs/LSA-User-Guide.pdf $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/LSA-User-Guide.pdf
	install -Dm644 docs/html/LSA-User-Guide.html $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/html/LSA-User-Guide.html
	install -Dm644 -t $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo docs/USER-GUIDE.md docs/RESULTS.md docs/CLI-REFERENCE.md docs/GLOSSARY.md
	install -Dm644 -t $(DESTDIR)$(SYSPREFIX)/share/doc/lsa-sysinfo/images docs/images/*

# docs: rebuild the guide (needs pandoc + weasyprint) / check that every result has a guide section
guide:
	python3 docs/gen_guide_map.py
	python3 -I docs/build_lsa_pdf.py docs docs/LSA-User-Guide.pdf --html docs/html/LSA-User-Guide.html

check-guide:
	python3 docs/gen_guide_map.py --check

install: native
	$(MAKE) install-system DESTDIR= SYSPREFIX=$(PREFIX)

clean:
	rm -rf dist

.PHONY: native win win-installer win-zip install install-system guide check-guide clean
