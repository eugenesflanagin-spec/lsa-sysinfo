/* cachemem.h - Linux System Audit: Cache & Memory benchmark engine.
 * Clean-room: written from the published description of a cache & memory benchmark
 * (Read / Write / Copy bandwidth and Latency of Memory, L1, L2, L3), not from any other
 * program's code. Bandwidth tests run one pinned thread per logical CPU; Latency is a
 * single-thread random pointer chase on 2 MB (transparent huge) pages. */
#ifndef CACHEMEM_H
#define CACHEMEM_H

enum { CM_MEM, CM_L1, CM_L2, CM_L3, CM_NLEVEL };
enum { CM_READ, CM_WRITE, CM_COPY, CM_LAT, CM_NTEST };
enum { CM_ISA_AUTO, CM_ISA_SSE2, CM_ISA_AVX2, CM_ISA_AVX512 };

typedef struct {
    int threads;          /* 0 = all logical CPUs */
    double seconds;       /* measuring time per test (default 1.0) */
    int isa;              /* CM_ISA_* */
    long mem_mib;         /* memory block, MiB, all threads together (0 = auto: 1024, capped by MemAvailable/4) */
    unsigned run_mask;    /* bit (level * CM_NTEST + test); 0 = everything */
    double mb;            /* bytes per "MB" in results (default 1e6) */
} cm_cfg;

typedef struct {
    double v[CM_NLEVEL][CM_NTEST];   /* MB/s for Read/Write/Copy, ns for Latency; 0 = not run */
    int threads, isa;
    long cache_kib[4];               /* L1d, L2, L3 per instance (index 1..3), from sysfs */
    double clock_mhz;                /* measured core clock during the run */
} cm_result;

/* called after every finished (or starting, when done == 0) cell; may run on a worker thread */
typedef void (*cm_cb)(int level, int test, int done, const cm_result *r, void *ud);

typedef struct {
    char cpu_type[160], cpu_stepping[96], cpu_clock[64], cpu_fsb[32], cpu_mult[32], nb_clock[64];
    char mem_bus[64], dram_ratio[32], mem_type[128], chipset[160], board[160], bios[160];
    char isa[32], threads[32], l1[48], l2[48], l3[48];
} cm_info;

void cm_cfg_default(cm_cfg *c);
int  cm_run(const cm_cfg *c, cm_result *r, cm_cb cb, void *ud, volatile int *cancel);  /* 0 ok, 1 cancelled, -1 error */
void cm_get_info(cm_info *i, double measured_mhz);
double cm_measure_mhz(void);
/* "65369 MB/s" for Memory; caches in GB/s: "2520.3 GB/s", "695.57 GB/s" (v is in MB/s) */
void cm_format_bw(char *b, unsigned long n, int level, double mbs, int mib);
const char *cm_isa_name(int isa);
int  cm_isa_best(void);
extern const char *const cm_level_name[CM_NLEVEL], *const cm_test_name[CM_NTEST];

#endif
