# Changelog

Linux System Audit is a clean-room Linux implementation of AIDA64 Business features.
AIDA64 is by FinalWire Ltd. (https://www.aida64.com); Linux System Audit is not affiliated with FinalWire.

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
