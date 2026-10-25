/* lsa.h - Linux System Audit common definitions */
#ifndef LSA_H
#define LSA_H
#define LSA_VERSION "0.1.3"
#define LSA_CREDIT "Clean-room Linux implementation of AIDA64 Business features. AIDA64 is by FinalWire Ltd. (aida64.com);\n" \
                   "Linux System Audit is not affiliated with FinalWire. GPL-2.0-or-later."
#include <stddef.h>
#include <stdio.h>
int lsa_cli_main(int argc, char **argv);

/* user guide (guide.c): item ids for the grid cells and the system lines, and their sections */
#define LSA_NINFO 13   /* the 12 system lines + "Code path" */
extern const char *const lsa_info_item[LSA_NINFO];
const char *lsa_cell_item(int level, int test);
const char *lsa_guide_title(const char *id);    /* NULL: unknown item */
const char *lsa_guide_anchor(const char *id);   /* NULL / "" -> the guide's start; NULL: no section */
void lsa_guide_list(FILE *f);
int  lsa_guide_open(const char *id, int dry_run, char *msg, size_t mn);   /* 1 ok (msg may hold a note), 0 failed */
#endif
