/* cminfo_win.c - Linux System Audit (Windows build): the system lines under the Cache & Memory
 * results. CPU from CPUID, cores from GetLogicalProcessorInformationEx, BIOS / board / DIMMs
 * from the SMBIOS table (GetSystemFirmwareTable 'RSMB' - no administrator rights needed). */
#include "cachemem.h"
#include <windows.h>
#include <cpuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *count_word(int n)
{
    static const char *const w[] = { "", "Single", "Dual", "Triple", "Quad", "Penta", "Hexa", "Hepta", "Octal",
        "", "Deca", "", "Dodeca", "", "Tetradeca", "", "Hexadeca" };
    return n > 0 && n < (int)(sizeof w / sizeof *w) && w[n][0] ? w[n] : NULL;
}

static const char *codename(int amd, int fam, int model)
{
    if (amd) {
        if (fam == 0x19 && model >= 0x60 && model <= 0x6f) return "Raphael";
        if (fam == 0x19 && model >= 0x70 && model <= 0x7f) return "Phoenix";
        if (fam == 0x19 && model >= 0x20 && model <= 0x2f) return "Vermeer";
        if (fam == 0x19 && model >= 0x50 && model <= 0x5f) return "Cezanne";
        if (fam == 0x1a && model >= 0x40 && model <= 0x4f) return "Granite Ridge";
        if (fam == 0x17 && model == 0x71) return "Matisse";
    } else if (fam == 6) {
        switch (model) {
        case 0xb7: case 0xba: case 0xbf: return "Raptor Lake";
        case 0x97: case 0x9a: return "Alder Lake";
        case 0xc6: return "Arrow Lake";
        }
    }
    return NULL;
}

static void trim(char *s)
{
    size_t l = strlen(s);
    while (l && s[l - 1] == ' ') s[--l] = 0;
    while (*s == ' ') memmove(s, s + 1, strlen(s));
}

static int physical_cores(void)
{
    DWORD len = 0;
    int n = 0;
    char *b, *p;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (!len || !(b = malloc(len))) return 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &len))
        for (p = b; p < b + len; p += ((PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p)->Size) n++;
    free(b);
    return n;
}

/* string #idx (1-based) of an SMBIOS structure */
static const char *smb_str(const unsigned char *s, int idx)
{
    const char *t = (const char *)s + s[1];
    if (idx <= 0) return "";
    while (--idx > 0 && *t) t += strlen(t) + 1;
    return t;
}

typedef struct {
    char bios_ver[64], bios_date[32], board_vendor[96], board_name[96], mem_type[16];
    int dimms, mts, channels;
} smb;

static void read_smbios(smb *o)
{
    DWORD sz = GetSystemFirmwareTable(0x52534D42u /* 'RSMB' */, 0, NULL, 0);
    unsigned char *b, *t, *end;
    char chans[8][24];
    int nch = 0, i;
    memset(o, 0, sizeof *o);
    if (!sz || !(b = malloc(sz))) return;
    if (GetSystemFirmwareTable(0x52534D42u /* 'RSMB' */, 0, b, sz) != sz) { free(b); return; }
    t = b + 8;   /* RawSMBIOSData header: 4 bytes version info + DWORD length */
    end = t + *(DWORD *)(b + 4);
    while (t + 4 <= end && t[0] != 127) {
        unsigned char type = t[0], len = t[1];
        unsigned char *next = t + len;
        if (len < 4) break;
        while (next + 1 < end && (next[0] || next[1])) next++;
        next += 2;
        if (type == 0 && len >= 0x09) {
            snprintf(o->bios_ver, sizeof o->bios_ver, "%s", smb_str(t, t[5]));
            snprintf(o->bios_date, sizeof o->bios_date, "%s", smb_str(t, t[8]));
        } else if (type == 2 && len >= 0x06) {
            snprintf(o->board_vendor, sizeof o->board_vendor, "%s", smb_str(t, t[4]));
            snprintf(o->board_name, sizeof o->board_name, "%s", smb_str(t, t[5]));
        } else if (type == 17 && len >= 0x15) {
            unsigned size = *(unsigned short *)(t + 0x0C);
            if (size && size != 0xFFFF) {
                static const struct { int code; const char *name; } mt[] = { { 0x1A, "DDR4" }, { 0x22, "DDR5" }, { 0x18, "DDR3" },
                    { 0x1E, "LPDDR4" }, { 0x23, "LPDDR5" }, { 0, NULL } };
                const char *bank = smb_str(t, t[0x11]), *c = strstr(bank, "CHANNEL ");
                int k;
                o->dimms++;
                for (k = 0; mt[k].name; k++) if (mt[k].code == t[0x12]) snprintf(o->mem_type, sizeof o->mem_type, "%s", mt[k].name);
                if (len >= 0x22) { int sp = *(unsigned short *)(t + 0x20); if (sp > o->mts) o->mts = sp; }
                else if (len >= 0x17) { int sp = *(unsigned short *)(t + 0x15); if (sp > o->mts) o->mts = sp; }
                if (c) {
                    for (i = 0; i < nch; i++) if (!strcmp(chans[i], c + 8)) break;
                    if (i == nch && nch < 8) snprintf(chans[nch++], sizeof chans[0], "%s", c + 8);
                }
            }
        }
        t = next;
    }
    o->channels = nch ? nch : (o->dimms >= 2 ? 2 : o->dimms);
    trim(o->board_vendor);
    trim(o->board_name);
    free(b);
}

