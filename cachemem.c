/* cachemem.c - Linux System Audit: Cache & Memory benchmark (see cachemem.h). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cachemem.h"
#include <cpuid.h>
#include <immintrin.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

const char *const cm_level_name[CM_NLEVEL] = { "Memory", "L1 Cache", "L2 Cache", "L3 Cache" };
const char *const cm_test_name[CM_NTEST] = { "Read", "Write", "Copy", "Latency" };

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* ---------------------------------------------------------------- kernels
 * n is a multiple of 256 bytes, buffers 64-byte aligned. Read ORs every loaded vector into
 * four accumulators (so no load can be optimised away) and stores the result to `sink`. */
typedef void (*kfn)(char *dst, const char *src, size_t n, char *sink);

__attribute__((target("avx512f"))) static void rd_512(char *d, const char *s, size_t n, char *sink)
{
    __m512i a = _mm512_setzero_si512(), b = a, c = a, e = a;
    size_t i;
    (void)d;
    for (i = 0; i < n; i += 256) {
        a = _mm512_or_si512(a, _mm512_load_si512((const void *)(s + i)));
        b = _mm512_or_si512(b, _mm512_load_si512((const void *)(s + i + 64)));
        c = _mm512_or_si512(c, _mm512_load_si512((const void *)(s + i + 128)));
        e = _mm512_or_si512(e, _mm512_load_si512((const void *)(s + i + 192)));
    }
    _mm512_storeu_si512((void *)sink, _mm512_or_si512(_mm512_or_si512(a, b), _mm512_or_si512(c, e)));
}
__attribute__((target("avx512f"))) static void wr_512(char *d, const char *s, size_t n, char *sink)
{
    __m512i v = _mm512_set1_epi32(0x5A5A5A5A);
    size_t i;
    (void)s; (void)sink;
    for (i = 0; i < n; i += 256) {
        _mm512_store_si512((void *)(d + i), v); _mm512_store_si512((void *)(d + i + 64), v);
        _mm512_store_si512((void *)(d + i + 128), v); _mm512_store_si512((void *)(d + i + 192), v);
    }
}
__attribute__((target("avx512f"))) static void wrnt_512(char *d, const char *s, size_t n, char *sink)
{
    __m512i v = _mm512_set1_epi32(0x5A5A5A5A);
    size_t i;
    (void)s; (void)sink;
    for (i = 0; i < n; i += 256) {
        _mm512_stream_si512((void *)(d + i), v); _mm512_stream_si512((void *)(d + i + 64), v);
        _mm512_stream_si512((void *)(d + i + 128), v); _mm512_stream_si512((void *)(d + i + 192), v);
    }
    _mm_sfence();
}
__attribute__((target("avx512f"))) static void cp_512(char *d, const char *s, size_t n, char *sink)
{
    size_t i;
    (void)sink;
    for (i = 0; i < n; i += 256) {
        __m512i a = _mm512_load_si512((const void *)(s + i)), b = _mm512_load_si512((const void *)(s + i + 64));
        __m512i c = _mm512_load_si512((const void *)(s + i + 128)), e = _mm512_load_si512((const void *)(s + i + 192));
        _mm512_store_si512((void *)(d + i), a); _mm512_store_si512((void *)(d + i + 64), b);
        _mm512_store_si512((void *)(d + i + 128), c); _mm512_store_si512((void *)(d + i + 192), e);
    }
}
__attribute__((target("avx512f"))) static void cpnt_512(char *d, const char *s, size_t n, char *sink)
{
    size_t i;
    (void)sink;
    for (i = 0; i < n; i += 256) {
        __m512i a = _mm512_load_si512((const void *)(s + i)), b = _mm512_load_si512((const void *)(s + i + 64));
        __m512i c = _mm512_load_si512((const void *)(s + i + 128)), e = _mm512_load_si512((const void *)(s + i + 192));
        _mm512_stream_si512((void *)(d + i), a); _mm512_stream_si512((void *)(d + i + 64), b);
        _mm512_stream_si512((void *)(d + i + 128), c); _mm512_stream_si512((void *)(d + i + 192), e);
    }
    _mm_sfence();
}

