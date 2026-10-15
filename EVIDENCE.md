# How the AIDA64 specification was gathered (clean-room evidence)

AIDA64 is a commercial product of FinalWire Ltd. (aida64.com). **Linux System Audit
(package `lsa-sysinfo`)** is an independent clean-room implementation. It is not affiliated
with FinalWire. AIDA64 labels quoted below are evidence for the mapping only. The
implementation must use its own wording, data, and reference results.

Source: the user's own licensed AIDA64 Business 7.65 portable folder
(portable install) (version string seen in the user's
screenshots: `AIDA64 v7.65.7400 / BenchDLL 4.7.916.8-x64`). Gathered 2026-10-15. All work was
black-box and read-only:

* **Nothing in the folder was executed.** No wine was run. Nothing in the folder was modified.
* **No disassembly or decompilation** of any binary.
* `aida64.dat` (the encrypted hardware DB) was not opened, parsed or decrypted.
* No icons, bitmaps or images were extracted from the PE files.
* Every scratch output is in this directory.

## 1. Existing reports: none

`find -maxdepth 6` over the AIDA folder (incl. `drive_c`), `-maxdepth 3` over `<local path>`,
`~/Documents ~/Desktop ~/Downloads ~/sysreports` for `*.htm *.html *.txt *.mht *.csv *.xml` containing
"AIDA64". The search found 42 files: 41 `Language/lang_*.txt` translation modules and `license.txt`.
**None of them is a report.** `drive_c` is a bare Proton prefix with no AIDA output. The rpf's
`RemoteReportFolder` points at `C:\Users\Administrator\Documents\AIDA64 Reports`, which does not exist.
The folder also holds `LACT-stats-20250617-092511.csv`, which is a LACT log and not AIDA.
→ We have no field-level sample values for any page. A full report can be generated later by the user on their own licensed copy.

## 2. Plain-text files read (primary evidence)