void cm_get_info(cm_info *in, double mhz)
{
    unsigned a = 0, b = 0, c = 0, d = 0, regs[12] = { 0 };
    char brand[64] = "", vendor[13] = "";
    int fam, model, step, cores = physical_cores(), amd;
    smb s;
    memset(in, 0, sizeof *in);
    __get_cpuid(0, &a, &b, &c, &d);
    memcpy(vendor, &b, 4); memcpy(vendor + 4, &d, 4); memcpy(vendor + 8, &c, 4);
    amd = !strcmp(vendor, "AuthenticAMD");
    __get_cpuid(1, &a, &b, &c, &d);
    fam = (a >> 8) & 0xf; model = (a >> 4) & 0xf; step = a & 0xf;
    if (fam == 0xf) fam += (a >> 20) & 0xff;
    if (fam >= 6) model += ((a >> 16) & 0xf) << 4;
    if (__get_cpuid(0x80000002u, &regs[0], &regs[1], &regs[2], &regs[3])) {
        __get_cpuid(0x80000003u, &regs[4], &regs[5], &regs[6], &regs[7]);
        __get_cpuid(0x80000004u, &regs[8], &regs[9], &regs[10], &regs[11]);
        memcpy(brand, regs, 48);
        brand[48] = 0;
        trim(brand);
    }
    {
        char m[64], *p;
        snprintf(m, sizeof m, "%s", brand);
        if ((p = strstr(m, " Processor"))) *p = 0;
        if ((p = strstr(m, "-Core"))) { while (p > m && p[-1] != ' ') p--; if (p > m) p[-1] = 0; }
        if (count_word(cores)) snprintf(in->cpu_type, sizeof in->cpu_type, "%sCore %s", count_word(cores), m);
        else snprintf(in->cpu_type, sizeof in->cpu_type, "%d-Core %s", cores, m);
    }
    {
        const char *cn = codename(amd, fam, model);
        if (cn) snprintf(in->cpu_stepping, sizeof in->cpu_stepping, "%s, stepping %d  (family %Xh model %Xh)", cn, step, fam, model);
        else snprintf(in->cpu_stepping, sizeof in->cpu_stepping, "Family %Xh, Model %Xh, Stepping %d", fam, model, step);
    }
    if (mhz > 0) {
        snprintf(in->cpu_clock, sizeof in->cpu_clock, "%.1f MHz", mhz);
        snprintf(in->cpu_fsb, sizeof in->cpu_fsb, "100.0 MHz");
        snprintf(in->cpu_mult, sizeof in->cpu_mult, "%.2fx", mhz / 100.0);
    }
    snprintf(in->nb_clock, sizeof in->nb_clock, "n/a (needs SMU access - not read)");
    read_smbios(&s);
    if (s.mts > 0) {
        snprintf(in->mem_bus, sizeof in->mem_bus, "%.1f MHz", s.mts / 2.0);
        snprintf(in->dram_ratio, sizeof in->dram_ratio, "DRAM:FSB %d:1", (int)(s.mts / 2.0 / 100.0 + 0.5));
        snprintf(in->mem_type, sizeof in->mem_type, "%s%s%s-%d SDRAM  (%d DIMM%s)", count_word(s.channels) ? count_word(s.channels) : "",
                 s.channels > 0 ? " Channel " : "", s.mem_type[0] ? s.mem_type : "DRAM", s.mts, s.dimms, s.dimms == 1 ? "" : "s");
    } else {
        snprintf(in->mem_bus, sizeof in->mem_bus, "unknown (SMBIOS has no memory device entries)");
        snprintf(in->mem_type, sizeof in->mem_type, "unknown");
    }
    {   /* chipset model from the board name, e.g. "TUF GAMING X870-PRO WIFI7 W NEO" -> "AMD X870" */
        char model_s[16] = "";
        size_t i;
        const char *bn = s.board_name;
        for (i = 0; bn[i]; i++)
            if (strchr("ABHQXZW", bn[i]) && bn[i + 1] >= '0' && bn[i + 1] <= '9' && bn[i + 2] >= '0' && bn[i + 2] <= '9' &&
                bn[i + 3] >= '0' && bn[i + 3] <= '9' && (i == 0 || bn[i - 1] == ' ' || bn[i - 1] == '-')) {
                size_t k = 0;
                while (bn[i + k] && bn[i + k] != ' ' && bn[i + k] != '-' && k < sizeof model_s - 1) { model_s[k] = bn[i + k]; k++; }
                model_s[k] = 0;
                break;
            }
        if (model_s[0]) snprintf(in->chipset, sizeof in->chipset, "%s %s%s%s", amd ? "AMD" : "Intel", model_s,
                                 codename(amd, fam, model) ? ", " : "", codename(amd, fam, model) ? codename(amd, fam, model) : "");
    }
    snprintf(in->board, sizeof in->board, "%s %s", s.board_vendor, s.board_name);
    snprintf(in->bios, sizeof in->bios, "%s  (%s)", s.bios_ver, s.bios_date);
    snprintf(in->isa, sizeof in->isa, "%s", cm_isa_name(cm_isa_best()));
}
