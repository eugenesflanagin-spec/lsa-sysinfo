# Changelog

Linux System Audit is a clean-room Linux implementation of AIDA64 Business features.
AIDA64 is by FinalWire Ltd. (https://www.aida64.com); Linux System Audit is not affiliated with FinalWire.

## 0.1.3 - 2026-10-16

### Fixed
- **Memory Latency read ~7% low.** The test block was 4 x L3 (384 MB on a 7800X3D), so about a
  quarter of the random chain still hit the L3 cache. It is now 8 x L3 (at least 256 MB, capped at
  half of available RAM), leaving about 1/8 L3 hits.
  - Same machine, FCLK 2000: 80.5 / 80.7 / 81.6 ns (was 75.5-76.8 ns); reference 80.2 / 81.5 /
    82.8 ns.
  - On Windows the chain runs on 4 KB pages unless "Lock pages in memory" is granted, so the
    Windows figure is higher than the Linux one.

### Known differences
- **L1 Read is ~5% below the reference** (about 2250 vs 2445 GB/s on a 7800X3D). A single core
  reaches ~61 bytes per clock with 256-bit loads, but with all cores loaded LSA gets about
  289 GB/s per core against the reference's ~305. Longer runs (`--seconds 5`), one thread per
  core, 256-bit loads and per-thread offsets were all tried; none closed it without pushing other
  cells away from the reference, so it is left as measured.
- Run length: each test runs ~1 s (`--seconds`); 5 s gives the same results, so the shorter run
  costs no accuracy.

## 0.1.2 - 2026-10-15

### Added
- **Windows build + installer** for side-by-side tests on Windows:
  `lsa-sysinfo-0.1.2-setup.exe` (per-user install, no administrator rights; Start-menu shortcut,
  optional desktop shortcut, uninstaller in Apps & features) and a portable zip.
  - `lsa-gui.exe`: native Win32 Cache & Memory Benchmark window (no runtime needed); Save writes
    BMP + TXT; history in `%LOCALAPPDATA%\LSA\cachemem-history.txt`.
  - `lsa.exe`: the command line (`lsa cachemem ...`).
  - Same benchmark engine; Windows platform layer (QueryPerformanceCounter, thread affinity,
    GetLogicalProcessorInformationEx). Memory details from SMBIOS (no admin needed).
  - Latency uses 2 MB large pages only when Windows grants "Lock pages in memory"; otherwise
    4 KB pages - the status line says which.
- `make win-installer` / `make win-zip` (mingw-w64 + NSIS).

### Fixed
- Calibration cap overflowed on 32-bit `long` (Windows).

## 0.1.1 - 2026-10-15

### Fixed
- **Memory Write was ~3x too low on AMD** (33 GB/s vs ~93 GB/s). Streaming stores stop at the
  core-to-memory link's write limit (~16 B per fabric clock on Zen). On AMD CPUs with CLZERO,
  Memory Write now uses CLZERO (whole zeroed cache lines written to DRAM): one thread per
  physical core, threads interleaved in 4 KB chunks over a shared block of 256 MB or 2.5 x L3,
  best complete pass. Same-machine result: ~93,000 MB/s, matching the reference.
  `--mem-write stream` still shows the streaming-store figure; other CPUs keep streaming stores.

### Changed
- Read kernels use plain vector loads (no OR accumulation): L1 Read +7%, L2 Read +4%.

## 0.1.0 - 2026-10-15

### Added
- **Cache & Memory Benchmark** (`lsa-gui`, `lsa cachemem`): Read / Write / Copy (MB/s) and
  Latency (ns) for Memory, L1, L2 and L3, in the familiar grid, with CPU type / stepping /
  measured clock / multiplier, memory bus, DRAM:FSB ratio, memory type, chipset, motherboard and
  BIOS below it.
  - All-thread pinned bandwidth tests, AVX-512 / AVX2 / SSE2 chosen at run time, streaming
    stores for memory, per-thread start/end timing, calibrated 40-80 ms trials, best result.
  - Latency: random 64-byte-line pointer chase on 2 MB pages; memory block >= 4 x L3.
  - Save to PNG + TXT; run history in `~/.cache/lsa/cachemem-history.txt`.
- CHECKLIST.tsv: the 162 AIDA64 pages / benchmarks / tools to implement, mapped to Linux sources.
- EVIDENCE.md: how the specification was made (no execution of AIDA64, no disassembly).
