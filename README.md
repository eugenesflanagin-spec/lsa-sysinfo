# Linux System Audit (lsa-sysinfo)

LSA is a clean-room Linux implementation of AIDA64 Business's Cache & Memory Benchmark, built to
produce results 1:1 with the native Windows app. On the reference system (Ryzen 7 7800X3D,
DDR5-6000, FCLK 2000) every cell - memory read / write / copy / latency and L1 / L2 / L3 - lands
within about 3% of AIDA64 except L1 Read (~5% low; see [CHANGELOG.md](CHANGELOG.md)). AIDA64
itself varies by a similar amount from run to run.

> AIDA64 is made by **FinalWire Ltd.** (https://www.aida64.com). Linux System Audit is an
> independent project and is **not affiliated with or endorsed by FinalWire**, and contains none
> of its code or data. No AIDA64 code was disassembled, decompiled or copied, and its encrypted
> hardware database and artwork are not used. See [EVIDENCE.md](EVIDENCE.md) for how the
> specification was made.

Native Linux (plain C, GTK3), GPL-2.0-or-later. Sister project of
[srs-sysinfo](https://github.com/eugenesflanagin-spec/srs-sysinfo) (SIV64X for Linux).

## Status

0.2.0: the first tool, **Cache & Memory Benchmark**, is complete, with a user guide
([docs/](docs/USER-GUIDE.md), installed as PDF + HTML). The rest of AIDA64's page tree,
benchmarks and tools are planned; [CHECKLIST.tsv](CHECKLIST.tsv) lists all 162 items.

Measured on the reference system (same machine, 3 runs of each):
every cell within ~3%, except L1 Read at ~2250-2330 GB/s against AIDA64's ~2445 (~5% low).

## Cache & Memory Benchmark

`lsa-gui` (menu: *LSA Cache & Memory Benchmark*) or `lsa cachemem` on the command line.

| | Read | Write | Copy | Latency |
|---|---|---|---|---|
| Memory, L1, L2, L3 | MB/s | MB/s | MB/s | ns |

- **Read / Write / Copy**: one thread pinned to every logical CPU, AVX-512 / AVX2 / SSE2 code
  picked at run time. Each thread works in its own block, sized to stay inside the cache level
  (half of its share of L1 / L2 / L3) or, for Memory, a block of at least 4 x L3 (min. 256 MB,
  default 1 GB) so it cannot be cache-resident. Memory Copy uses streaming stores. **Memory
  Write** on AMD CPUs with CLZERO writes zeroed cache lines (one thread per core, 4 KB
  interleave, 256 MB or 2.5 x L3 block, best full pass) - what the memory itself absorbs, since
  streaming stores stop at the core-to-memory link's write limit; `--mem-write stream` shows
  that figure. Other CPUs use streaming stores.
  **Copy counts bytes read + bytes written**, the usual convention for this benchmark.
  The best of repeated ~40-80 ms trials over ~1 s per cell is shown. Memory in MB/s, caches in
  GB/s (1 MB = 1,000,000 bytes; `--mib` for binary units).
- **Latency**: one thread, random pointer chase through every 64-byte line of a block. Caches
  use ordinary 4 KB pages (L1: half of L1d, L2: half of L2, L3: 8 x L2), memory uses 2 MB
  (transparent huge) pages over 8 x L3 (at least 256 MB, at most half of free RAM) so page walks
  and L3 hits do not dominate it.
- Below the grid: CPU type / stepping, measured clock, FSB (reference clock, measured as
  TSC x 100 / CPPC nominal frequency) and multiplier, North Bridge clock (AMD FCLK from the
  `ryzen_smu` PM table, Zen 4 desktop), memory bus, DRAM:FSB ratio, memory type, chipset,
  motherboard, BIOS. DIMM speed / type come from the root DMI cache written by `srs-root`
  (srs-sysinfo), since dmidecode needs root.
- **Guide** button / F1 / right-click a value: opens the user guide at that value's section
  (`lsa guide ITEM` on the command line).
- **Save** writes a PNG of the window plus a TXT; every finished run is appended to
  `~/.cache/lsa/cachemem-history.txt`.

```
lsa cachemem [--threads N] [--seconds S] [--isa avx512|avx2|sse2] [--mem-mib M]
             [--only mem,l1,l2,l3] [--tests read,write,copy,latency] [--mib]
             [--mem-write auto|clzero|stream] [--yes]
lsa guide [ITEM] [--print] | lsa guide --list
```

The benchmark loads every CPU thread and the memory bus for about 20 seconds and asks first
(`--yes` skips the question, e.g. in scripts).

## Windows

For side-by-side tests on Windows, each release has `lsa-sysinfo-<ver>-setup.exe` (per-user
install, no admin rights) and a portable zip: `lsa-gui.exe` (the Cache & Memory window, native
Win32) and `lsa.exe` (command line). Memory latency uses 2 MB large pages only if the account
has "Lock pages in memory" (secpol.msc -> Local Policies -> User Rights Assignment); otherwise
4 KB pages, which adds TLB-miss time - the status line shows which was used.

## Build

```
make native            # dist/native/lsa, dist/native/lsa-gui
make install           # ~/.local
make install-system DESTDIR=... SYSPREFIX=/usr
make win-installer win-zip   # Windows: dist/win (mingw-w64-gcc + nsis)
```

Dependencies: gtk3, glibc. Optional: srs-sysinfo (`srs-root` DMI cache for memory details).
