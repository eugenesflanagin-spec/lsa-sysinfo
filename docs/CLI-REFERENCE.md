# Linux System Audit - Command-Line Reference

[Back to the User Guide](USER-GUIDE.md) - [Glossary](GLOSSARY.md)

Linux System Audit is a clean-room Linux implementation of AIDA64 Business features. AIDA64 is by
FinalWire Ltd.; LSA is not affiliated with or endorsed by FinalWire.

---

## In plain words

There is one command, `lsa`. `lsa cachemem` runs the Cache & Memory Benchmark and prints the
result grid as text; `lsa guide` opens this guide at the section for one value.

> **Before a run:** the benchmark loads every CPU thread and the memory bus for about 20 seconds.
> Like the window, the command line **asks first** (`Start now? [y/N]`). In scripts add `--yes`;
> without a terminal and without `--yes` it does not start. Close other heavy programs first, and
> never run it while another memory benchmark (for example AIDA64 under Wine) is running.

`lsa-gui` given any argument that does not start with `--gui` behaves exactly like `lsa`
(for example `lsa-gui cachemem --only mem`). On Windows the same command is `lsa.exe`.

## Synopsis

```
lsa cachemem [--threads N] [--seconds S] [--isa avx512|avx2|sse2] [--mem-mib M]
             [--only mem,l1,l2,l3] [--tests read,write,copy,latency] [--mib]
             [--mem-write auto|clzero|stream] [--yes]
lsa guide [ITEM] [--print]
lsa guide --list
lsa --version
lsa --help | -h
```

## Commands

### `lsa --help`, `lsa -h`

Prints the usage text. Exit code 0. (`lsa` with no arguments, or with an unknown command, prints
the same text and exits with code 2.)

```
usage: lsa cachemem [--threads N] [--seconds S] [--isa avx512|avx2|sse2] [--mem-mib M]
                    [--only mem,l1,l2,l3] [--tests read,write,copy,latency] [--mib]
                    [--mem-write auto|clzero|stream] [--yes]
       lsa guide [ITEM] [--print] | lsa guide --list
       lsa --version | --help

cachemem  Cache & Memory benchmark: Read / Write / Copy (MB/s) and Latency (ns) of
          Memory, L1, L2 and L3. Uses every CPU thread and the memory bus for ~20 s.
          Asks before it starts; --yes (or -y) starts at once, e.g. in scripts.
guide     Open the user guide at ITEM's section (e.g. mem-latency, cpu-fsb); --print shows
          the command instead of running it; --list lists the items.
```

### `lsa --version`

```
Linux System Audit (lsa) 0.2.0
Clean-room Linux implementation of AIDA64 Business features. AIDA64 is by FinalWire Ltd. (aida64.com);
Linux System Audit is not affiliated with FinalWire. GPL-2.0-or-later.
```

### `lsa cachemem [options]`

Runs the benchmark. With no options it measures all 16 cells (4 levels x 4 tests) with the
defaults below, exactly as the window does. It asks first unless `--yes` is given.
`lsa cachemem --help` prints the usage text and exits with code 0.

## Options for `cachemem`

| Option | Default | What it does |
|---|---|---|
| `--threads N` | all logical CPUs | Use only N worker threads for the bandwidth tests (on CPUs 0..N-1 of the allowed set). Values of 0, negative, or more than the CPU count mean "all". Latency always uses 1 thread. |
| `--seconds S` | `1` | Measuring time per cell, in seconds. Longer runs do not change the result much (5 s gives the same figures as 1 s on the reference PC); they mostly cost time. |
| `--isa avx512\|avx2\|sse2` | widest the CPU supports | Force a narrower code path. Asking for one the CPU lacks falls back to the best it has. Any other word stops with an error that lists the valid values. |
| `--mem-mib M` | auto (1024 MiB, capped at a quarter of available RAM) | Total size of the Memory bandwidth block, in MiB, for all threads together. Use a smaller value on PCs with little free RAM. Values below 256 MiB or 4 x L3 (whichever is bigger) are raised to that, so the block can never fit in the caches. |
| `--only LIST` | all levels | Measure only these levels. Comma-separated words: `mem`, `l1`, `l2`, `l3`. |
| `--tests LIST` | all tests | Measure only these tests. Comma-separated words: `read`, `write`, `copy`, `latency` (`lat` also works). |
| `--mib` | off | Show binary units: MiB/s for memory and GiB/s for caches (1 MiB = 1,048,576 bytes, 1 GiB = 1024 MiB). |
| `--yes`, `-y` | off | Start without asking. Needed when there is no terminal (scripts, cron). The environment variable `LSA_TEST_NOCONFIRM` does the same. |
| `--mem-write auto\|clzero\|stream` | `auto` | How Memory Write is measured. `auto`: CLZERO on AMD CPUs that have it, streaming stores elsewhere. `stream`: always streaming stores (shows the core-to-memory link limit on AMD). `clzero`: same as auto (CLZERO is used only if the CPU has it). |

Notes:

- `--only` and `--tests` combine: `--only l1 --tests read` measures one cell (L1 Read).
  If you give only one of them, the other means "all".
- An unknown word in `--only`, `--tests`, `--isa` or `--mem-write` stops at once, before anything
  runs, with the valid values, for example:
  `cachemem: unknown --only value 'l4' (valid: mem, l1, l2, l3)` (exit code 2).
- An unknown option prints an error and the usage text and exits with code 2 without running anything.

## Output

While it runs, one progress line per finished cell goes to **standard error**:

```
  L1 Cache Read      2292344 MB/s
```

(progress lines always show MB/s - MiB/s with `--mib` - and ns, without converting caches to GB/s).

