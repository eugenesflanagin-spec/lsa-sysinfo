/* lsa_gui.c - Linux System Audit GTK3 GUI.
 * Clean-room Linux implementation of AIDA64 Business features (AIDA64 is by FinalWire Ltd.,
 * aida64.com; not affiliated). First tool: the Cache & Memory Benchmark window - a grid of
 * Read / Write / Copy / Latency for Memory, L1, L2, L3 with the system lines below it.
 * With command-line arguments the same binary runs the CLI (lsa cachemem ...). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <gtk/gtk.h>
#include "cachemem.h"
#include "lsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    GtkWidget *win, *cell[CM_NLEVEL][CM_NTEST], *info[16], *start, *save, *guide, *status;
    const char *hover;   /* item under the pointer, for F1 */
    cm_result res;
    cm_info inf;
    int running, have;
    volatile int cancel;
    GThread *th;
} cmwin;

typedef struct { cmwin *w; int lv, te, done; cm_result r; } upd;

static const char *const info_lbl[] = { "CPU Type", "CPU Stepping", "CPU Clock", "CPU FSB", "CPU Multiplier", "North Bridge Clock",
    "Memory Bus", "DRAM:FSB Ratio", "Memory Type", "Chipset", "Motherboard", "BIOS Version", NULL };

static void fmt_cell(char *b, size_t n, int lv, int te, double v)
{
    if (v <= 0) { b[0] = 0; return; }
    if (te == CM_LAT) snprintf(b, n, "%.1f ns", v);
    else cm_format_bw(b, n, lv, v, 0);
}

static void fill_info(cmwin *w)
{
    const char *v[12];
    int i;
    v[0] = w->inf.cpu_type; v[1] = w->inf.cpu_stepping; v[2] = w->inf.cpu_clock; v[3] = w->inf.cpu_fsb; v[4] = w->inf.cpu_mult;
    v[5] = w->inf.nb_clock; v[6] = w->inf.mem_bus; v[7] = w->inf.dram_ratio; v[8] = w->inf.mem_type; v[9] = w->inf.chipset;
    v[10] = w->inf.board; v[11] = w->inf.bios;
    for (i = 0; i < 12; i++) gtk_entry_set_text(GTK_ENTRY(w->info[i]), v[i]);
}

static gboolean on_upd(gpointer data)
{
    upd *u = data;
    cmwin *w = u->w;
    char b[64];
    if (u->lv >= 0) {
        GtkStyleContext *sc = gtk_widget_get_style_context(w->cell[u->lv][u->te]);
        if (!u->done) {
            gtk_style_context_add_class(sc, "running");
            gtk_entry_set_text(GTK_ENTRY(w->cell[u->lv][u->te]), "...");
            snprintf(b, sizeof b, "Measuring %s %s ...", cm_level_name[u->lv], cm_test_name[u->te]);
            gtk_label_set_text(GTK_LABEL(w->status), b);
        } else {
            gtk_style_context_remove_class(sc, "running");
            fmt_cell(b, sizeof b, u->lv, u->te, u->r.v[u->lv][u->te]);
            gtk_entry_set_text(GTK_ENTRY(w->cell[u->lv][u->te]), b);
        }
    } else {   /* finished */
        int lv, te;
        w->res = u->r;
        w->running = 0;
        w->have = 1;
        g_thread_join(w->th);
        w->th = NULL;
        cm_get_info(&w->inf, w->res.clock_mhz);
        fill_info(w);
        for (lv = 0; lv < CM_NLEVEL; lv++)
            for (te = 0; te < CM_NTEST; te++) gtk_style_context_remove_class(gtk_widget_get_style_context(w->cell[lv][te]), "running");
        snprintf(b, sizeof b, "%s  -  %s code path, %d threads", w->cancel ? "Stopped" : "Finished", cm_isa_name(w->res.isa), w->res.threads);
        gtk_label_set_text(GTK_LABEL(w->status), b);
        gtk_button_set_label(GTK_BUTTON(w->start), "Start Benchmark");
        gtk_widget_set_sensitive(w->save, TRUE);
        if (!w->cancel) {   /* history, for comparing runs */
            char *dir = g_build_filename(g_get_user_cache_dir(), "lsa", NULL), *f = g_build_filename(dir, "cachemem-history.txt", NULL);
            FILE *fp;
            g_mkdir_with_parents(dir, 0755);
            if ((fp = fopen(f, "a"))) {
                time_t t = time(NULL);
                char ts[32];
                strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
                fprintf(fp, "%s  %s %dT", ts, cm_isa_name(w->res.isa), w->res.threads);
                for (lv = 0; lv < CM_NLEVEL; lv++)
                    for (te = 0; te < CM_NTEST; te++) fprintf(fp, " %.*f", te == CM_LAT ? 1 : 0, w->res.v[lv][te]);
                fputc('\n', fp);
                fclose(fp);
            }
            g_free(f);
            g_free(dir);
        }
    }
    g_free(u);
    return G_SOURCE_REMOVE;
}

