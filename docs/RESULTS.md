# Linux System Audit - Result Reference

[Back to the User Guide](USER-GUIDE.md) - [Command-line reference](CLI-REFERENCE.md) - [Glossary](GLOSSARY.md)

One short section for every value LSA shows: the 16 cells of the grid and the lines below it.
In the window, **right-click a value** and choose **Open guide**, or point at it and press **F1**,
to jump straight here. On the command line, `lsa guide ITEM` does the same (`lsa guide --list`
shows the items).

Each section starts with the plain-English meaning, then the detail. Example figures are from the
author's PC (Ryzen 7 7800X3D, 2 x 16 GB DDR5-6000, FCLK 2133) unless a section says otherwise.

---

## Memory Read

`mem-read` - grid cell, bandwidth

**In plain words:** how much data per second the whole CPU can read from your RAM sticks.
Higher is better.

- Every CPU thread reads its own part of a block far bigger than all caches (default 1 GB in
  total, at least 4 x L3, at most a quarter of free RAM), so the data really comes from RAM.
- Shown in MB/s (1 MB = 1,000,000 bytes). Example: about 67,700 MB/s.
- What moves it: memory speed (MT/s), number of channels, on AMD the fabric clock (FCLK, see
  [North Bridge Clock](#north-bridge-clock)) and UCLK ratio, background load.
- Detail: [How the tests work](USER-GUIDE.md#71-bandwidth-tests).

## Memory Write

`mem-write` - grid cell, bandwidth

**In plain words:** how much data per second the CPU can write to RAM. Higher is better.

- On AMD CPUs with the CLZERO instruction LSA writes whole zeroed 64-byte lines straight to
  memory, one thread per physical core. That measures what the memory itself can take: about
  93,000 MB/s on the example PC.
- Other CPUs use streaming stores (writes that bypass the caches). `--mem-write stream` shows that
  figure on AMD too; it is much lower there (about 33,000 MB/s), because of the link between the
  cores and the memory controller.
- Detail: [Memory Write on AMD CPUs](USER-GUIDE.md#72-memory-write-on-amd-cpus-clzero).

## Memory Copy

`mem-copy` - grid cell, bandwidth

**In plain words:** how fast the CPU can read data from one place in RAM and write it to another.

- LSA counts **both** the bytes read and the bytes written, the usual convention, so Copy is
  close to Read rather than half of it. Example: about 67,400 MB/s.
- The writes are streaming stores. Same factors as [Memory Read](#memory-read).

## Memory Latency

`mem-latency` - grid cell, latency

**In plain words:** how long the CPU waits for one single, unpredictable read from RAM.
Lower is better.

- One thread follows a random chain of pointers through a block of 8 x L3 (at least 256 MB, at
  most half of free RAM) on 2 MB pages, so the CPU cannot guess the next address.
- Shown in ns (nanoseconds). Example: about 80-82 ns. Typical DDR5: 60-90 ns.
- What moves it: memory timings (CL, tRCD, ...), memory speed, FCLK and the UCLK ratio.
- If it reads 120-160 ns, another memory benchmark is probably running at the same time.
- Detail: [Latency test](USER-GUIDE.md#73-latency-test).

## L1 Cache Read

`l1-read` - grid cell, bandwidth

**In plain words:** how fast all cores together read from their own tiny, fastest cache.

- Each thread works on half of its L1 data cache. Shown in GB/s. Example: about 2250-2330 GB/s.
- Depends on the CPU clock and the code path (AVX-512 reads 64 bytes per instruction, AVX2 32).
- Known difference: about 5% below AIDA64 on the reference system. See
  [Known limits](USER-GUIDE.md#10-known-limits-and-known-differences-from-aida64).

## L1 Cache Write

`l1-write` - grid cell, bandwidth

**In plain words:** how fast all cores together write into their L1 cache.

- About half of L1 Read (example: about 1280 GB/s), because a core can do two reads but only one
  write per clock tick.

## L1 Cache Copy

`l1-copy` - grid cell, bandwidth

**In plain words:** read and write inside the L1 cache at the same time.

- Counts bytes read plus bytes written, so it can be higher than Read. Example: about 2460 GB/s.

## L1 Cache Latency

`l1-latency` - grid cell, latency

**In plain words:** the wait for one read that hits the L1 cache.

- One thread, random pointer chase over half of L1d. Example: 0.8 ns, about 4-5 clock ticks.
  It only changes with the CPU clock.

## L2 Cache Read

`l2-read` - grid cell, bandwidth

**In plain words:** how fast all cores together read from their L2 caches (bigger and a bit
slower than L1, still one per core).

- Each thread works on half of its share of L2. Example: about 1240 GB/s.
- Depends on the CPU clock.

## L2 Cache Write

`l2-write` - grid cell, bandwidth

**In plain words:** how fast all cores together write into their L2 caches.

- Example: about 1200 GB/s. Depends on the CPU clock.

## L2 Cache Copy

`l2-copy` - grid cell, bandwidth

**In plain words:** read and write inside the L2 cache at the same time (bytes read plus bytes
written).

- Example: about 1215 GB/s.

## L2 Cache Latency

`l2-latency` - grid cell, latency

**In plain words:** the wait for one read that hits the L2 cache.

- Random pointer chase over half of L2, 4 KB pages. Example: 3.6-3.9 ns.

## L3 Cache Read

`l3-read` - grid cell, bandwidth

**In plain words:** how fast all cores together read from the L3 cache, the big cache the cores
share.

- Each thread works on half of its share of L3. Example: about 700-760 GB/s.
- Depends on the CPU clock and, more than L1/L2, on background load (other programs use L3 too).

## L3 Cache Write

`l3-write` - grid cell, bandwidth

**In plain words:** how fast all cores together write into the shared L3 cache.

- Example: about 640 GB/s.

## L3 Cache Copy

`l3-copy` - grid cell, bandwidth

**In plain words:** read and write inside the L3 cache at the same time (bytes read plus bytes
written).

- Example: about 715 GB/s.

## L3 Cache Latency

`l3-latency` - grid cell, latency

**In plain words:** the wait for one read that hits the L3 cache.

- Random pointer chase over 8 x the L2 size. Example: about 11.6 ns.

## CPU Type

`cpu-type` - system line

**In plain words:** which processor you have, with the core count written as a word.

- Example: `OctalCore AMD Ryzen 7 7800X3D` (8 cores = Octal). From `/proc/cpuinfo`
  (Windows: CPUID).

## CPU Stepping

`cpu-stepping` - system line

**In plain words:** the CPU's design name and revision.

- Example: `Raphael, stepping 2  (family 19h model 61h)`. The code name comes from a built-in
  table of public CPU IDs; unknown CPUs show only family, model and stepping.

## CPU Clock

`cpu-clock` - system line

**In plain words:** how fast one core was actually ticking a moment ago.

- LSA **measures** it with a short timing loop (it is not the number on the box). Example: about
  4850-4970 MHz. It moves a little from run to run, because modern CPUs change clock all the time.

## CPU FSB

`cpu-fsb` - system line

**In plain words:** the base (reference) clock that the CPU clock is a multiple of. Usually
100 MHz; some boards let you raise it (on ASUS AM5 boards this is "eCLK").

- **Linux:** measured as TSC rate x 100 / the CPU's nominal frequency
  (`/sys/devices/system/cpu/cpu0/acpi_cppc/nominal_freq`). The TSC is a counter that runs at the
  CPU's nominal clock times the base clock / 100, so this gives the real base clock. Example:
  `105.0 MHz` on a board set to eCLK 105.
- It needs an invariant TSC (`constant_tsc` in `/proc/cpuinfo`) and the ACPI CPPC
  `nominal_freq` file. Without them LSA shows `100.0 MHz (nominal)`.
- **Windows:** always `100.0 MHz (nominal)` - not measured.

## CPU Multiplier

`cpu-multiplier` - system line

**In plain words:** how many times faster than the base clock the core runs.

- CPU Clock / CPU FSB. Example: 4846 MHz / 105.0 MHz = `46.16x`.
- When the FSB is the nominal 100 MHz, it is CPU Clock / 100.

## North Bridge Clock

`nb-clock` - system line

**In plain words:** on AMD Ryzen, the speed of the Infinity Fabric (FCLK), the internal link
between the cores and the memory controller. A faster fabric helps memory bandwidth and latency.

- Read from the PM table of the `ryzen_smu` kernel driver
  (`/sys/kernel/ryzen_smu_drv/pm_table`), read-only. Example: `2133.0 MHz` (FCLK 2133).
- Only the PM table layout `0x540104` (Ryzen 7000 desktop, Zen 4) is known to LSA. Other cases
  show why the value is missing:
  - `n/a (ryzen_smu driver not loaded)` - the driver is not installed or not loaded.
  - `n/a (PM table version 0x...... not supported)` - a CPU whose table layout LSA does not know.
  - `n/a (ryzen_smu pm_table not readable)` - the file exists but this user may not read it.
- **Windows:** not read (`n/a`).

## Memory Bus

`memory-bus` - system line

**In plain words:** the real clock of your RAM, half the "MT/s" figure on the box.

- Example: `3000.0 MHz` for DDR5-6000.
- Linux: from the root DMI cache written by `srs-root` (srs-sysinfo), because reading it needs
  root. Without it the line says `run srs-root once for DIMM data`. Windows: from SMBIOS.

## DRAM:FSB Ratio

`dram-fsb` - system line

**In plain words:** how many times faster than the base clock the memory clock is.

- Memory Bus / CPU FSB. A whole ratio is shown as `30:1`; otherwise with one decimal, for example
  `DRAM:FSB 28.6:1` (3000 MHz / 105.0 MHz).
- Needs the Memory Bus value (see above).

## Memory Type

`memory-type` - system line

**In plain words:** the kind of RAM, its speed and how many sticks and channels.

- Example: `Dual Channel DDR5-6000 SDRAM  (2 DIMMs)`. Same source as Memory Bus.
- If the speed is lower than your kit's rating, XMP / EXPO is probably off in the BIOS.

## Chipset

`chipset` - system line

**In plain words:** the motherboard's support chip and the CPU platform.

- Example: `AMD X870, AMD Raphael/Granite Ridge`. The chipset model comes from the board name, the
  platform from the PCI root complex name in hwdata `pci.ids`.

## Motherboard

`motherboard` - system line

**In plain words:** who made your motherboard and its model name.

- From `/sys/class/dmi/id/board_vendor` and `board_name` (Windows: SMBIOS).

## BIOS Version

`bios` - system line

**In plain words:** the version and date of the firmware that starts the PC.

- From `/sys/class/dmi/id/bios_version` and `bios_date` (Windows: SMBIOS). Example:
  `1805  (09/18/2026)`.

## Code path

`code-path` - status / last command-line line

**In plain words:** which set of CPU instructions the test loops used, and how many threads.

- AVX-512 (64 bytes per instruction), AVX2 (32) or SSE2 (16): the widest your CPU has, unless
  `--isa` asked for a narrower one. Cache bandwidth depends on it strongly, memory bandwidth hardly.
- The command line also lists the cache sizes LSA read from the kernel and the Memory Write method.
- Detail: [Code paths](USER-GUIDE.md#74-code-paths-avx-512-avx2-sse2).