| file | what it gave |
|---|---|
| `aida64.rpf` | Report profile `ReportProfile="AllPages"` with format `htm`. It has **117 `InfoPage="Group;Page"` lines** in 15 groups: Computer 8, Motherboard 8, Operating System 8, Server 7, Display 12, Multimedia 8, Storage 7, Network 9, DirectX 4, Devices 7, Software 8, Security 6, Config 7, Database 4, Benchmark 14. **This is the authoritative page tree.** |
| `Language/lang_hu.txt` (cp1250, maintained by the AIDA author; the de file has the same 3,564 keys) | `English=Translation` pairs. The **English keys are the complete UI vocabulary** and are grouped by `// section` comments: info menu (20), info menuitems (110), page descs (140, one-line description per page, used verbatim as `aida_content`), treeview content (21, DMI tree nodes), column captions (145), listview field ≈1,480 (field labels; the awk split it at a translator note, so "water pump" 819 + "listview field" 661), listview value (194), preferences (246), application menuitems (90), report wizard (89), audit manager (88), alerting (72), remote monitor (64), sensor icons (168), Portable Computer page, IPMI page, Certificates page, Benchmark results save & compare, and others. Dump: `lang_en_keys_by_section.txt`. |
| `aida64.ini` (1.18 MB, latin-1, CRLF) | Sections `[Generic]` (line 1), `[Layout]` (9445), `[RptLook]` (9470), `[Sum]` (9490). It holds **3,041 sensor item IDs** ×3 (`OSDItem_<ID>`, `VSBItem_<ID>`, `SensorIcon_<ID>`, 2,781 icon entries) with values like `<SHOW>1</SHOW><FONTNAME>…<FGCOLOR>…<TEXTSIZE>…<BOLD>…<ITALIC>`. It has 41 `LowLevel*` sensor-backend switches (MSR PCI SMBus Sensor SMART RAID ×5, NVDRV, ACPISMBus, AppleSMC, Aquaero, ATKEX, AsusWMI, DellSMI, EC, HIDUPS, Koolance, KrakenX2, TBalancer, HM2, AXi, HXi, HydroPt, CorsairLink, AsetekLC, GRIDPlusV2, DIMMTS, TTDPS, RiingPlus, TVALZ, Volterra, GPUSensor/Fan, MultiGPU, WakeGPU, W11CPUUtiWA). There are about 80 `HWMonEnable??LCD` vendor LCD switches. The other settings are SensorPanel (`HWMonSP*`, 29 `HWMonSPItem`), OSD, VSB, RGB LED, logging (`HWMonLogToHTM/CSV`, `LogItems`), external apps (SharedMem, Registry, WMI, RTSS), alerting (14 `AlertTrigger*`, 11 `AlertMethod*`), 18 `HotKey*`, Report/Email/SMTP/FTP/ADO (DB), Remote (ports and passwords I/M/R/C, `RC*` remote-control), FileScanner/FileProp, ELFilter (event-log filter), and `[Sum]` (= **Summary page field layout**: group_computer…group_dmi, group_customvars). Dumps: `ini.u8`, `ini_other_keys.txt`, `aida_sensor_ids.txt`, `aida_sensor_families.txt` (1,380 families after stripping indices). |
| `aida64.sst.ini` | System Stability Test settings. `MemAlloc=95`, `UseAVX/UseAVX512`, `CPUMaskAuto/CPUMask`, `UpdateFreqSensor/CPUUsage`, and graph slots `Temp/Fan/Volt/Curr/Pwr/Flow/LiqL/Clk/Uni 01–14` + `Min/Max/Thick` per class. The selected IDs include TMOBO TCPU THDD1 FCPU FCHA2 VCPU V33V VP5V VP12V VDIMM PCPUPKG SCPUCLK SNBCLK SCC-1-1..8 TCPUDIO. |
| `aida64.mondiag.ini` | `AutoModeInterval=4 MultiMon=True SaveSelected=True`. |
| `aida64.customvars.ini` | 0 bytes. |
| `aida64.mem` | Not text. It is a UPX-packed DOS MZ executable (header only seen via `cat`). It was not analysed further. |
| `SQL_Schema/DB - *.sql` | Report DB schema: `Report(ID,RVersion,RHost,RHostComment,RUser,RLocation,RDateTime,RComplete)`, `Item(INum,IPage,IDevice,IGroup,IField,IValue,IIcon,IID,ReportID)`, `NextID`. This is the canonical **Page → Device → Group → Field → Value** report model. |

Sensor-ID prefix letters (from `aida_sensor_ids.txt`, with counts):

* T = temperature (934)
* V = voltage (490)
* S = system/software value: clocks, utilisation, memory, disk, NIC, GPU clocks/usage, time (809)
* P = power (290)
* C = current (264)
* F = fan RPM (188)
* D = duty/PWM % (42)
* W = flow (20)
* L = liquid level (4)

Linux sources: hwmon `temp*/in*/fan*/curr*/power*/pwm*`, `/proc`, drm/amdgpu sysfs, nvme/drivetemp, spd5118.

## 3. PE resources and strings

`wrestool -l` and `wrestool -x -R --type=6` (RT_STRING) and `--type=5` (RT_DIALOG) were run on
`aida64.exe`, `aida_rcs.dll` and `aida_rcc.dll`.

* `aida64.exe` has 55 RT_STRING, 1 RT_DIALOG and 232 RCDATA resources.
* `aida_uires.dll` has 410 RCDATA resources. Their names show they are gauge and icon images, which were not extracted.
* **The extracted string-table and dialog blobs are high-entropy, non-text data.** Decoding them as UTF-16 gave 0 strings. They were discarded and no decryption was attempted.
* Four Delphi form resources (`TFORM_STABILITYTEST`, `TFORM_DISKBENCH`, `TFORM_CACHEMEMBENCHSUITE`, `TFORM_GPGPUBENCHSUITE`) were sampled. They are also opaque and were discarded.
* There is no RT_MENU.
* `strings` on `aida64.exe` gives noise apart from resource names.

