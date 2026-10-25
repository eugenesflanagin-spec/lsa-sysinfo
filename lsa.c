/* lsa.c - Linux System Audit command line.
 * Clean-room Linux implementation of AIDA64 Business features; AIDA64 is by FinalWire Ltd.
 * (https://www.aida64.com). Linux System Audit is not affiliated with FinalWire. */
#include "cachemem.h"
#include "lsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
    puts("usage: lsa cachemem [--threads N] [--seconds S] [--isa avx512|avx2|sse2] [--mem-mib M]\n"
         "                    [--only mem,l1,l2,l3] [--tests read,write,copy,latency] [--mib]\n"
         "                    [--mem-write auto|clzero|stream] [--yes]\n"
         "       lsa guide [ITEM] [--print] | lsa guide --list\n"
         "       lsa --version | --help\n\n"
         "cachemem  Cache & Memory benchmark: Read / Write / Copy (MB/s) and Latency (ns) of\n"
         "          Memory, L1, L2 and L3. Uses every CPU thread and the memory bus for ~20 s.\n"
         "          Asks before it starts; --yes (or -y) starts at once, e.g. in scripts.\n"
         "guide     Open the user guide at ITEM's section (e.g. mem-latency, cpu-fsb); --print shows\n"
         "          the command instead of running it; --list lists the items.");
}

/* "a,b,c" -> bit mask over names[]; 0 and an error message for an unknown word */
static int parse_list(const char *opt, const char *v, const char *const *names, const int *bits, const char *valid, unsigned *mask)
{
    char w[32];
    while (*v) {
        size_t l = strcspn(v, ","), k;
        int found = 0;
        snprintf(w, sizeof w, "%.*s", (int)(l < sizeof w - 1 ? l : sizeof w - 1), v);
        for (k = 0; names[k]; k++) if (!strcmp(w, names[k])) { *mask |= 1u << bits[k]; found = 1; }
        if (l && !found) { fprintf(stderr, "cachemem: unknown %s value '%s' (valid: %s)\n", opt, w, valid); return 0; }
        v += l + (v[l] == ',');
    }
    if (!*mask) { fprintf(stderr, "cachemem: %s needs a value (valid: %s)\n", opt, valid); return 0; }
    return 1;
}

