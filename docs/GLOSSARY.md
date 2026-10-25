# Linux System Audit - Glossary

[Back to the User Guide](USER-GUIDE.md) - [Command-line reference](CLI-REFERENCE.md)

Plain-English meanings of the words used in the LSA documents, A to Z.

- **AIDA64** - a system information and benchmark program for Windows made by FinalWire Ltd.
  LSA copies its Cache & Memory Benchmark layout so results can be compared. LSA is not affiliated
  with FinalWire.
- **AVX2** - "Advanced Vector Extensions 2". CPU instructions that handle 32 bytes at once.
  Found on most x86 CPUs since about 2013-2015.
- **AVX-512** - CPU instructions that handle 64 bytes at once. Found on AMD Zen 4 and newer and on
  some Intel CPUs. Gives the highest cache bandwidth figures.
- **Bandwidth** - how much data moves per second. Like the number of lanes on a road. Higher is better.
- **Best-of trials** - LSA repeats each measurement many times and keeps the fastest. Interruptions
  only ever make a trial slower, so the fastest one is the truest.
- **BIOS / UEFI** - the firmware that starts the PC and holds settings such as memory speed.
- **Bytes per clock** - how many bytes a core moves in one tick of its clock. A way to compare
  results at different clock speeds.
- **Cache** - small, very fast memory inside the CPU that keeps copies of recently used data, so
  the CPU does not have to wait for RAM.
- **Cache line** - the unit a cache works in: 64 bytes on current x86 CPUs. Reading one byte
  brings the whole 64-byte line in.
- **CCD / core complex** - on AMD Ryzen, a chip (die) holding up to 8 cores and their L3 cache.
- **Chipset** - the motherboard's support chip(s) that provide extra USB, SATA and PCIe connections.
- **CLZERO** - an AMD instruction that writes a whole 64-byte line of zeros to memory without
  first reading it. LSA uses it to measure Memory Write on AMD CPUs.
- **Code path** - which version of LSA's test loops is used: AVX-512, AVX2 or SSE2.
- **Copy** - reading data from one place and writing it to another. LSA counts both the bytes read
  and the bytes written.
- **DDR4 / DDR5** - the two current generations of desktop memory. DDR5 is faster in bandwidth;
  latency is similar.
- **DIMM** - one memory stick.
- **DMI / SMBIOS** - tables in the firmware that describe the PC (board, BIOS, memory sticks).
  Reading the memory part needs root on Linux.
- **dmidecode** - the Linux tool that reads DMI / SMBIOS tables. srs-root runs it and saves the
  output so LSA can read it without root.
- **DRAM** - "dynamic RAM", the kind of memory on your memory sticks.
- **DRAM:FSB ratio** - memory clock divided by the reference clock (FSB). DDR5-6000 runs its
  memory clock at 3000 MHz, so with a 100 MHz FSB the ratio is 30:1 (28.6:1 at 105 MHz).
- **Dual channel** - two independent paths between the CPU and memory. Twice the bandwidth of one.
  Usually means one stick in each of two channels.
- **eCLK** - on ASUS AM5 boards, the BIOS setting for the CPU's reference clock. Raising it from
  100 MHz raises the CPU clock with it; LSA shows it as CPU FSB.
- **EXPO / XMP** - memory profiles stored on the sticks (EXPO by AMD, XMP by Intel). Turning one on
  in the BIOS runs the memory at its rated speed instead of a slow default.
- **Fabric / Infinity Fabric** - the internal link on AMD CPUs that joins the cores to the memory
  controller and the rest of the chip.
- **FCLK** - the Infinity Fabric clock on AMD CPUs. Higher FCLK usually lowers memory latency.
- **FSB** - "front side bus". In older PCs the bus between CPU and chipset; today used as the name
  for the reference clock (normally 100 MHz) that the CPU multiplier is applied to. LSA measures it
  on Linux from the TSC and the CPU's nominal frequency.
- **GB/s** - gigabytes per second. 1 GB/s = 1,000 MB/s in LSA.
- **GiB/s, MiB/s** - binary units: 1 MiB = 1,048,576 bytes. Shown with `--mib`.
- **Huge pages / transparent huge pages (THP)** - memory pages of 2 MB instead of 4 KB. Fewer pages
  means fewer address translations, so memory latency measures the memory, not the translation.
