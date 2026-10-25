/* lsa_win.c - Linux System Audit, Windows GUI (native Win32, no runtime needed).
 * Clean-room implementation of AIDA64 Business features (AIDA64 by FinalWire Ltd., aida64.com;
 * not affiliated). Cache & Memory Benchmark window: Read / Write / Copy / Latency for Memory,
 * L1, L2, L3, the system lines below, Save (BMP + TXT), Start Benchmark, Close. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachemem.h"
#include "lsa.h"

#define WM_CM (WM_APP + 1)
enum { ID_START = 100, ID_SAVE, ID_CLOSE, ID_GUIDE };

static HWND g_win, g_cell[CM_NLEVEL][CM_NTEST], g_info[12], g_start, g_save, g_status;
static HFONT g_font, g_bold, g_mono;
static cm_result g_res;
static volatile int g_cancel, g_running;
static HANDLE g_thread;
static int g_have;
static cm_info g_inf;

static const char *const info_lbl[] = { "CPU Type", "CPU Stepping", "CPU Clock", "CPU FSB", "CPU Multiplier", "North Bridge Clock",
    "Memory Bus", "DRAM:FSB Ratio", "Memory Type", "Chipset", "Motherboard", "BIOS Version", NULL };

typedef struct { int lv, te, done; cm_result r; } upd;

static void set_text(HWND h, const char *s)
{
    wchar_t w[512];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 512);
    SetWindowTextW(h, w);
}

static void fmt_cell(char *b, size_t n, int lv, int te, double v)
{
    if (v <= 0) { b[0] = 0; return; }
    if (te == CM_LAT) snprintf(b, n, "%.1f ns", v);
    else cm_format_bw(b, n, lv, v, 0);
}

static void fill_info(void)
{
    const char *v[12] = { g_inf.cpu_type, g_inf.cpu_stepping, g_inf.cpu_clock, g_inf.cpu_fsb, g_inf.cpu_mult, g_inf.nb_clock,
                          g_inf.mem_bus, g_inf.dram_ratio, g_inf.mem_type, g_inf.chipset, g_inf.board, g_inf.bios };
    int i;
    for (i = 0; i < 12; i++) set_text(g_info[i], v[i]);
}

static void cb_progress(int lv, int te, int done, const cm_result *r, void *ud)
{
    upd *u = malloc(sizeof *u);
    (void)ud;
    if (!u) return;
    u->lv = lv; u->te = te; u->done = done; u->r = *r;
    PostMessageW(g_win, WM_CM, 0, (LPARAM)u);
}

static DWORD WINAPI run_thread(LPVOID arg)
{
    cm_cfg c;
    upd *u = calloc(1, sizeof *u);
    (void)arg;
    cm_cfg_default(&c);
    if (getenv("LSA_TEST_THREADS")) c.threads = atoi(getenv("LSA_TEST_THREADS"));
    if (getenv("LSA_TEST_SECONDS")) c.seconds = atof(getenv("LSA_TEST_SECONDS"));
    cm_run(&c, &u->r, cb_progress, NULL, &g_cancel);
    u->lv = -1;
    PostMessageW(g_win, WM_CM, 0, (LPARAM)u);
    return 0;
}

static void on_start(void)
{
    int lv, te;
    if (g_running) { g_cancel = 1; set_text(g_status, "Stopping ..."); return; }
    if (!getenv("LSA_TEST_NOCONFIRM") &&
        MessageBoxW(g_win, L"The Cache & Memory benchmark loads every CPU thread and the memory bus for about 20 seconds.\n\n"
                           L"Close other heavy programs for accurate results. Start now?",
                    L"Cache & Memory Benchmark", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    for (lv = 0; lv < CM_NLEVEL; lv++)
        for (te = 0; te < CM_NTEST; te++) SetWindowTextW(g_cell[lv][te], L"");
    g_cancel = 0;
    g_running = 1;
    SetWindowTextW(g_start, L"Stop");
    EnableWindow(g_save, FALSE);
    g_thread = CreateThread(NULL, 0, run_thread, NULL, 0, NULL);
}

static void history_append(void)
{
    wchar_t dir[MAX_PATH], file[MAX_PATH];
    FILE *f;
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    wcscat(dir, L"\\LSA");
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\cachemem-history.txt", dir);
    if ((f = _wfopen(file, L"a"))) {
        SYSTEMTIME t;
        int lv, te;
        GetLocalTime(&t);
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %s %dT", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, cm_isa_name(g_res.isa), g_res.threads);
        for (lv = 0; lv < CM_NLEVEL; lv++)
            for (te = 0; te < CM_NTEST; te++) fprintf(f, " %.*f", te == CM_LAT ? 1 : 0, g_res.v[lv][te]);
        fputc('\n', f);
        fclose(f);
    }
}

static void on_update(upd *u)
{
    char b[200];
    if (u->lv >= 0) {
        if (!u->done) {
            SetWindowTextW(g_cell[u->lv][u->te], L"...");
            snprintf(b, sizeof b, "Measuring %s %s ...", cm_level_name[u->lv], cm_test_name[u->te]);
            set_text(g_status, b);
        } else {
            fmt_cell(b, sizeof b, u->lv, u->te, u->r.v[u->lv][u->te]);
            set_text(g_cell[u->lv][u->te], b);
        }
        return;
    }
    g_res = u->r;
    g_running = 0;
    g_have = 1;
    if (g_thread) { WaitForSingleObject(g_thread, INFINITE); CloseHandle(g_thread); g_thread = NULL; }
    cm_get_info(&g_inf, g_res.clock_mhz);
    fill_info();
    snprintf(b, sizeof b, "%s  -  %s, %d threads; latency on %s pages%s", g_cancel ? "Stopped" : "Finished", cm_isa_name(g_res.isa), g_res.threads,
             cm_large_pages() ? "2 MB" : "4 KB", g_res.mem_write == CM_MW_CLZERO ? "; Memory Write: CLZERO" : "");
    set_text(g_status, b);
    SetWindowTextW(g_start, L"Start Benchmark");
    EnableWindow(g_save, TRUE);
    if (!g_cancel) history_append();
}

/* Save: client area as BMP + the results as TXT next to it */
static void on_save(void)
{
    wchar_t file[MAX_PATH] = L"cachemem.bmp", txt[MAX_PATH];
    OPENFILENAMEW of;
    RECT rc;
    HDC wdc, mdc;
    HBITMAP bmp;
    BITMAPINFOHEADER bi;
    int W, H, lv, te, i;
    void *bits;
    FILE *f;
    memset(&of, 0, sizeof of);
    of.lStructSize = sizeof of;
    of.hwndOwner = g_win;
    of.lpstrFilter = L"Bitmap (*.bmp)\0*.bmp\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrDefExt = L"bmp";
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&of)) return;
    GetClientRect(g_win, &rc);
    W = rc.right; H = rc.bottom;
    wdc = GetDC(g_win);
    mdc = CreateCompatibleDC(wdc);
    memset(&bi, 0, sizeof bi);
    bi.biSize = sizeof bi; bi.biWidth = W; bi.biHeight = H; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = BI_RGB;
    bmp = CreateDIBSection(wdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mdc, bmp);
    PrintWindow(g_win, mdc, PW_CLIENTONLY);
    if ((f = _wfopen(file, L"wb"))) {
        BITMAPFILEHEADER fh;
        DWORD stride = ((DWORD)W * 3 + 3) & ~3u, img = stride * (DWORD)H;
        memset(&fh, 0, sizeof fh);
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof bi; fh.bfSize = fh.bfOffBits + img;
        bi.biSizeImage = img;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&bi, sizeof bi, 1, f); fwrite(bits, img, 1, f);
        fclose(f);
    }
    DeleteDC(mdc); DeleteObject(bmp); ReleaseDC(g_win, wdc);
    wcscpy(txt, file);
    { wchar_t *dot = wcsrchr(txt, L'.'); if (dot) wcscpy(dot, L".txt"); else wcscat(txt, L".txt"); }
    if ((f = _wfopen(txt, L"w"))) {
        char b[64];
        fprintf(f, "Linux System Audit %s (Windows) - Cache & Memory Benchmark\n\n%-10s %14s %14s %14s %12s\n", LSA_VERSION, "", "Read", "Write", "Copy", "Latency");
        for (lv = 0; lv < CM_NLEVEL; lv++) {
            fprintf(f, "%-10s", cm_level_name[lv]);
            for (te = 0; te < CM_NTEST; te++) { fmt_cell(b, sizeof b, lv, te, g_res.v[lv][te]); fprintf(f, " %*s", te == CM_LAT ? 12 : 14, b[0] ? b : "-"); }
            fputc('\n', f);
        }
        fputc('\n', f);
        {
            const char *v[12] = { g_inf.cpu_type, g_inf.cpu_stepping, g_inf.cpu_clock, g_inf.cpu_fsb, g_inf.cpu_mult, g_inf.nb_clock,
                                  g_inf.mem_bus, g_inf.dram_ratio, g_inf.mem_type, g_inf.chipset, g_inf.board, g_inf.bios };
            for (i = 0; info_lbl[i]; i++) fprintf(f, "%-20s %s\n", info_lbl[i], v[i]);
        }
        fprintf(f, "%-20s %s, %d threads; latency on %s pages%s\n", "Code path", cm_isa_name(g_res.isa), g_res.threads,
                cm_large_pages() ? "2 MB" : "4 KB", g_res.mem_write == CM_MW_CLZERO ? "; Memory Write: CLZERO" : "");
        fclose(f);
    }
    set_text(g_status, "Saved (BMP + TXT)");
}

