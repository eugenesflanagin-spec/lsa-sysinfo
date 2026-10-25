/* guide.c - Linux System Audit: open the user guide at the section for a result
 * (lsa-gui Guide button / F1 / right-click on a value, "lsa guide ITEM").
 *
 * Every value LSA shows has an item id (g_items below: the 16 grid cells and the system lines).
 * docs/gen_guide_map.py maps each id to its "## Title" section in docs/RESULTS.md and writes
 * guide_map.inc; "make check-guide" fails when an item has no section. Each anchor is a named
 * destination in LSA-User-Guide.pdf and an element id in html/LSA-User-Guide.html.
 * Linux: the PDF opens in the desktop's viewer when it can jump to a named destination
 * (Okular: file.pdf#dest, Evince / Papers: --named-dest=dest), otherwise the HTML build opens in
 * the default browser (file:///...html#dest).
 * Search order: $LSA_DOC_DIR, /usr/share/doc/lsa-sysinfo, <exe>/../share/doc/lsa-sysinfo,
 * <exe>/docs, <exe>/../../docs (build tree: dist/native/ -> docs/).
 * Windows: LSA-User-Guide.html beside the program, opened in the default browser. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cachemem.h"
#include "lsa.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

/* GUIDE-ITEMS-BEGIN (read by docs/gen_guide_map.py: every id here needs a section) */
static const struct { const char *id, *title; } g_items[] = {
    { "mem-read", "Memory Read" },       { "mem-write", "Memory Write" },     { "mem-copy", "Memory Copy" },     { "mem-latency", "Memory Latency" },
    { "l1-read", "L1 Cache Read" },      { "l1-write", "L1 Cache Write" },    { "l1-copy", "L1 Cache Copy" },    { "l1-latency", "L1 Cache Latency" },
    { "l2-read", "L2 Cache Read" },      { "l2-write", "L2 Cache Write" },    { "l2-copy", "L2 Cache Copy" },    { "l2-latency", "L2 Cache Latency" },
    { "l3-read", "L3 Cache Read" },      { "l3-write", "L3 Cache Write" },    { "l3-copy", "L3 Cache Copy" },    { "l3-latency", "L3 Cache Latency" },
    { "cpu-type", "CPU Type" },          { "cpu-stepping", "CPU Stepping" },  { "cpu-clock", "CPU Clock" },
    { "cpu-fsb", "CPU FSB" },            { "cpu-multiplier", "CPU Multiplier" }, { "nb-clock", "North Bridge Clock" },
    { "memory-bus", "Memory Bus" },      { "dram-fsb", "DRAM:FSB Ratio" },    { "memory-type", "Memory Type" },
    { "chipset", "Chipset" },            { "motherboard", "Motherboard" },    { "bios", "BIOS Version" },
    { "code-path", "Code path" },
    { NULL, NULL }
};
/* GUIDE-ITEMS-END */

static const struct { const char *id, *anchor; } g_map[] = {
#include "guide_map.inc"
    { NULL, NULL }
};

const char *const lsa_info_item[LSA_NINFO] = { "cpu-type", "cpu-stepping", "cpu-clock", "cpu-fsb", "cpu-multiplier", "nb-clock",
    "memory-bus", "dram-fsb", "memory-type", "chipset", "motherboard", "bios", "code-path" };

const char *lsa_cell_item(int level, int test)
{
    return level >= 0 && level < CM_NLEVEL && test >= 0 && test < CM_NTEST ? g_items[level * CM_NTEST + test].id : NULL;
}

const char *lsa_guide_title(const char *id)
{
    int i;
    for (i = 0; id && g_items[i].id; i++) if (!strcmp(g_items[i].id, id)) return g_items[i].title;
    return NULL;
}

const char *lsa_guide_anchor(const char *id)
{
    int i;
    if (!id || !*id) return "user-guide";
    for (i = 0; g_map[i].id; i++) if (!strcmp(g_map[i].id, id)) return g_map[i].anchor;
    return NULL;
}