static void cb_progress(int lv, int te, int done, const cm_result *r, void *ud)
{
    upd *u = g_new0(upd, 1);
    u->w = ud; u->lv = lv; u->te = te; u->done = done; u->r = *r;
    g_idle_add(on_upd, u);
}

static gpointer run_thread(gpointer data)
{
    cmwin *w = data;
    cm_cfg c;
    upd *u = g_new0(upd, 1);
    cm_cfg_default(&c);
    if (getenv("LSA_TEST_THREADS")) c.threads = atoi(getenv("LSA_TEST_THREADS"));   /* test hooks */
    if (getenv("LSA_TEST_SECONDS")) c.seconds = atof(getenv("LSA_TEST_SECONDS"));
    if (getenv("LSA_TEST_MEMMIB")) c.mem_mib = atol(getenv("LSA_TEST_MEMMIB"));
    cm_run(&c, &u->r, cb_progress, w, &w->cancel);
    u->w = w;
    u->lv = -1;
    g_idle_add(on_upd, u);
    return NULL;
}

static void on_start(GtkWidget *b, gpointer data)
{
    cmwin *w = data;
    int lv, te;
    (void)b;
    if (w->running) { w->cancel = 1; gtk_label_set_text(GTK_LABEL(w->status), "Stopping ..."); return; }
    if (!getenv("LSA_TEST_NOCONFIRM")) {
        GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(w->win), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
            "The Cache & Memory benchmark loads every CPU thread and the memory bus for about 20 seconds.\n\n"
            "Close other heavy programs for accurate results. Start now?");
        int ok;
        gtk_window_set_title(GTK_WINDOW(m), "Cache & Memory Benchmark");
        ok = gtk_dialog_run(GTK_DIALOG(m)) == GTK_RESPONSE_OK;
        gtk_widget_destroy(m);
        if (!ok) return;
    }
    for (lv = 0; lv < CM_NLEVEL; lv++)
        for (te = 0; te < CM_NTEST; te++) gtk_entry_set_text(GTK_ENTRY(w->cell[lv][te]), "");
    w->cancel = 0;
    w->running = 1;
    gtk_button_set_label(GTK_BUTTON(w->start), "Stop");
    gtk_widget_set_sensitive(w->save, FALSE);
    w->th = g_thread_new("cachemem", run_thread, w);
}