#define AVX2_LOOP(BODY) for (i = 0; i < n; i += 256) { BODY(0) BODY(32) BODY(64) BODY(96) BODY(128) BODY(160) BODY(192) BODY(224) }
__attribute__((target("avx2"))) static void rd_256(char *d, const char *s, size_t n, char *sink)
{
    __m256i a = _mm256_setzero_si256(), b = a, c = a, e = a;
    size_t i;
    (void)d;
    for (i = 0; i < n; i += 256) {
        a = _mm256_or_si256(a, _mm256_load_si256((const void *)(s + i)));       b = _mm256_or_si256(b, _mm256_load_si256((const void *)(s + i + 32)));
        c = _mm256_or_si256(c, _mm256_load_si256((const void *)(s + i + 64)));  e = _mm256_or_si256(e, _mm256_load_si256((const void *)(s + i + 96)));
        a = _mm256_or_si256(a, _mm256_load_si256((const void *)(s + i + 128))); b = _mm256_or_si256(b, _mm256_load_si256((const void *)(s + i + 160)));
        c = _mm256_or_si256(c, _mm256_load_si256((const void *)(s + i + 192))); e = _mm256_or_si256(e, _mm256_load_si256((const void *)(s + i + 224)));
    }
    _mm256_storeu_si256((void *)sink, _mm256_or_si256(_mm256_or_si256(a, b), _mm256_or_si256(c, e)));
}
#define W256(o) _mm256_store_si256((void *)(d + i + (o)), v);
#define WNT256(o) _mm256_stream_si256((void *)(d + i + (o)), v);
#define C256(o) _mm256_store_si256((void *)(d + i + (o)), _mm256_load_si256((const void *)(s + i + (o))));
#define CNT256(o) _mm256_stream_si256((void *)(d + i + (o)), _mm256_load_si256((const void *)(s + i + (o))));
__attribute__((target("avx2"))) static void wr_256(char *d, const char *s, size_t n, char *sink)
{ __m256i v = _mm256_set1_epi32(0x5A5A5A5A); size_t i; (void)s; (void)sink; AVX2_LOOP(W256) }
__attribute__((target("avx2"))) static void wrnt_256(char *d, const char *s, size_t n, char *sink)
{ __m256i v = _mm256_set1_epi32(0x5A5A5A5A); size_t i; (void)s; (void)sink; AVX2_LOOP(WNT256) _mm_sfence(); }
__attribute__((target("avx2"))) static void cp_256(char *d, const char *s, size_t n, char *sink)
{ size_t i; (void)sink; AVX2_LOOP(C256) }
__attribute__((target("avx2"))) static void cpnt_256(char *d, const char *s, size_t n, char *sink)
{ size_t i; (void)sink; AVX2_LOOP(CNT256) _mm_sfence(); }

#define SSE_LOOP(BODY) for (i = 0; i < n; i += 256) { int k; for (k = 0; k < 256; k += 64) { BODY(k) BODY(k + 16) BODY(k + 32) BODY(k + 48) } }
static void rd_128(char *d, const char *s, size_t n, char *sink)
{
    __m128i a = _mm_setzero_si128(), b = a, c = a, e = a;
    size_t i;
    (void)d;
    for (i = 0; i < n; i += 64) {
        a = _mm_or_si128(a, _mm_load_si128((const void *)(s + i)));      b = _mm_or_si128(b, _mm_load_si128((const void *)(s + i + 16)));
        c = _mm_or_si128(c, _mm_load_si128((const void *)(s + i + 32))); e = _mm_or_si128(e, _mm_load_si128((const void *)(s + i + 48)));
    }
    _mm_storeu_si128((void *)sink, _mm_or_si128(_mm_or_si128(a, b), _mm_or_si128(c, e)));
}
#define W128(o) _mm_store_si128((void *)(d + i + (o)), v);
#define WNT128(o) _mm_stream_si128((void *)(d + i + (o)), v);
#define C128(o) _mm_store_si128((void *)(d + i + (o)), _mm_load_si128((const void *)(s + i + (o))));
#define CNT128(o) _mm_stream_si128((void *)(d + i + (o)), _mm_load_si128((const void *)(s + i + (o))));
static void wr_128(char *d, const char *s, size_t n, char *sink) { __m128i v = _mm_set1_epi32(0x5A5A5A5A); size_t i; (void)s; (void)sink; SSE_LOOP(W128) }
static void wrnt_128(char *d, const char *s, size_t n, char *sink) { __m128i v = _mm_set1_epi32(0x5A5A5A5A); size_t i; (void)s; (void)sink; SSE_LOOP(WNT128) _mm_sfence(); }
static void cp_128(char *d, const char *s, size_t n, char *sink) { size_t i; (void)sink; SSE_LOOP(C128) }
static void cpnt_128(char *d, const char *s, size_t n, char *sink) { size_t i; (void)sink; SSE_LOOP(CNT128) _mm_sfence(); }

