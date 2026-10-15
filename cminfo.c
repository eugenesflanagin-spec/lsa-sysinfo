/* cminfo.c - Linux System Audit: the system lines shown under the Cache & Memory results
 * (CPU type / stepping / clock, memory bus and type, chipset, motherboard, BIOS).
 * Sources: /proc/cpuinfo, sysfs cache + DMI, hwdata pci.ids, and the root cache written by
 * srs-root (~/.cache/srs/root/dmidecode.txt) for DIMM speed / type when present. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cachemem.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void rd1(const char *path, char *out, size_t n)
{
    FILE *f = fopen(path, "r");
    out[0] = 0;
    if (!f) return;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\n")] = 0;
    fclose(f);
}

static void trim(char *s)
{
    size_t l = strlen(s);
    while (l && (s[l - 1] == ' ' || s[l - 1] == '\n' || s[l - 1] == '\t')) s[--l] = 0;
    while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
}

static const char *count_word(int n)
{
    static const char *const w[] = { "", "Single", "Dual", "Triple", "Quad", "Penta", "Hexa", "Hepta", "Octal",
        "", "Deca", "", "Dodeca", "", "Tetradeca", "", "Hexadeca" };
    return n > 0 && n < (int)(sizeof w / sizeof *w) && w[n][0] ? w[n] : NULL;
}

/* known code names (family, model range) - public CPUID data */
static const char *codename(const char *vendor, int fam, int model)
{
    if (strstr(vendor, "AMD")) {
        if (fam == 0x19 && model >= 0x60 && model <= 0x6f) return "Raphael";
        if (fam == 0x19 && model >= 0x70 && model <= 0x7f) return "Phoenix";
        if (fam == 0x19 && model >= 0x10 && model <= 0x1f) return "Genoa";
        if (fam == 0x19 && model >= 0x20 && model <= 0x2f) return "Vermeer";
        if (fam == 0x19 && model >= 0x50 && model <= 0x5f) return "Cezanne";
        if (fam == 0x19 && model >= 0x40 && model <= 0x4f) return "Rembrandt";
        if (fam == 0x1a && model >= 0x40 && model <= 0x4f) return "Granite Ridge";
        if (fam == 0x1a && model >= 0x20 && model <= 0x2f) return "Strix Point";
        if (fam == 0x17 && model == 0x71) return "Matisse";
        if (fam == 0x17 && model == 0x08) return "Pinnacle Ridge";
        if (fam == 0x17 && model == 0x01) return "Summit Ridge";
    } else if (strstr(vendor, "Intel") && fam == 6) {
        switch (model) {
        case 0xb7: case 0xba: case 0xbf: return "Raptor Lake";
        case 0x97: case 0x9a: return "Alder Lake";
        case 0xc6: return "Arrow Lake";
        case 0xa7: return "Rocket Lake";
        case 0xa5: return "Comet Lake";
        case 0x9e: return "Coffee Lake";
        }
    }
    return NULL;
}

/* name for vendor:device from hwdata pci.ids */
static int pci_name(unsigned ven, unsigned dev, char *out, size_t n)
{
    static const char *const files[] = { "/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", NULL };
    char l[512], vtag[8], dtag[8];
    int i, inv = 0;
    snprintf(vtag, sizeof vtag, "%04x", ven);
    snprintf(dtag, sizeof dtag, "\t%04x", dev);
    out[0] = 0;
    for (i = 0; files[i]; i++) {
        FILE *f = fopen(files[i], "r");
        if (!f) continue;
        while (fgets(l, sizeof l, f)) {
            if (l[0] != '\t' && l[0] != '#') { if (inv) break; inv = !strncmp(l, vtag, 4); continue; }
            if (inv && !strncmp(l, dtag, 5) && l[5] == ' ') { snprintf(out, n, "%s", l + 7); out[strcspn(out, "\n")] = 0; break; }
        }
        fclose(f);
        if (out[0]) return 1;
    }
    return 0;
}