static void save_png_txt(cmwin *w, const char *png)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(w->win));
    int W = gtk_widget_get_allocated_width(child), H = gtk_widget_get_allocated_height(child), lv, te, i;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, W, H);
    cairo_t *cr = cairo_create(s);
    GtkStyleContext *sc = gtk_widget_get_style_context(w->win);
    char *txt = g_strdup(png), *dot = strrchr(txt, '.');
    FILE *f;
    gtk_render_background(sc, cr, 0, 0, W, H);
    gtk_widget_draw(child, cr);
    cairo_destroy(cr);
    cairo_surface_write_to_png(s, png);
    cairo_surface_destroy(s);
    if (dot && !strchr(dot, '/')) strcpy(dot, ".txt");
    else { g_free(txt); txt = g_strconcat(png, ".txt", NULL); }
    if ((f = fopen(txt, "w"))) {
        fprintf(f, "Linux System Audit %s - Cache & Memory Benchmark\n\n%-10s %14s %14s %14s %12s\n", LSA_VERSION, "", "Read", "Write", "Copy", "Latency");
        for (lv = 0; lv < CM_NLEVEL; lv++) {
            fprintf(f, "%-10s", cm_level_name[lv]);
            for (te = 0; te < CM_NTEST; te++) { char b[32]; fmt_cell(b, sizeof b, lv, te, w->res.v[lv][te]); fprintf(f, " %*s", te == CM_LAT ? 12 : 14, b[0] ? b : "-"); }
            fputc('\n', f);
        }
        fputc('\n', f);
        for (i = 0; info_lbl[i]; i++) fprintf(f, "%-20s %s\n", info_lbl[i], gtk_entry_get_text(GTK_ENTRY(w->info[i])));
        fprintf(f, "%-20s %s, %d threads\n", "Code path", cm_isa_name(w->res.isa), w->res.threads);
        fclose(f);
    }
    g_free(txt);
}

static void on_save(GtkWidget *b, gpointer data)
{
    cmwin *w = data;
    GtkWidget *d = gtk_file_chooser_dialog_new("Save Cache & Memory results", GTK_WINDOW(w->win), GTK_FILE_CHOOSER_ACTION_SAVE,
                                               "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    char name[64];
    time_t t = time(NULL);
    (void)b;
    strftime(name, sizeof name, "cachemem-%Y%m%d-%H%M%S.png", localtime(&t));
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d), name);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        save_png_txt(w, fn);
        gtk_label_set_text(GTK_LABEL(w->status), "Saved (PNG + TXT)");
        g_free(fn);
    }
    gtk_widget_destroy(d);
}

static gboolean on_delete(GtkWidget *x, GdkEvent *e, gpointer data)
{
    cmwin *w = data;
    (void)x; (void)e;
    if (w->running) { w->cancel = 1; if (w->th) g_thread_join(w->th); w->th = NULL; }
    gtk_main_quit();
    return FALSE;
}

static void on_close(GtkWidget *b, gpointer data) { (void)b; on_delete(NULL, NULL, data); }

/* ---- user guide: Guide button, F1 (item under the pointer), right-click on a value */
static void open_guide(cmwin *w, const char *id)
{
    char msg[8192];
    int dry = getenv("LSA_GUIDE_DRY_RUN") != NULL, ok = lsa_guide_open(id, dry, msg, sizeof msg);
    if (dry) { printf("guide %s: %s\n", id ? id : "(start)", msg); fflush(stdout); return; }
    if (!ok || *msg) {
        GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(w->win), GTK_DIALOG_MODAL, ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE, "%s", *msg ? msg : "Could not open the user guide.");
        gtk_window_set_title(GTK_WINDOW(m), "LSA User Guide");
        gtk_dialog_run(GTK_DIALOG(m));
        gtk_widget_destroy(m);
    }
}

static void on_guide(GtkWidget *b, gpointer data) { (void)b; open_guide(data, NULL); }

static void on_menu_guide(GtkMenuItem *mi, gpointer data)
{
    open_guide(data, g_object_get_data(G_OBJECT(mi), "item"));
}

static void on_menu_copy(GtkMenuItem *mi, gpointer data)
{
    (void)data;
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(mi), "entry"))), -1);
}

static gboolean on_item_press(GtkWidget *e, GdkEventButton *ev, gpointer data)
{
    const char *id = g_object_get_data(G_OBJECT(e), "item");
    GtkWidget *menu, *mi;
    char lbl[128];
    if (ev->type != GDK_BUTTON_PRESS || ev->button != 3 || !id) return FALSE;
    menu = gtk_menu_new();
    snprintf(lbl, sizeof lbl, "Open _guide: %s (F1)", lsa_guide_title(id));
    mi = gtk_menu_item_new_with_mnemonic(lbl);
    g_object_set_data(G_OBJECT(mi), "item", (gpointer)id);
    g_signal_connect(mi, "activate", G_CALLBACK(on_menu_guide), data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    mi = gtk_menu_item_new_with_mnemonic("_Copy value");
    g_object_set_data(G_OBJECT(mi), "entry", e);
    g_signal_connect(mi, "activate", G_CALLBACK(on_menu_copy), data);
    gtk_widget_set_sensitive(mi, gtk_entry_get_text_length(GTK_ENTRY(e)) > 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)ev);
    return TRUE;
}

