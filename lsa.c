/* lsa.c - Linux System Audit command line.
 * Clean-room Linux implementation of AIDA64 Business features; AIDA64 is by FinalWire Ltd.
 * (https://www.aida64.com). Linux System Audit is not affiliated with FinalWire. */
#include "cachemem.h"
#include "lsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("usage: lsa cachemem [--threads N] [--seconds S] [--isa avx512|avx2|sse2] [--mem-mib M]\n"
         "                    [--only mem|l1|l2|l3[,..]] [--tests read,write,copy,latency] [--mib]\n"
         "                    [--mem-write auto|clzero|stream]\n"
         "       lsa --version\n\n"
         "cachemem  Cache & Memory benchmark: Read / Write / Copy (MB/s) and Latency (ns) of\n"
         "          Memory, L1, L2 and L3. Uses every CPU thread and the memory bus for ~20 s.");
}

static void progress(int lv, int te, int done, const cm_result *r, void *ud)
{
    (void)ud;
    if (!done) { fprintf(stderr, "  %-8s %-7s ...\r", cm_level_name[lv], cm_test_name[te]); return; }
    fprintf(stderr, "  %-8s %-7s %12.*f %s\n", cm_level_name[lv], cm_test_name[te], te == CM_LAT ? 1 : 0, r->v[lv][te], te == CM_LAT ? "ns" : "MB/s");
}

static int cmd_cachemem(int argc, char **argv)
{
    cm_cfg c;
    cm_result r;
    cm_info in;
    unsigned lvmask = 0, temask = 0;
    int i, lv, te, rc;
    cm_cfg_default(&c);
    for (i = 0; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(a, "--threads")) { c.threads = atoi(v); i++; }
        else if (!strcmp(a, "--seconds")) { c.seconds = atof(v); i++; }
        else if (!strcmp(a, "--mem-mib")) { c.mem_mib = atol(v); i++; }
        else if (!strcmp(a, "--mib")) c.mb = 1048576.0;
        else if (!strcmp(a, "--mem-write")) { c.mem_write = !strcmp(v, "stream") ? CM_MW_STREAM : !strcmp(v, "clzero") ? CM_MW_CLZERO : CM_MW_AUTO; i++; }
        else if (!strcmp(a, "--isa")) { c.isa = !strcmp(v, "avx512") ? CM_ISA_AVX512 : !strcmp(v, "avx2") ? CM_ISA_AVX2 : CM_ISA_SSE2; i++; }
        else if (!strcmp(a, "--only")) {
            lvmask |= strstr(v, "mem") ? 1u << CM_MEM : 0; lvmask |= strstr(v, "l1") ? 1u << CM_L1 : 0;
            lvmask |= strstr(v, "l2") ? 1u << CM_L2 : 0; lvmask |= strstr(v, "l3") ? 1u << CM_L3 : 0; i++;
        } else if (!strcmp(a, "--tests")) {
            temask |= strstr(v, "read") ? 1u << CM_READ : 0; temask |= strstr(v, "write") ? 1u << CM_WRITE : 0;
            temask |= strstr(v, "copy") ? 1u << CM_COPY : 0; temask |= strstr(v, "lat") ? 1u << CM_LAT : 0; i++;
        } else { usage(); return 2; }
    }
    if (lvmask || temask) {
        if (!lvmask) lvmask = 0xF;
        if (!temask) temask = 0xF;
        for (lv = 0; lv < CM_NLEVEL; lv++)
            for (te = 0; te < CM_NTEST; te++)
                if ((lvmask >> lv & 1) && (temask >> te & 1)) c.run_mask |= 1u << (lv * CM_NTEST + te);
    }
    rc = cm_run(&c, &r, progress, NULL, NULL);
    if (rc < 0) { fprintf(stderr, "cachemem: could not allocate the test memory\n"); return 1; }
    cm_get_info(&in, r.clock_mhz);
    printf("Linux System Audit %s - Cache & Memory Benchmark\n\n", LSA_VERSION);
    printf("%-10s %14s %14s %14s %12s\n", "", "Read", "Write", "Copy", "Latency");
    for (lv = 0; lv < CM_NLEVEL; lv++) {
        printf("%-10s", cm_level_name[lv]);
        for (te = 0; te < CM_NTEST; te++) {
            char b[32];
            if (r.v[lv][te] <= 0) snprintf(b, sizeof b, "-");
            else if (te == CM_LAT) snprintf(b, sizeof b, "%.1f ns", r.v[lv][te]);
            else cm_format_bw(b, sizeof b, lv, r.v[lv][te], c.mb > 1e6);
            printf(" %*s", te == CM_LAT ? 12 : 14, b);
        }
        putchar('\n');
    }
    printf("\nCPU Type         %s\nCPU Stepping     %s\nCPU Clock        %s\nCPU FSB          %s  (multiplier %s)\n"
           "NB Clock         %s\nMemory Bus       %s  %s\nMemory Type      %s\nChipset          %s\n"
           "Motherboard      %s\nBIOS Version     %s\nCode path        %s, %d threads; L1d %ld KB, L2 %ld KB, L3 %ld KB%s\n",
           in.cpu_type, in.cpu_stepping, in.cpu_clock, in.cpu_fsb, in.cpu_mult, in.nb_clock, in.mem_bus, in.dram_ratio,
           in.mem_type, in.chipset, in.board, in.bios, cm_isa_name(r.isa), r.threads, r.cache_kib[1], r.cache_kib[2], r.cache_kib[3],
           r.mem_write == CM_MW_CLZERO ? "; Memory Write: CLZERO, one thread per core" : r.mem_write == CM_MW_STREAM ? "; Memory Write: streaming stores" : "");
    return 0;
}

int lsa_cli_main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) { usage(); return argc < 2 ? 2 : 0; }
    if (!strcmp(argv[1], "--version")) {
        printf("Linux System Audit (lsa) %s\n%s\n", LSA_VERSION, LSA_CREDIT);
        return 0;
    }
    if (!strcmp(argv[1], "cachemem")) return cmd_cachemem(argc - 2, argv + 2);
    usage();
    return 2;
}

#ifndef LSA_NO_MAIN
int main(int argc, char **argv) { return lsa_cli_main(argc, argv); }
#endif