static int confirm(void)
{
    char a[16];
    if (getenv("LSA_TEST_NOCONFIRM")) return 1;
    if (!isatty(0)) {
        fprintf(stderr, "cachemem: not started - the benchmark loads every CPU thread and the memory bus for about 20 s.\n"
                        "          Run it from a terminal to be asked, or pass --yes to start without asking.\n");
        return 0;
    }
    fprintf(stderr, "The Cache & Memory benchmark loads every CPU thread and the memory bus for about 20 seconds.\n"
                    "Close other heavy programs for accurate results. Start now? [y/N] ");
    fflush(stderr);
    if (fgets(a, sizeof a, stdin) && (a[0] == 'y' || a[0] == 'Y')) return 1;
    fprintf(stderr, "cachemem: not started\n");
    return 0;
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
    int i, lv, te, rc, yes = 0;
    cm_cfg_default(&c);
    for (i = 0; i < argc; i++) {
        static const char *const lvn[] = { "mem", "l1", "l2", "l3", NULL }, *const ten[] = { "read", "write", "copy", "latency", "lat", NULL };
        static const int lvb[] = { CM_MEM, CM_L1, CM_L2, CM_L3 }, teb[] = { CM_READ, CM_WRITE, CM_COPY, CM_LAT, CM_LAT };
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        int need = !strcmp(a, "--threads") || !strcmp(a, "--seconds") || !strcmp(a, "--mem-mib") || !strcmp(a, "--mem-write") ||
                   !strcmp(a, "--isa") || !strcmp(a, "--only") || !strcmp(a, "--tests");
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
        if (need && !v) { fprintf(stderr, "cachemem: %s needs a value\n", a); return 2; }
        if (need) i++;
        if (!strcmp(a, "--threads")) c.threads = atoi(v);
        else if (!strcmp(a, "--seconds")) c.seconds = atof(v);
        else if (!strcmp(a, "--mem-mib")) c.mem_mib = atol(v);
        else if (!strcmp(a, "--mib")) c.mb = 1048576.0;
        else if (!strcmp(a, "--yes") || !strcmp(a, "-y")) yes = 1;
        else if (!strcmp(a, "--mem-write")) {
            if (!strcmp(v, "stream")) c.mem_write = CM_MW_STREAM;
            else if (!strcmp(v, "clzero")) c.mem_write = CM_MW_CLZERO;
            else if (!strcmp(v, "auto")) c.mem_write = CM_MW_AUTO;
            else { fprintf(stderr, "cachemem: unknown --mem-write value '%s' (valid: auto, clzero, stream)\n", v); return 2; }
        } else if (!strcmp(a, "--isa")) {
            if (!strcmp(v, "avx512")) c.isa = CM_ISA_AVX512;
            else if (!strcmp(v, "avx2")) c.isa = CM_ISA_AVX2;
            else if (!strcmp(v, "sse2")) c.isa = CM_ISA_SSE2;
            else { fprintf(stderr, "cachemem: unknown --isa value '%s' (valid: avx512, avx2, sse2)\n", v); return 2; }
        } else if (!strcmp(a, "--only")) {
            if (!parse_list("--only", v, lvn, lvb, "mem, l1, l2, l3", &lvmask)) return 2;
        } else if (!strcmp(a, "--tests")) {
            if (!parse_list("--tests", v, ten, teb, "read, write, copy, latency (or lat)", &temask)) return 2;
        } else { fprintf(stderr, "cachemem: unknown option '%s'\n\n", a); usage(); return 2; }
    }
    if (lvmask || temask) {
        if (!lvmask) lvmask = 0xF;
        if (!temask) temask = 0xF;
        for (lv = 0; lv < CM_NLEVEL; lv++)
            for (te = 0; te < CM_NTEST; te++)
                if ((lvmask >> lv & 1) && (temask >> te & 1)) c.run_mask |= 1u << (lv * CM_NTEST + te);
    }
    if (!yes && !confirm()) return 2;
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

static int cmd_guide(int argc, char **argv)
{
    const char *id = NULL;
    char msg[8192];
    int i, dry = 0, ok;
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--print")) dry = 1;
        else if (!strcmp(argv[i], "--list")) { lsa_guide_list(stdout); return 0; }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (argv[i][0] == '-' || id) { fprintf(stderr, "usage: lsa guide [ITEM] [--print] | lsa guide --list\n"); return 2; }
        else id = argv[i];
    }
    if (id && !lsa_guide_title(id)) { fprintf(stderr, "guide: unknown item '%s' (lsa guide --list shows the items)\n", id); return 2; }
    ok = lsa_guide_open(id, dry, msg, sizeof msg);
    if (dry && ok) { printf("Item     %s\nSection  %s\nCommand  %s\n", id ? id : "(start)", lsa_guide_anchor(id), msg); return 0; }
    if (*msg) fprintf(stderr, "guide: %s\n", msg);
    return ok ? 0 : 1;
}

int lsa_cli_main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) { usage(); return argc < 2 ? 2 : 0; }
    if (!strcmp(argv[1], "--version")) {
        printf("Linux System Audit (lsa) %s\n%s\n", LSA_VERSION, LSA_CREDIT);
        return 0;
    }
    if (!strcmp(argv[1], "cachemem")) return cmd_cachemem(argc - 2, argv + 2);
    if (!strcmp(argv[1], "guide")) return cmd_guide(argc - 2, argv + 2);
    usage();
    return 2;
}

#ifndef LSA_NO_MAIN
int main(int argc, char **argv) { return lsa_cli_main(argc, argv); }
#endif