static void chipset(char *out, size_t n)
{
    DIR *d = opendir("/sys/bus/pci/devices");
    struct dirent *e;
    char root[200] = "", hub[200] = "";
    out[0] = 0;
    if (!d) return;
    while ((e = readdir(d))) {
        char p[300], v[32], dv[32], cl[32], nm[200];
        unsigned ven, dev, cls;
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "/sys/bus/pci/devices/%s/vendor", e->d_name); rd1(p, v, sizeof v);
        snprintf(p, sizeof p, "/sys/bus/pci/devices/%s/device", e->d_name); rd1(p, dv, sizeof dv);
        snprintf(p, sizeof p, "/sys/bus/pci/devices/%s/class", e->d_name); rd1(p, cl, sizeof cl);
        ven = (unsigned)strtoul(v, NULL, 16); dev = (unsigned)strtoul(dv, NULL, 16); cls = (unsigned)strtoul(cl, NULL, 16);
        if (!pci_name(ven, dev, nm, sizeof nm)) continue;
        if (!strcmp(e->d_name, "0000:00:00.0")) snprintf(root, sizeof root, "%s", nm);
        if (!hub[0] && (cls >> 8) != 0x0600 && (strstr(nm, "Chipset") || strstr(nm, "Promontory") || strstr(nm, "PCH") || strstr(nm, "LPC Controller")))
            snprintf(hub, sizeof hub, "%s", nm);
    }
    closedir(d);
    {   /* "AMD X870, AMD Raphael/Granite Ridge": chipset model from the board name, CPU-side from the root complex */
        char bn[128], model[16] = "", rc[200] = "";
        const char *vend = strstr(root, "Intel") ? "Intel" : "AMD";
        char *p;
        size_t i;
        rd1("/sys/class/dmi/id/board_name", bn, sizeof bn);
        for (i = 0; bn[i]; i++)
            if (strchr("ABHQXZW", bn[i]) && bn[i + 1] >= '0' && bn[i + 1] <= '9' && bn[i + 2] >= '0' && bn[i + 2] <= '9' && bn[i + 3] >= '0' && bn[i + 3] <= '9'
                && (i == 0 || bn[i - 1] == ' ' || bn[i - 1] == '-')) {
                size_t k = 0;
                while (bn[i + k] && bn[i + k] != ' ' && bn[i + k] != '-' && k < sizeof model - 1) { model[k] = bn[i + k]; k++; }
                model[k] = 0;
                break;
            }
        snprintf(rc, sizeof rc, "%s", root);
        if ((p = strstr(rc, " Root Complex"))) *p = 0;
        if ((p = strstr(rc, " Host Bridge"))) *p = 0;
        if (model[0]) snprintf(out, n, "%s %s%s%s%s", vend, model, rc[0] ? ", " : "", rc[0] ? vend : "", rc[0] ? " " : "");
        else snprintf(out, n, "%s%s", hub[0] ? hub : "", hub[0] && rc[0] ? ", " : "");
        if (rc[0]) { size_t l = strlen(out); snprintf(out + l, n - l, "%s", rc); }
    }
}

/* DIMM speed / type / count from the srs-root dmidecode cache (dmidecode needs root) */
static void dimms(int *count, int *mts, char *type, size_t tn, int *channels)
{
    const char *h = getenv("SUDO_USER") ? NULL : getenv("HOME");
    char path[600], l[512], chan[8][32];
    FILE *f;
    int in17 = 0, size_ok = 0, nch = 0, i;
    char cur_bank[32] = "";
    *count = 0; *mts = 0; type[0] = 0; *channels = 0;
    if (!h) h = "/root";
    snprintf(path, sizeof path, "%s/.cache/srs/root/dmidecode.txt", h);
    if (!(f = fopen(path, "r"))) return;
    while (fgets(l, sizeof l, f)) {
        char *t = l;
        int v;
        while (*t == ' ' || *t == '\t') t++;
        if (!strncmp(l, "Handle ", 7)) { in17 = 0; continue; }
        if (!strncmp(t, "Memory Device", 13) && l[0] != '\t') { in17 = 1; size_ok = 0; cur_bank[0] = 0; continue; }
        if (!in17) continue;
        if (!strncmp(t, "Size:", 5)) size_ok = !strstr(t, "No Module") && !strstr(t, "Not Installed");
        else if (size_ok && sscanf(t, "Configured Memory Speed: %d MT/s", &v) == 1) { (*count)++; if (v > *mts) *mts = v; }
        else if (size_ok && !strncmp(t, "Type: ", 6) && strncmp(t + 6, "Unknown", 7)) { snprintf(type, tn, "%s", t + 6); trim(type); }
        else if (size_ok && !strncmp(t, "Bank Locator:", 13)) {
            char *c = strstr(t, "CHANNEL ");
            if (c) {
                snprintf(cur_bank, sizeof cur_bank, "%s", c + 8); trim(cur_bank);
                for (i = 0; i < nch; i++) if (!strcmp(chan[i], cur_bank)) break;
                if (i == nch && nch < 8) snprintf(chan[nch++], sizeof chan[0], "%s", cur_bank);
            }
        }
    }
    fclose(f);
    *channels = nch;
}