- **L1 cache** - the first, smallest and fastest cache, inside each core (32-48 KB for data on
  current CPUs). Latency about 1 ns.
- **L2 cache** - second level, per core, about 0.5-2 MB. Latency about 3-4 ns.
- **L3 cache** - third level, shared by many cores, 16-128 MB. Latency about 10-12 ns. "X3D" AMD
  CPUs have an extra-large stacked L3 (96 MB on a 7800X3D).
- **Latency** - the waiting time for one piece of data. Lower is better.
- **Lock pages in memory** - a Windows user right needed for large pages. Without it LSA's Windows
  build measures memory latency on 4 KB pages, which reads higher.
- **Logical CPU / thread** - what the operating system sees as a CPU. With SMT, each core shows as
  two logical CPUs.
- **MB/s** - megabytes per second. In LSA 1 MB = 1,000,000 bytes.
- **Memory controller** - the part of the CPU that talks to the memory sticks.
- **MT/s** - mega-transfers per second, the speed rating of memory (DDR5-6000 = 6000 MT/s). The
  actual memory clock is half of it.
- **Multiplier** - CPU clock divided by the reference clock (e.g. 46.16 x 105 MHz = 4847 MHz).
- **Nominal frequency (CPPC)** - the CPU's rated base clock as the firmware reports it through ACPI
  CPPC (`/sys/devices/system/cpu/cpu0/acpi_cppc/nominal_freq`), counted at a 100 MHz reference.
- **ns (nanosecond)** - one billionth of a second.
- **North Bridge clock** - on old PCs the chipset's clock; on current AMD CPUs the Infinity Fabric
  clock (FCLK). LSA reads it from the ryzen_smu PM table on Ryzen 7000 desktop CPUs.
- **Page / page walk** - memory is managed in pages (4 KB normally). Finding where a page really is
  takes a "page walk", which costs time if it is not cached.
- **Pinned thread** - a thread that is not allowed to move to another CPU. Keeps measurements steady.
- **PM table** - a block of live values (clocks, voltages, power) that the AMD SMU publishes. The
  `ryzen_smu` driver makes it readable at `/sys/kernel/ryzen_smu_drv/pm_table`. Its layout changes
  between CPU generations, so LSA only reads the layout it knows (version `0x540104`).
- **Pointer chase** - a chain where each value read is the address of the next read. The CPU cannot
  guess ahead, so each step shows the full latency.
- **Prefetch** - the CPU fetching data before it is asked for, because it guessed the pattern.
  Good for programs, bad for latency tests, so LSA uses a random chain.
- **RAM** - the main memory of the PC.
- **Read** - the CPU fetching data from a cache or memory.
- **ryzen_smu** - an optional Linux kernel driver that gives read access to the AMD SMU's PM table.
- **SMT** - "simultaneous multithreading" (Intel calls it Hyper-Threading). One core runs two threads.
- **SMU** - AMD's "System Management Unit", a small processor inside the CPU that controls clocks and
  power. LSA never sends it commands; it only reads the PM table through ryzen_smu.
- **srs-root** - the root helper from the sister project srs-sysinfo. Run once, it saves root-only
  hardware data (including memory details) into `~/.cache/srs/root`.
- **SSE2** - CPU instructions that handle 16 bytes at once. Every 64-bit x86 CPU has them.
- **Stepping** - the revision number of a CPU design.
- **Streaming store** - a write that goes straight to memory without being kept in the cache.
  Used for big copies so the cache is not filled with data that will not be read again.
- **TLB** - "translation lookaside buffer", a small cache of recent address translations.
  A TLB miss means a page walk.
- **TSC** - "time stamp counter", a CPU counter that ticks at a fixed rate (the nominal clock) on
  current CPUs (`constant_tsc`). LSA times it to measure the FSB.
- **UCLK** - on AMD CPUs, the memory controller clock. "1:1" means it runs at the memory clock;
  "1:2" means half. 1:1 gives lower latency when the memory can be run that way.
- **Write** - the CPU sending data to a cache or memory.
- **X3D** - AMD CPUs with an extra cache chip stacked on top ("3D V-Cache"), giving a very large L3.