static gboolean on_item_enter(GtkWidget *e, GdkEventCrossing *ev, gpointer data)
{
    (void)ev;
    ((cmwin *)data)->hover = g_object_get_data(G_OBJECT(e), "item");
    return FALSE;
}

static gboolean on_item_leave(GtkWidget *e, GdkEventCrossing *ev, gpointer data)
{
    cmwin *w = data;
    (void)ev;
    if (w->hover == g_object_get_data(G_OBJECT(e), "item")) w->hover = NULL;
    return FALSE;
}

static gboolean on_key(GtkWidget *x, GdkEventKey *ev, gpointer data)
{
    (void)x;
    if (ev->keyval == GDK_KEY_F1) { open_guide(data, ((cmwin *)data)->hover); return TRUE; }
    return FALSE;
}

/* a value with a guide section: tooltip, right-click menu, F1 while the pointer is on it */
static void link_item(cmwin *w, GtkWidget *e, const char *id)
{
    char tip[160];
    g_object_set_data(G_OBJECT(e), "item", (gpointer)id);
    snprintf(tip, sizeof tip, "%s - right-click or F1: open the user guide at this value", lsa_guide_title(id));
    gtk_widget_set_tooltip_text(e, tip);
    gtk_widget_add_events(e, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(e, "button-press-event", G_CALLBACK(on_item_press), w);
    g_signal_connect(e, "enter-notify-event", G_CALLBACK(on_item_enter), w);
    g_signal_connect(e, "leave-notify-event", G_CALLBACK(on_item_leave), w);
}

static GtkWidget *ro_entry(int chars, int right)
{
    GtkWidget *e = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(e), FALSE);
    gtk_widget_set_can_focus(e, FALSE);
    gtk_entry_set_width_chars(GTK_ENTRY(e), chars);
    if (right) gtk_entry_set_alignment(GTK_ENTRY(e), 1.0);
    return e;
}