int cm_isa_best(void)
{
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512f")) return CM_ISA_AVX512;
    if (__builtin_cpu_supports("avx2")) return CM_ISA_AVX2;
    return CM_ISA_SSE2;
}

void cm_format_bw(char *b, unsigned long n, int level, double v, int mib)
{
    if (v <= 0) { snprintf(b, n, "-"); return; }
    if (level == CM_MEM || v < 1000) snprintf(b, n, "%.0f %s", v, mib ? "MiB/s" : "MB/s");
    else if (v < 1e6) snprintf(b, n, "%.2f %s", v / 1000, mib ? "GiB/s" : "GB/s");
    else snprintf(b, n, "%.1f %s", v / 1000, mib ? "GiB/s" : "GB/s");
}

const char *cm_isa_name(int isa)
{
    return isa == CM_ISA_AVX512 ? "AVX-512" : isa == CM_ISA_AVX2 ? "AVX2" : isa == CM_ISA_SSE2 ? "SSE2" : "auto";
}

/* memory: non-temporal (streaming) stores bypass the caches; caches: ordinary stores */
static kfn pick(int isa, int test, int mem)
{
    static const kfn t[3][5] = {
        { rd_128, wr_128, wrnt_128, cp_128, cpnt_128 },
        { rd_256, wr_256, wrnt_256, cp_256, cpnt_256 },
        { rd_512, wr_512, wrnt_512, cp_512, cpnt_512 } };
    int r = isa == CM_ISA_AVX512 ? 2 : isa == CM_ISA_AVX2 ? 1 : 0;
    if (test == CM_READ) return t[r][0];
    if (test == CM_WRITE) return t[r][mem ? 2 : 1];
    return t[r][mem ? 4 : 3];
}

/* ---------------------------------------------------------------- topology */
static long cache_kib(int level)
{
    char p[128], buf[64];
    int idx;
    for (idx = 0; idx < 8; idx++) {
        FILE *f;
        int lv = 0;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        if (!(f = fopen(p, "r"))) break;
        if (fscanf(f, "%d", &lv) != 1) lv = 0;
        fclose(f);
        if (lv != level) continue;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/type", idx);
        if ((f = fopen(p, "r"))) { if (!fgets(buf, sizeof buf, f)) buf[0] = 0; fclose(f); if (!strncmp(buf, "Instruction", 11)) continue; }
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);
        if ((f = fopen(p, "r"))) {
            long v = 0;
            char u = 'K';
            if (fscanf(f, "%ld%c", &v, &u) < 1) v = 0;
            fclose(f);
            return u == 'M' ? v * 1024 : v;
        }
    }
    return level == 1 ? 32 : level == 2 ? 1024 : 8192;
}

/* logical CPUs sharing one instance of this cache level (cpu0's) */
static int cache_sharers(int level)
{
    char p[128], buf[256];
    int idx, n = 1;
    for (idx = 0; idx < 8; idx++) {
        FILE *f;
        int lv = 0;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        if (!(f = fopen(p, "r"))) break;
        if (fscanf(f, "%d", &lv) != 1) lv = 0;
        fclose(f);
        if (lv != level) continue;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/shared_cpu_list", idx);
        if ((f = fopen(p, "r"))) {
            if (fgets(buf, sizeof buf, f)) {   /* "0,8" / "0-15" / "0-3,16-19" */
                char *t = buf, *e;
                n = 0;
                while (*t && *t != '\n') {
                    long a = strtol(t, &e, 10), b = a;
                    if (e == t) break;
                    if (*e == '-') b = strtol(e + 1, &e, 10);
                    n += (int)(b - a + 1);
                    t = *e == ',' ? e + 1 : e;
                }
                if (n < 1) n = 1;
            }
            fclose(f);
        }
        break;
    }
    return n;
}

static int cpu_list(int *cpus, int max)
{
    cpu_set_t s;
    int i, n = 0;
    if (sched_getaffinity(0, sizeof s, &s)) { long c = sysconf(_SC_NPROCESSORS_ONLN); for (i = 0; i < c && i < max; i++) cpus[n++] = i; return n; }
    for (i = 0; i < CPU_SETSIZE && n < max; i++) if (CPU_ISSET(i, &s)) cpus[n++] = i;
    return n;
}

static void pin(int cpu)
{
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    sched_setaffinity(0, sizeof s, &s);
}