void cm_get_info(cm_info *in, double mhz)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    char l[512], model[160] = "", vendor[64] = "", v[160], v2[160];
    int fam = 0, mod = 0, step = 0, cores = 0, n, mts = 0, nch = 0;
    char type[32];
    memset(in, 0, sizeof *in);
    if (f) {
        while (fgets(l, sizeof l, f)) {
            char *c = strchr(l, ':');
            if (!c) { if (model[0]) break; continue; }
            c += 2;
            c[strcspn(c, "\n")] = 0;
            if (!strncmp(l, "model name", 10)) snprintf(model, sizeof model, "%s", c);
            else if (!strncmp(l, "vendor_id", 9)) snprintf(vendor, sizeof vendor, "%s", c);
            else if (!strncmp(l, "cpu family", 10)) fam = atoi(c);
            else if (!strncmp(l, "model\t", 6)) mod = atoi(c);
            else if (!strncmp(l, "stepping", 8)) step = atoi(c);
            else if (!strncmp(l, "cpu cores", 9)) cores = atoi(c);
        }
        fclose(f);
    }
    trim(model);
    {   /* "OctalCore AMD Ryzen 7 7800X3D" (core-count word + model name, without trailing "8-Core Processor") */
        char m[160];
        char *p;
        snprintf(m, sizeof m, "%s", model);
        if ((p = strstr(m, " Processor"))) *p = 0;
        if ((p = strstr(m, "-Core"))) { while (p > m && p[-1] != ' ') p--; if (p > m) p[-1] = 0; }
        if (count_word(cores)) snprintf(in->cpu_type, sizeof in->cpu_type, "%sCore %s", count_word(cores), m);
        else snprintf(in->cpu_type, sizeof in->cpu_type, "%d-Core %s", cores, m);
    }
    {
        const char *cn = codename(vendor, fam, mod);
        if (cn) snprintf(in->cpu_stepping, sizeof in->cpu_stepping, "%s, stepping %d  (family %Xh model %Xh)", cn, step, fam, mod);
        else snprintf(in->cpu_stepping, sizeof in->cpu_stepping, "Family %Xh, Model %Xh, Stepping %d", fam, mod, step);
    }
    if (mhz > 0) {
        snprintf(in->cpu_clock, sizeof in->cpu_clock, "%.1f MHz", mhz);
        snprintf(in->cpu_fsb, sizeof in->cpu_fsb, "100.0 MHz");
        snprintf(in->cpu_mult, sizeof in->cpu_mult, "%.2fx", mhz / 100.0);
    }
    snprintf(in->nb_clock, sizeof in->nb_clock, "n/a (needs SMU access - not read)");
    dimms(&n, &mts, type, sizeof type, &nch);
    if (mts > 0) {
        snprintf(in->mem_bus, sizeof in->mem_bus, "%.1f MHz", mts / 2.0);
        snprintf(in->dram_ratio, sizeof in->dram_ratio, "DRAM:FSB %d:1", (int)(mts / 2.0 / 100.0 + 0.5));
        snprintf(in->mem_type, sizeof in->mem_type, "%s%s%s-%d SDRAM  (%d DIMM%s)", count_word(nch) ? count_word(nch) : "",
                 nch > 0 ? " Channel " : "", type[0] ? type : "DRAM", mts, n, n == 1 ? "" : "s");
    } else {
        snprintf(in->mem_bus, sizeof in->mem_bus, "run srs-root once for DIMM data");
        snprintf(in->mem_type, sizeof in->mem_type, "unknown (no root DMI cache)");
    }
    chipset(in->chipset, sizeof in->chipset);
    rd1("/sys/class/dmi/id/board_vendor", v, sizeof v);
    rd1("/sys/class/dmi/id/board_name", v2, sizeof v2);
    snprintf(in->board, sizeof in->board, "%s %s", v, v2);
    rd1("/sys/class/dmi/id/bios_version", v, sizeof v);
    rd1("/sys/class/dmi/id/bios_date", v2, sizeof v2);
    snprintf(in->bios, sizeof in->bios, "%s  (%s)", v, v2);
    snprintf(in->isa, sizeof in->isa, "%s", cm_isa_name(cm_isa_best()));
}