At the end, the result goes to **standard output**:

```
Linux System Audit 0.1.3 - Cache & Memory Benchmark

                     Read          Write           Copy      Latency
Memory          67678 MB/s     93211 MB/s     67373 MB/s      81.8 ns
L1 Cache       2292.3 GB/s    1279.0 GB/s    2459.7 GB/s       0.8 ns
L2 Cache       1240.2 GB/s    1198.0 GB/s    1215.6 GB/s       3.9 ns
L3 Cache       764.39 GB/s    637.68 GB/s    715.16 GB/s      11.6 ns

CPU Type         OctalCore AMD Ryzen 7 7800X3D
CPU Stepping     Raphael, stepping 2  (family 19h model 61h)
CPU Clock        4968.7 MHz
CPU FSB          105.0 MHz  (multiplier 47.32x)
NB Clock         2133.0 MHz
Memory Bus       3000.0 MHz  DRAM:FSB 28.6:1
Memory Type      Dual Channel DDR5-6000 SDRAM  (2 DIMMs)
Chipset          AMD X870, AMD Raphael/Granite Ridge
Motherboard      ASUSTeK COMPUTER INC. TUF GAMING X870-PRO WIFI7 W NEO
BIOS Version     1805  (09/18/2026)
Code path        AVX-512, 16 threads; L1d 32 KB, L2 1024 KB, L3 98304 KB; Memory Write: CLZERO, one thread per core
```

(Example values from the author's PC: the grid comes from a saved run, the system lines from the
same machine with its base clock (eCLK) at 105 MHz and FCLK at 2133 MHz. What each line means:
[RESULTS.md](RESULTS.md).)

A one-cell run (`lsa cachemem --only l1 --tests latency --yes`, about 2 seconds) looks like this:

```
Linux System Audit 0.1.3 - Cache & Memory Benchmark

                     Read          Write           Copy      Latency
Memory                  -              -              -            -
L1 Cache                -              -              -       0.8 ns
L2 Cache                -              -              -            -
L3 Cache                -              -              -            -

CPU Type         OctalCore AMD Ryzen 7 7800X3D
...
Code path        AVX-512, 16 threads; L1d 32 KB, L2 1024 KB, L3 98304 KB
```

- A cell that was not measured shows `-`.
- The "Memory Write: ..." note at the end of the last line appears only when Memory Write was measured.
- Cache cells use two decimals below 1000 GB/s and one decimal above.
- The last line tells you the code path, thread count, the cache sizes LSA read from the kernel,
  and the Memory Write method.
- Command-line runs are **not** added to `~/.cache/lsa/cachemem-history.txt` (only window runs are).

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Finished, or `--help` / `--version` / `guide` |
| 1 | The test memory could not be allocated (`cachemem: could not allocate the test memory`), or the guide could not be opened |
| 2 | No command, unknown command, unknown option or value, or the run was not confirmed |

## Examples

```
# Full run, save the result (asks first; the question goes to the terminal, not the file)
lsa cachemem > cachemem-$(date +%F).txt

# Memory only (Read, Write, Copy, Latency) - about 6 seconds
lsa cachemem --only mem

# Just memory latency, e.g. after changing timings (one thread, about 1-2 seconds)
lsa cachemem --only mem --tests latency

# One cheap cell, to check the program works
lsa cachemem --only l1 --tests read

# Compare streaming-store Memory Write with the CLZERO figure (AMD)
lsa cachemem --only mem --tests write
lsa cachemem --only mem --tests write --mem-write stream

# Compare code paths
lsa cachemem --only l1,l2 --isa avx2

# A smaller memory block (e.g. a PC with little free RAM)
lsa cachemem --mem-mib 512

# Run 3 times and keep all results
for i in 1 2 3; do lsa cachemem --yes >> runs.txt; sleep 30; done
```

## `lsa guide [ITEM] [--print]`

Opens the user guide at the section for one value, the same as right-clicking it in the window.

| Command | What it does |
|---|---|
| `lsa guide` | Opens the guide at the start. |
| `lsa guide ITEM` | Opens the guide at ITEM's section, e.g. `lsa guide mem-latency`, `lsa guide cpu-fsb`. |
| `lsa guide ITEM --print` | Shows which section and which command would be used, without opening anything. |
| `lsa guide --list` | Lists the item names: `mem-read` ... `l3-latency` for the 16 cells, `cpu-type`, `cpu-fsb`, `nb-clock` and so on for the lines below the grid, and `code-path`. |

The guide opens in the PDF viewer if it can jump to a section (Okular, Evince, Papers), otherwise
the HTML version opens in the web browser. It is looked for in `$LSA_DOC_DIR`,
`/usr/share/doc/lsa-sysinfo` and beside the program. On Windows, `lsa.exe guide` opens
`LSA-User-Guide.html` from the program's folder (at the start; Windows does not pass the section on).

```
$ lsa guide nb-clock --print
Item     nb-clock
Section  results--north-bridge-clock
Command  'okular' '/usr/share/doc/lsa-sysinfo/LSA-User-Guide.pdf#results--north-bridge-clock'
```

## Shell aliases named `lsa`

Some shell setups (Oh My Zsh, some fish configurations) define `lsa` as an alias for `ls -a`.
If `lsa --version` prints a file list, run `command lsa ...` or `/usr/bin/lsa ...`, or remove the
alias.

## The window's command-line switches

| Command | What it does |
|---|---|
| `lsa-gui` | Opens the Cache & Memory Benchmark window. |
| `lsa-gui --gui-start` | Opens the window and starts a run at once (the confirmation dialog still appears). |
| `lsa-gui cachemem ...` | Same as `lsa cachemem ...` (no window). |