void lsa_guide_list(FILE *f)
{
    int i;
    for (i = 0; g_items[i].id; i++) fprintf(f, "%-16s %s\n", g_items[i].id, g_items[i].title);
}

static int is_file(const char *p) { struct stat st; return !stat(p, &st) && S_ISREG(st.st_mode); }

#ifdef _WIN32
int lsa_guide_open(const char *id, int dry_run, char *msg, size_t mn)
{
    char exe[MAX_PATH], html[MAX_PATH + 64], *s;
    const char *a = lsa_guide_anchor(id);
    *msg = 0;
    if (!a) { snprintf(msg, mn, "No guide section for item '%s' (lsa guide --list shows the item ids)", id); return 0; }
    if (!GetModuleFileNameA(NULL, exe, sizeof exe)) exe[0] = 0;
    if ((s = strrchr(exe, '\\'))) *s = 0;
    snprintf(html, sizeof html, "%s\\LSA-User-Guide.html", exe);
    if (!is_file(html)) { snprintf(msg, mn, "The user guide is not installed (looked for %s).", html); return 0; }
    /* the browser is started on the file itself (Windows drops #fragments of file paths) */
    if (dry_run) { snprintf(msg, mn, "open '%s' (section '%s')", html, a); return 1; }
    if ((INT_PTR)ShellExecuteA(NULL, "open", html, NULL, NULL, SW_SHOWNORMAL) <= 32) { snprintf(msg, mn, "cannot open %s", html); return 0; }
    if (id && *id) snprintf(msg, mn, "Opened the guide; the section is '%s'.", lsa_guide_title(id) ? lsa_guide_title(id) : a);
    return 1;
}
#else
/* directory holding LSA-User-Guide.pdf and/or html/LSA-User-Guide.html */
static int guide_dir(char *out, size_t n)
{
    char exe[PATH_MAX], cand[6][PATH_MAX + 32], t[PATH_MAX + 64];
    ssize_t l = readlink("/proc/self/exe", exe, sizeof exe - 1);
    int k = 0, i;
    const char *env = getenv("LSA_DOC_DIR");
    if (env && *env) snprintf(cand[k++], sizeof cand[0], "%s", env);
    snprintf(cand[k++], sizeof cand[0], "/usr/share/doc/lsa-sysinfo");
    if (l > 0) {
        exe[l] = 0;
        if (strrchr(exe, '/')) *strrchr(exe, '/') = 0;
        snprintf(cand[k++], sizeof cand[0], "%s/../share/doc/lsa-sysinfo", exe);
        snprintf(cand[k++], sizeof cand[0], "%s/docs", exe);
        snprintf(cand[k++], sizeof cand[0], "%s/../../docs", exe);
    }
    for (i = 0; i < k; i++) {
        snprintf(t, sizeof t, "%s/LSA-User-Guide.pdf", cand[i]);
        if (is_file(t)) break;
        snprintf(t, sizeof t, "%s/html/LSA-User-Guide.html", cand[i]);
        if (is_file(t)) break;
    }
    if (i == k) return 0;
    {
        char rp[PATH_MAX];   /* realpath() needs a PATH_MAX buffer (_FORTIFY_SOURCE aborts otherwise) */
        snprintf(out, n, "%s", realpath(cand[i], rp) ? rp : cand[i]);
    }
    return 1;
}

static int in_path(const char *prog)
{
    const char *p = getenv("PATH");
    char d[PATH_MAX];
    while (p && *p) {
        size_t l = strcspn(p, ":");
        snprintf(d, sizeof d, "%.*s/%s", (int)l, p, prog);
        if (!access(d, X_OK)) return 1;
        p += l + (p[l] == ':');
    }
    return 0;
}

/* default application for application/pdf (desktop file id), "" if unknown */
static void pdf_default(char *out, size_t n)
{
    FILE *p;
    *out = 0;
    if (!in_path("xdg-mime") || !(p = popen("xdg-mime query default application/pdf 2>/dev/null", "r"))) return;
    if (!fgets(out, (int)n, p)) *out = 0;
    out[strcspn(out, "\r\n")] = 0;
    pclose(p);
}