static HWND mk(const wchar_t *cls, const wchar_t *txt, DWORD style, int x, int y, int w, int h, int id, HFONT f)
{
    HWND c = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, txt, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_win,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}

static void build(void)
{
    static const wchar_t *const lvn[] = { L"Memory", L"L1 Cache", L"L2 Cache", L"L3 Cache" };
    static const wchar_t *const ten[] = { L"Read", L"Write", L"Copy", L"Latency" };
    static const int colx[] = { 100, 222, 344, 466 }, colw[] = { 116, 116, 116, 90 };
    int lv, te, i, y;
    for (te = 0; te < CM_NTEST; te++) mk(L"STATIC", ten[te], SS_CENTER, colx[te], 12, colw[te], 18, 0, g_bold);
    for (lv = 0; lv < CM_NLEVEL; lv++) {
        mk(L"STATIC", lvn[lv], 0, 14, 38 + lv * 28, 84, 18, 0, g_bold);
        for (te = 0; te < CM_NTEST; te++)
            g_cell[lv][te] = mk(L"EDIT", L"", ES_READONLY | ES_RIGHT | ES_AUTOHSCROLL, colx[te], 34 + lv * 28, colw[te], 24, 0, g_mono);
    }
    y = 160;
    for (i = 0; info_lbl[i]; i++) {
        wchar_t w[64];
        MultiByteToWideChar(CP_UTF8, 0, info_lbl[i], -1, w, 64);
        mk(L"STATIC", w, 0, 14, y + i * 26 + 4, 130, 18, 0, g_font);
        g_info[i] = mk(L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL, 150, y + i * 26, 406, 22, 0, g_font);
    }
    y += 12 * 26 + 6;
    g_status = mk(L"STATIC", L"", 0, 14, y, 542, 18, 0, g_font);
    mk(L"STATIC", L"Linux System Audit for Windows - clean-room implementation of AIDA64 Business features. "
                  L"AIDA64 is by FinalWire Ltd.; not affiliated.", 0, 14, y + 22, 542, 32, 0, g_font);
    y += 62;
    g_save = mk(L"BUTTON", L"&Save", BS_PUSHBUTTON, 14, y, 90, 28, ID_SAVE, g_font);
    mk(L"BUTTON", L"&Guide", BS_PUSHBUTTON, 110, y, 90, 28, ID_GUIDE, g_font);
    g_start = mk(L"BUTTON", L"Start Benchmark", BS_DEFPUSHBUTTON, 356, y, 120, 28, ID_START, g_font);
    mk(L"BUTTON", L"&Close", BS_PUSHBUTTON, 482, y, 74, 28, ID_CLOSE, g_font);
    EnableWindow(g_save, FALSE);
}

