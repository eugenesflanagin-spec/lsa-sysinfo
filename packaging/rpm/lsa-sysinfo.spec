Name:           lsa-sysinfo
Version:        0.1.3
Release:        1%{?dist}
Summary:        Linux System Audit - cache and memory benchmark
License:        GPL-2.0-or-later
URL:            https://github.com/eugenesflanagin-spec/lsa-sysinfo
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
Source1:        lsa.1
Source2:        lsa-gui.1
ExclusiveArch:  x86_64

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  desktop-file-utils
Requires:       hwdata
Requires:       xdg-utils
%if !0%{?suse_version}
Requires:       hicolor-icon-theme
%endif
Suggests:       srs-sysinfo

%description
Linux System Audit is a clean-room Linux implementation of AIDA64 Business
features. AIDA64 is the work of FinalWire Ltd.; Linux System Audit contains
no AIDA64 code, binaries or data files and is not affiliated with or
endorsed by FinalWire.

This release provides the Cache & Memory Benchmark: read, write, copy
(MB/s) and latency (ns) of memory and the L1, L2 and L3 caches, using every
CPU thread, as a GTK 3 window (lsa-gui) and on the command line (lsa).
With srs-sysinfo installed, its root data cache adds memory speed and type
details. The North Bridge Clock (FCLK) line needs the optional ryzen_smu
kernel driver (not packaged in the distribution) on Ryzen 7000 desktop CPUs.

A user guide (PDF and HTML, in /usr/share/doc/lsa-sysinfo) explains every
result and system line; F1, the Guide button or "lsa guide ITEM" opens it at
that item's section.

%prep
%autosetup -n %{name}-%{version}

%build
%set_build_flags
# the Makefile links without -pie; ask for a PIE explicitly (default on Fedora, not on openSUSE)
export CFLAGS="$CFLAGS -fPIE" LDFLAGS="$LDFLAGS -pie"
%make_build CC=gcc native

%install
%make_build CC=gcc DESTDIR=%{buildroot} SYSPREFIX=%{_prefix} install-system
# the licence is packaged with %%license below. The doc tree installed by
# install-system (README, CHANGELOG, user guide PDF/HTML/Markdown, images) is
# kept as it is: the programs open /usr/share/doc/lsa-sysinfo/LSA-User-Guide.pdf
# and html/LSA-User-Guide.html by these exact paths (on openSUSE too, where
# %%{_docdir} is /usr/share/doc/packages).
rm -rf %{buildroot}%{_datadir}/licenses
install -Dm644 -t %{buildroot}%{_mandir}/man1 %{SOURCE1} %{SOURCE2}

%check
for f in %{buildroot}%{_datadir}/applications/*.desktop; do desktop-file-validate "$f"; done

%files
%license LICENSE
%doc %{_datadir}/doc/%{name}/
%{_bindir}/lsa
%{_bindir}/lsa-gui
%{_datadir}/applications/lsa-cachemem.desktop
%{_datadir}/applications/lsa-user-guide.desktop
%{_datadir}/icons/hicolor/256x256/apps/lsa.png
%{_mandir}/man1/lsa.1*
%{_mandir}/man1/lsa-gui.1*

%changelog
* Sun Oct 25 2026 Eugene Flanagin <eflanagin@pm.me> - 0.1.3-1
- Native package of upstream release 0.1.3