#define GARG (PATH_MAX + 128)
/* argv for opening the guide at the item's section. 1 ok, 0 no guide / unknown item (msg says why). */
static int guide_command(const char *id, char argv[4][GARG], char *msg, size_t mn)
{
    const char *a = lsa_guide_anchor(id);
    char dir[PATH_MAX], pdf[PATH_MAX + 32], html[PATH_MAX + 32], def[256];
    int have_pdf, have_html;
    *msg = 0;
    if (!a) { snprintf(msg, mn, "No guide section for item '%s' (lsa guide --list shows the item ids)", id); return 0; }
    if (!guide_dir(dir, sizeof dir)) {
        snprintf(msg, mn, "The user guide is not installed (looked for LSA-User-Guide.pdf / html/LSA-User-Guide.html in "
                          "/usr/share/doc/lsa-sysinfo and beside the program; LSA_DOC_DIR overrides).");
        return 0;
    }
    snprintf(pdf, sizeof pdf, "%s/LSA-User-Guide.pdf", dir);
    snprintf(html, sizeof html, "%s/html/LSA-User-Guide.html", dir);
    have_pdf = is_file(pdf);
    have_html = is_file(html);
    pdf_default(def, sizeof def);
    memset(argv, 0, 4 * sizeof argv[0]);
    if (have_pdf && strstr(def, "okular") && in_path("okular")) {
        snprintf(argv[0], GARG, "okular");
        snprintf(argv[1], GARG, "%s#%s", pdf, a);
    } else if (have_pdf && (strstr(def, "Evince") || strstr(def, "evince")) && in_path("evince")) {
        snprintf(argv[0], GARG, "evince");
        snprintf(argv[1], GARG, "--named-dest=%s", a);
        snprintf(argv[2], GARG, "%s", pdf);
    } else if (have_pdf && (strstr(def, "Papers") || strstr(def, "papers")) && in_path("papers")) {
        snprintf(argv[0], GARG, "papers");
        snprintf(argv[1], GARG, "--named-dest=%s", a);
        snprintf(argv[2], GARG, "%s", pdf);
    } else if (have_html) {
        snprintf(argv[0], GARG, "xdg-open");
        snprintf(argv[1], GARG, "file://%s#%s", html, a);
    } else {
        snprintf(argv[0], GARG, "xdg-open");
        snprintf(argv[1], GARG, "%s", pdf);
        snprintf(msg, mn, "The PDF viewer (%s) cannot jump to a section and the HTML guide is missing: opening the PDF at the start; "
                          "the section is '%s'.", *def ? def : "unknown", a);
    }
    return 1;
}

/* start the viewer detached (double fork: no zombie, survives the caller); with dry_run only
   put the command line into msg */
int lsa_guide_open(const char *id, int dry_run, char *msg, size_t mn)
{
    static char argv[4][GARG];
    char *av[5];
    pid_t pid;
    int i, st;
    if (!guide_command(id, argv, msg, mn)) return 0;
    if (dry_run) {
        size_t l = strlen(msg);
        for (i = 0; i < 4 && argv[i][0]; i++) {
            l += (size_t)snprintf(msg + l, l < mn ? mn - l : 0, "%s'%s'", i ? " " : (l ? "\n" : ""), argv[i]);
            if (l >= mn) break;
        }
        return 1;
    }
    for (i = 0; i < 4 && argv[i][0]; i++) av[i] = argv[i];
    av[i] = NULL;
    fflush(NULL);
    pid = fork();
    if (pid < 0) { snprintf(msg, mn, "cannot start %s", argv[0]); return 0; }
    if (pid == 0) {
        setsid();
        if (fork() == 0) { execvp(av[0], av); _exit(127); }
        _exit(0);
    }
    waitpid(pid, &st, 0);
    return 1;
}
#endif