/* the HTML user guide beside the program (F1 / Guide button) */
static void open_guide(void)
{
    char msg[1024];
    if (!lsa_guide_open(NULL, 0, msg, sizeof msg)) {
        wchar_t w[1024];
        MultiByteToWideChar(CP_UTF8, 0, msg, -1, w, 1024);
        MessageBoxW(g_win, w, L"LSA User Guide", MB_OK | MB_ICONWARNING);
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START) on_start();
        else if (LOWORD(wp) == ID_SAVE) on_save();
        else if (LOWORD(wp) == ID_CLOSE) SendMessageW(h, WM_CLOSE, 0, 0);
        else if (LOWORD(wp) == ID_GUIDE) open_guide();
        return 0;
    case WM_CM: { upd *u = (upd *)lp; on_update(u); free(u); return 0; }
    case WM_CTLCOLORSTATIC: {   /* read-only edits keep a white field like the classic layout */
        HDC dc = (HDC)wp;
        wchar_t cls[16];
        GetClassNameW((HWND)lp, cls, 16);
        if (!wcscmp(cls, L"Edit")) { SetBkColor(dc, GetSysColor(COLOR_WINDOW)); return (LRESULT)GetSysColorBrush(COLOR_WINDOW); }
        break;
    }
    case WM_CLOSE:
        if (g_running) { g_cancel = 1; if (g_thread) WaitForSingleObject(g_thread, INFINITE); }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    WNDCLASSEXW wc;
    MSG msg;
    RECT r = { 0, 0, 570, 584 };
    char b[160];
    NONCLIENTMETRICSW ncm;
    (void)prev; (void)cmd;
    SetProcessDPIAware();
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    g_bold = CreateFontIndirectW(&ncm.lfMessageFont);
    g_mono = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"LSACacheMem";
    RegisterClassExW(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    {
        wchar_t title[128];
        swprintf(title, 128, L"Cache & Memory Benchmark - Linux System Audit %hs (Windows)", LSA_VERSION);
        g_win = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    }
    build();
    cm_get_info(&g_inf, cm_measure_mhz());
    fill_info();
    snprintf(b, sizeof b, "Ready  -  %s code path; latency on %s pages", g_inf.isa, cm_large_pages() ? "2 MB" : "4 KB (grant \"Lock pages in memory\" for 2 MB)");
    set_text(g_status, b);
    ShowWindow(g_win, show);
    UpdateWindow(g_win);
    if (getenv("LSA_AUTOSTART")) PostMessageW(g_win, WM_COMMAND, ID_START, 0);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1) { open_guide(); continue; }
        if (IsDialogMessageW(g_win, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