static void open_cachemem(void)
{
    cmwin *w = g_new0(cmwin, 1);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8), *g = gtk_grid_new(), *gi = gtk_grid_new(), *bb, *close, *hdr;
    int lv, te, i;
    char title[128];
    snprintf(title, sizeof title, "Cache & Memory Benchmark - Linux System Audit %s", LSA_VERSION);
    w->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w->win), title);
    gtk_window_set_icon_name(GTK_WINDOW(w->win), "lsa");
    gtk_window_set_resizable(GTK_WINDOW(w->win), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 12);
    gtk_container_add(GTK_CONTAINER(w->win), vb);

    gtk_grid_set_row_spacing(GTK_GRID(g), 4);
    gtk_grid_set_column_spacing(GTK_GRID(g), 6);
    for (te = 0; te < CM_NTEST; te++) {
        hdr = gtk_label_new(cm_test_name[te]);
        gtk_widget_set_name(hdr, "colhdr");
        gtk_grid_attach(GTK_GRID(g), hdr, te + 1, 0, 1, 1);
    }
    for (lv = 0; lv < CM_NLEVEL; lv++) {
        GtkWidget *l = gtk_label_new(cm_level_name[lv]);
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        gtk_widget_set_name(l, "rowhdr");
        gtk_grid_attach(GTK_GRID(g), l, 0, lv + 1, 1, 1);
        for (te = 0; te < CM_NTEST; te++) {
            w->cell[lv][te] = ro_entry(te == CM_LAT ? 10 : 13, 1);
            gtk_widget_set_name(w->cell[lv][te], "cell");
            link_item(w, w->cell[lv][te], lsa_cell_item(lv, te));
            gtk_grid_attach(GTK_GRID(g), w->cell[lv][te], te + 1, lv + 1, 1, 1);
        }
    }
    gtk_box_pack_start(GTK_BOX(vb), g, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    gtk_grid_set_row_spacing(GTK_GRID(gi), 3);
    gtk_grid_set_column_spacing(GTK_GRID(gi), 10);
    for (i = 0; info_lbl[i]; i++) {
        GtkWidget *l = gtk_label_new(info_lbl[i]);
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(gi), l, 0, i, 1, 1);
        w->info[i] = ro_entry(58, 0);
        link_item(w, w->info[i], lsa_info_item[i]);
        gtk_widget_set_hexpand(w->info[i], TRUE);
        gtk_grid_attach(GTK_GRID(gi), w->info[i], 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(vb), gi, FALSE, FALSE, 0);

    w->status = gtk_label_new("");
    gtk_widget_set_halign(w->status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vb), w->status, FALSE, FALSE, 0);
    {
        GtkWidget *note = gtk_label_new("Linux System Audit is a clean-room Linux implementation of AIDA64 Business features. "
                                        "AIDA64 is by FinalWire Ltd.; not affiliated.");
        gtk_widget_set_name(note, "note");
        gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
        gtk_widget_set_halign(note, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(vb), note, FALSE, FALSE, 0);
    }
    bb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    w->save = gtk_button_new_with_mnemonic("_Save");
    w->start = gtk_button_new_with_mnemonic("Start Benchmark");
    close = gtk_button_new_with_mnemonic("_Close");
    w->guide = gtk_button_new_with_mnemonic("_Guide");
    gtk_widget_set_tooltip_text(w->guide, "Open the user guide (F1: at the value under the pointer; right-click a value for its section)");
    gtk_widget_set_sensitive(w->save, FALSE);
    gtk_box_pack_start(GTK_BOX(bb), w->save, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bb), w->guide, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bb), close, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bb), w->start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), bb, FALSE, FALSE, 0);
    g_signal_connect(w->start, "clicked", G_CALLBACK(on_start), w);
    g_signal_connect(w->save, "clicked", G_CALLBACK(on_save), w);
    g_signal_connect(close, "clicked", G_CALLBACK(on_close), w);
    g_signal_connect(w->guide, "clicked", G_CALLBACK(on_guide), w);
    g_signal_connect(w->win, "key-press-event", G_CALLBACK(on_key), w);
    g_signal_connect(w->win, "delete-event", G_CALLBACK(on_delete), w);

    cm_get_info(&w->inf, cm_measure_mhz());
    fill_info(w);
    {
        char b[160];
        snprintf(b, sizeof b, "Ready  -  %s code path", w->inf.isa);
        gtk_label_set_text(GTK_LABEL(w->status), b);
    }
    gtk_widget_show_all(w->win);
    if (getenv("LSA_AUTOSTART")) on_start(NULL, w);   /* test hook / --gui-start */
    if (getenv("LSA_TEST_GUIDE")) open_guide(w, *getenv("LSA_TEST_GUIDE") ? getenv("LSA_TEST_GUIDE") : NULL);   /* test hook */
}

static void load_css(void)
{
    GtkCssProvider *c = gtk_css_provider_new();
    gtk_css_provider_load_from_data(c,
        "#colhdr { font-weight: bold; }"
        "#rowhdr { font-weight: bold; padding-right: 8px; }"
        "#cell { font-family: monospace; }"
        "#cell.running { background-color: #FFF4B0; color: #000000; }"
        "#note { font-size: 8pt; opacity: 0.7; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(c), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(c);
}

int main(int argc, char **argv)
{
    int i;
    if (argc > 1 && strncmp(argv[1], "--gui", 5)) return lsa_cli_main(argc, argv);
    for (i = 1; i < argc; i++) if (!strcmp(argv[i], "--gui-start")) g_setenv("LSA_AUTOSTART", "1", TRUE);
    gtk_init(&argc, &argv);
    load_css();
    open_cachemem();
    gtk_main();
    return 0;
}