**Only the RCDATA resource *names*** (from `wrestool -l`) were used. They give the tool-window inventory: `TFORM_*` (about 100 forms, e.g. ACPITOOL, AUDITMANAGER, BENCHRESULTSMANAGER, CACHEMEMBENCHSUITE, CACHEMEMLATENCY, CHANGEMANAGER, CNS_* remote, CPUCLOCKMONITOR, CPUID, DATABASEMANAGER, DEBUGDUMP, DISKBENCH, DRAMTIMINGS, GPGPUBENCHSUITE(_RESULTS), HWMONITORINGOSD, HWMONITORINGSENSORPANEL, MONDIAG(_PREFERENCES/_SECONDARY/_TESTSCREEN), PERFMON, PREFERENCES_*, PSTATESMONITOR(_CPUBENCH), REPORTWIZARD1–6, REPORTCONVERTER, SCALABILITY, SENSORPANEL{DEFAULTS,ITEM,MANAGER}, STABILITYTEST(_PREFERENCES), STATISTICS{DETAILS,DIAGRAM}, SYSLOG, TWEAKINGS(_RDMSR/_WRMSR)). Rows that rest on a form name alone are marked "name only" in the checklist.

`strings -n5 aida_bench64.dll` gave readable symbol-style names (dump: `bench64_strings.txt`), which map the benchmark engine families:

* `Bench_MTMBW/MTMBW2` (memory bandwidth)
* `Bench_NGMemLat`
* `Bench_NGQueen`
* `Bench_NGImage2D`
* `Bench_NGPack` + `Base_ZLib_1_2_5`
* `Bench_NGAES`
* `Bench_Hash/Hash2`
* `Bench_NGFractal`
* `Bench_RayTrace`
* `Bench_InstLat/NGInstLat`, which comes with about 3,160 x86 instruction-form strings (an instruction latency dump)
* `Bench_SST/SST2` (stability test: `SST2MemPercent`, `SST2ProcMask`, `SST2MinRunTime`, …)

The pairings "CPU CheckMate ↔ NGQueen" and "SHA3 ↔ Hash2" are **inferred** from the page descriptions. They were not verified.

**Command-line switches could not be recovered.** The only evidence is the menu label "Co&mmand-line Options". The `.desktop` shortcut runs `wine aida64.exe` with no arguments. The `/R /ALL /TEXT /HTML /CSV /SAFE` switches named in the task are unverified. Confirm them from AIDA's own Help → Command-line Options.

## 4. User screenshots (viewed as images; all were taken under Proton on the previous ASRock B650E Taichi Lite / Ryzen 7 7800X3D)

**Cache & Memory Benchmark** (539×518, title "AIDA64 Cache & Memory Benchmark"):

* Grid: rows Memory, L1 Cache, L2 Cache and L3 Cache; columns Read, Write, Copy and Latency. Memory values are in MB/s, cache values in GB/s, latency in ns.
* Info rows: CPU Type, CPU Stepping, CPU Clock, CPU FSB, CPU Multiplier with North Bridge Clock, Memory Bus with DRAM:FSB Ratio, Memory Type, Chipset, Motherboard, BIOS Version.
* Footer: version and BenchDLL string. Buttons: Save, Start Benchmark, Close.

| shot | Mem R/W/C MB/s | Lat ns | L1 R/W/C GB/s | L2 | L3 | CPU clock | BIOS |
|---|---|---|---|---|---|---|---|
| cachemem.png | 65369 / 97640 / 65446 | 64.2 | 2520.3/1288.2/2535.0, 0.8 ns | 1287.9/1226.6/1200.8, 4.0 ns | 695.57/701.77/684.63, 11.0 ns | 4407.5 MHz | 3.40 |
| cachemembasictweak.png | 63066 / 78440 / 62379 | 68.0 | 2530.0/1288.9/2556.7, 0.8 | 1260.1/1234.2/1222.7, 4.0 | 680.04/707.14/702.34, 10.9 | 4409.9 | 3.50 |
| cachememnotweaks.png | 52413 / 68779 / 52585 | 88.9 | 2405.4/1233.4/2444.8, 0.8 | 1146.9/1125.9/1151.9, 4.3 | 675.82/684.62/622.17, 19.9 | 4200.0 | 3.50 |