static void *alloc_pages(size_t bytes, int huge)
{
    size_t al = 2u << 20, sz = (bytes + al - 1) / al * al;
    void *p = NULL;
    if (posix_memalign(&p, al, sz)) return NULL;
    madvise(p, sz, huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
    return p;
}
static void *alloc_huge(size_t bytes) { return alloc_pages(bytes, 1); }

/* ---------------------------------------------------------------- bandwidth */
#define MAXT 512
typedef struct {
    int cpu, idx;
    char *buf;
    size_t bytes;           /* this thread's block */
    kfn fn;
    int test;
    long reps;
    double t0, t1;          /* this thread's own start / end of the last trial */
    char sink[64] __attribute__((aligned(64)));
} worker;

typedef struct {
    pthread_barrier_t start, done;
    worker w[MAXT];
    int n, quit;
} pool;

static void work_once(worker *w)
{
    size_t half = w->bytes / 2 / 256 * 256;
    long r;
    for (r = 0; r < w->reps; r++) {
        if (w->test == CM_COPY) w->fn(w->buf + half, w->buf, half, w->sink);
        else w->fn(w->buf, w->buf, w->bytes, w->sink);
    }
}

static void *worker_main(void *arg)
{
    worker *w = arg;
    pool *p = (pool *)((char *)(w - w->idx) - offsetof(pool, w));
    pin(w->cpu);
    for (;;) {
        pthread_barrier_wait(&p->start);
        if (p->quit) break;
        w->t0 = now_s();
        work_once(w);
        w->t1 = now_s();
        pthread_barrier_wait(&p->done);
    }
    return NULL;
}

/* one timed trial on every thread; returns seconds from the first thread's start to the last
   thread's end (each thread stamps its own times, so wake-up order cannot skew the result) */
static double trial(pool *p)
{
    double t0 = 1e300, t1 = 0;
    int i;
    pthread_barrier_wait(&p->start);
    pthread_barrier_wait(&p->done);
    for (i = 0; i < p->n; i++) {
        if (p->w[i].t0 < t0) t0 = p->w[i].t0;
        if (p->w[i].t1 > t1) t1 = p->w[i].t1;
    }
    return t1 - t0;
}

static double bandwidth(pool *p, int level, int test, int isa, size_t per_thread, double secs, volatile int *cancel)
{
    int i;
    double best = 0, t, spent = 0, bytes_per_rep;
    size_t useful;
    for (i = 0; i < p->n; i++) {
        worker *w = &p->w[i];
        w->bytes = per_thread / 256 * 256;
        w->fn = pick(isa, test, level == CM_MEM);
        w->test = test;
        w->reps = 1;
    }
    /* Copy is reported as bytes read + bytes written (2 x the bytes copied), the usual convention for
       this benchmark - it makes Copy comparable with Read and Write */
    useful = test == CM_COPY ? p->w[0].bytes / 2 / 256 * 256 * 2 : p->w[0].bytes;
    bytes_per_rep = (double)useful * p->n;
    t = trial(p);   /* warm-up */
    for (;;) {      /* calibrate: grow the work until one trial lasts >= 40 ms */
        long reps = p->w[0].reps;
        if (cancel && *cancel) return 0;
        t = trial(p);
        if (t >= 0.04 || reps > (1L << 40)) break;
        reps = t > 0.004 ? (long)(reps * 0.06 / t) + 1 : reps * 8;
        for (i = 0; i < p->n; i++) p->w[i].reps = reps;
    }
    while (spent < secs || best == 0) {
        double bw;
        if (cancel && *cancel) return 0;
        t = trial(p);
        spent += t;
        bw = bytes_per_rep * p->w[0].reps / t;
        if (bw > best) best = bw;
        if (spent > secs * 4) break;
    }
    return best;
}

/* ---------------------------------------------------------------- latency */
static uint64_t xs(uint64_t *s) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; return *s; }

