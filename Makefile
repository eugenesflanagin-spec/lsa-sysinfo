# Linux System Audit - clean-room Linux implementation of AIDA64 Business features.
CC ?= gcc
PREFIX ?= $(HOME)/.local
SYSPREFIX ?= /usr
DESTDIR ?=
WFLAGS = -std=c11 -Wall -Wextra -Wno-format-truncation -pthread
BASE = $(WFLAGS) $(CFLAGS)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)
CORE = lsa.c cachemem.c cminfo.c

native: dist/native/lsa dist/native/lsa-gui

dist/native/lsa: $(CORE) cachemem.h lsa.h
	@mkdir -p dist/native
	$(CC) $(BASE) $(LDFLAGS) -o $@ $(CORE) -lm

dist/native/lsa-gui: lsa_gui.c $(CORE) cachemem.h lsa.h
	@mkdir -p dist/native
	$(CC) $(BASE) -DLSA_NO_MAIN $(GTK_CFLAGS) $(LDFLAGS) -o $@ lsa_gui.c $(CORE) $(GTK_LIBS) -lm

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

install: native
	$(MAKE) install-system DESTDIR= SYSPREFIX=$(PREFIX)

clean:
	rm -rf dist

.PHONY: native install install-system clean