CPU Type reads "OctalCore AMD Ryzen 7 7800X3D (Raphael, Socket AM5)" and stepping "RPL-B2". The blank FSB, Multiplier, Memory Bus, Memory Type and Chipset fields come from running under Proton without the kernel driver. They are not AIDA's normal behaviour.

**GPGPU Benchmark** (400×574):

* Each device has a checkbox and two description lines:
  * GPU line: "AMD Radeon RX 9070 XT (radeonsi, gfx1201, ACO, DRM 3.64, 6.19.0-…) / 2400 MHz, 64 CUs, Driver 26.0.0-devel (git-3d8286d7c8)", unchecked.
  * CPU line: "AMD Ryzen 7 7800X3D (Raphael) / 4660 MHz, 8 cores, 16 threads", checked.
* Column headers: "---" (the GPU slot, which is empty because Proton has no OpenCL) and "x86 CPU".
* There are 12 rows. Values in the x86 CPU column:

| row | x86 CPU |
|---|---|
| Memory Read | 62583 MB/s |
| Memory Write | 88214 MB/s |
| Memory Copy | 62802 MB/s |
| Single-Precision FLOPS | 1246 GFLOPS |
| Double-Precision FLOPS | 589.3 GFLOPS |
| 24-bit Integer IOPS | 1047 GIOPS |
| 32-bit Integer IOPS | 1071 GIOPS |
| 64-bit Integer IOPS | 529.5 GIOPS |
| AES-256 | 167830 MB/s |
| SHA-1 Hash | 32428 MB/s |
| Single-Precision Julia | 545.2 FPS |
| Double-Precision Mandel | 296.9 FPS |

* Buttons: Save, Results, Start Benchmark, Close.
* The lang file also has a "Tensor FLOPS" label, which suggests a newer extra row.

These numbers are the user's own measurements on old hardware. Use them only as a sanity range, not as reference data.

## 5. Outputs

* `aida_CHECKLIST.tsv` has the same columns as SRS (`id category title bucket group needs_root linux_source`) plus `aida_content lsa_overlap evidence`. It has 162 rows: 107 info pages (103 from the rpf and 4 lang-only), 20 benchmark rows (14 rpf pages, 3 legacy pages that have descriptions but are no longer in the rpf, 1 reference/compare row, Cache&Mem and Scalability) and 35 tool/monitoring rows.
* `lsa_overlap` names the existing SRS ids and `sec_*.c` files. "-" means it is new work.

## 6. Measurement conventions deduced from the screenshots (black-box arithmetic)

Only the published *results* in the user's own screenshots were used, against the documented
limits of the CPU (Zen 4: 2 x 256-bit loads and 1 x 256-bit store per clock per core; Infinity
Fabric 32 B/clk read and 16 B/clk write per CCD):

* **Copy = bytes read + bytes written.** L1 Copy 2535 GB/s on 8 cores is ~63 B/clk per core,
  above the 32 B/clk store limit; Memory Copy 65,446 MB/s is 2 x the ~32 GB/s fabric write limit.
* **Cache latency on 4 KB pages, memory latency without page walks.** L2 latency 4.0 ns (~20
  cycles) = L2's ~14 cycles + a first-level TLB miss; memory 64.2 ns is only reachable without
  page-walk penalties.
* **Units**: memory in MB/s (integer), caches in GB/s (1 or 2 decimals), latency in ns (1 decimal).

Linux System Audit's own code was then written to those conventions; its numbers are verified by
side-by-side runs on the same machine, not copied.