/* random cyclic chain through every 64-byte line of `bytes` (Sattolo), ns per dependent load */
static double latency(char *buf, size_t bytes, double secs, volatile int *cancel)
{
    size_t lines = bytes / 64, i;
    uint32_t *perm = malloc(lines * sizeof *perm);
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    void **p;
    double best = 1e9, spent = 0;
    long steps;
    if (!perm || lines < 2) { free(perm); return 0; }
    for (i = 0; i < lines; i++) perm[i] = (uint32_t)i;
    for (i = lines - 1; i > 0; i--) { size_t j = xs(&seed) % i; uint32_t t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    for (i = 0; i < lines; i++) *(void **)(buf + (size_t)perm[i] * 64) = buf + (size_t)perm[(i + 1) % lines] * 64;
    free(perm);
    p = (void **)buf;
    for (i = 0; i < lines; i++) p = *p;   /* warm up: TLB + caches */
    steps = 1 << 16;
    for (;;) {   /* calibrate to ~50 ms */
        double t0 = now_s(), t;
        long k;
        for (k = 0; k < steps; k += 16) {
            p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p;
            p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p;
        }
        t = now_s() - t0;
        if (t > 0.05 || steps > (1L << 30)) break;
        steps *= 2;
    }
    while (spent < secs) {
        double t0 = now_s(), t;
        long k;
        if (cancel && *cancel) break;
        for (k = 0; k < steps; k += 16) {
            p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p;
            p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p; p = *p;
        }
        t = now_s() - t0;
        spent += t;
        if (t / steps * 1e9 < best) best = t / steps * 1e9;
    }
    __asm__ volatile("" : : "r"(p));
    return best;
}

/* ---------------------------------------------------------------- clock */
double cm_measure_mhz(void)
{
    /* 20 dependent 1-cycle adds per loop: cycles = 20 x iterations, whatever the turbo state */
    long it = 20000000, x = 0;
    double t0, t, best = 0;
    int r;
    for (r = 0; r < 3; r++) {
        long n = it;
        t0 = now_s();
        __asm__ volatile(
            "1:\n\t"
            "add $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\t"
            "add $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\t"
            "add $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\t"
            "add $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\tadd $1, %0\n\t"
            "dec %1\n\tjnz 1b" : "+r"(x), "+r"(n));
        t = now_s() - t0;
        if (t > 0 && 20.0 * it / t / 1e6 > best) best = 20.0 * it / t / 1e6;
    }
    return best;
}

/* ---------------------------------------------------------------- driver */
void cm_cfg_default(cm_cfg *c)
{
    memset(c, 0, sizeof *c);
    c->seconds = 1.0;
    c->isa = CM_ISA_AUTO;
    c->mb = 1e6;
}

static long mem_available_mib(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char l[256];
    long kb = 0;
    if (!f) return 0;
    while (fgets(l, sizeof l, f)) if (sscanf(l, "MemAvailable: %ld kB", &kb) == 1) break;
    fclose(f);
    return kb / 1024;
}

int cm_run(const cm_cfg *cfg, cm_result *r, cm_cb cb, void *ud, volatile int *cancel)
{
    static int cpus[MAXT];
    pool *p;
    int ncpu = cpu_list(cpus, MAXT), n, i, lv, te, rc = 0, isa = cfg->isa ? cfg->isa : cm_isa_best();
    long mem_mib = cfg->mem_mib;
    double mb = cfg->mb > 0 ? cfg->mb : 1e6;
    size_t per[CM_NLEVEL];
    char **bufs;
    pthread_t tid[MAXT];
    memset(r, 0, sizeof *r);
    n = cfg->threads > 0 && cfg->threads < ncpu ? cfg->threads : ncpu;
    if (n < 1) return -1;
    if (isa == CM_ISA_AVX512 && cm_isa_best() != CM_ISA_AVX512) isa = cm_isa_best();
    if (isa == CM_ISA_AVX2 && cm_isa_best() == CM_ISA_SSE2) isa = CM_ISA_SSE2;
    r->threads = n;
    r->isa = isa;
    for (i = 1; i <= 3; i++) r->cache_kib[i] = cache_kib(i);
    if (mem_mib <= 0) {
        long av = mem_available_mib() / 4;
        mem_mib = 1024;
        if (av > 0 && mem_mib > av) mem_mib = av;
        if (mem_mib < 64) mem_mib = 64;
    }
    {   /* the memory block must not fit in the caches (X3D parts have 96+ MB of L3) */
        long min_mib = r->cache_kib[3] * 4 / 1024;
        if (min_mib < 256) min_mib = 256;
        if (mem_mib < min_mib) mem_mib = min_mib;
    }
    /* per-thread block for each level: half of this thread's share of the cache, so it stays resident */
    per[CM_MEM] = (size_t)mem_mib * 1048576 / (size_t)n;
    per[CM_L1] = (size_t)r->cache_kib[1] * 1024 / (size_t)cache_sharers(1) / 2;
    per[CM_L2] = (size_t)r->cache_kib[2] * 1024 / (size_t)cache_sharers(2) / 2;
    {
        int sh3 = cache_sharers(3), per_inst = n < sh3 ? n : sh3;
        per[CM_L3] = (size_t)r->cache_kib[3] * 1024 / (size_t)(per_inst > 0 ? per_inst : 1) / 2;
        if (per[CM_L3] < per[CM_L2] * 4) per[CM_L3] = per[CM_L2] * 4;   /* must spill out of L2 */
    }
    for (lv = 0; lv < CM_NLEVEL; lv++) if (per[lv] < 4096) per[lv] = 4096;

    p = calloc(1, sizeof *p);
    bufs = calloc((size_t)n, sizeof *bufs);
    if (!p || !bufs) { free(p); free(bufs); return -1; }
    for (i = 0; i < n; i++) {
        bufs[i] = alloc_huge(per[CM_MEM] > per[CM_L3] ? per[CM_MEM] : per[CM_L3]);
        if (!bufs[i]) { rc = -1; n = i; break; }
    }
    p->n = n;
    if (rc == 0) {
        pthread_barrier_init(&p->start, NULL, (unsigned)n + 1);
        pthread_barrier_init(&p->done, NULL, (unsigned)n + 1);
        for (i = 0; i < n; i++) {
            p->w[i].idx = i;
            p->w[i].cpu = cpus[i];
            p->w[i].buf = bufs[i];
            /* first touch by the owning thread happens in the warm-up trial; pre-fault here once */
            memset(bufs[i], 1, per[CM_MEM] > per[CM_L3] ? per[CM_MEM] : per[CM_L3]);
            pthread_create(&tid[i], NULL, worker_main, &p->w[i]);
        }
        r->clock_mhz = cm_measure_mhz();
        for (lv = 0; lv < CM_NLEVEL && rc == 0; lv++)
            for (te = 0; te < CM_NTEST && rc == 0; te++) {
                unsigned bit = 1u << (lv * CM_NTEST + te);
                if (cfg->run_mask && !(cfg->run_mask & bit)) continue;
                if (cancel && *cancel) { rc = 1; break; }
                if (cb) cb(lv, te, 0, r, ud);
                if (te == CM_LAT) {
                    /* single thread on the first CPU; chain over the level's block size */
                    /* block per level: L1 = half of L1d; L2 = half of L2 (few L1 hits); L3 = 8 x L2 (few L2
                       hits); Memory = 4 x L3, at least 256 MB, so it cannot be cache-resident */
                    size_t l3 = (size_t)r->cache_kib[3] * 1024, bytes;
                    char *lb = bufs[0], *own = NULL;
                    cpu_set_t old;
                    if (lv == CM_L1) bytes = (size_t)r->cache_kib[1] * 1024 / 2;
                    else if (lv == CM_L2) bytes = (size_t)r->cache_kib[2] * 1024 / 2;
                    else if (lv == CM_L3) {   /* 8 x L2 (mostly L3 hits, still inside the second-level TLB's reach) */
                        bytes = (size_t)r->cache_kib[2] * 1024 * 8;
                        if (bytes > l3 / 2) bytes = l3 / 2;
                    }
                    else bytes = l3 * 4 > (256u << 20) ? l3 * 4 : (256u << 20);
                    /* cache latency on ordinary 4 KB pages (includes first-level TLB misses, as on a normal
                       desktop); memory latency on 2 MB pages so page walks do not dominate it */
                    own = alloc_pages(bytes, lv == CM_MEM);
                    if (own) { memset(own, 1, bytes); lb = own; }
                    else if (bytes > (per[CM_MEM] > per[CM_L3] ? per[CM_MEM] : per[CM_L3])) bytes = per[CM_MEM] > per[CM_L3] ? per[CM_MEM] : per[CM_L3];
                    sched_getaffinity(0, sizeof old, &old);
                    pin(cpus[0]);
                    r->v[lv][te] = latency(lb, bytes, cfg->seconds, cancel);
                    sched_setaffinity(0, sizeof old, &old);
                    free(own);
                } else {
                    r->v[lv][te] = bandwidth(p, lv, te, isa, per[lv], cfg->seconds, cancel) / mb;
                }
                if (cancel && *cancel) { r->v[lv][te] = 0; rc = 1; }
                if (cb) cb(lv, te, 1, r, ud);
            }
        p->quit = 1;
        pthread_barrier_wait(&p->start);
        for (i = 0; i < n; i++) pthread_join(tid[i], NULL);
        pthread_barrier_destroy(&p->start);
        pthread_barrier_destroy(&p->done);
    }
    for (i = 0; i < n; i++) free(bufs[i]);
    free(bufs);
    free(p);
    return rc;
}
