/* lsa.h - Linux System Audit common definitions */
#ifndef LSA_H
#define LSA_H
#define LSA_VERSION "0.1.1"
#define LSA_CREDIT "Clean-room Linux implementation of AIDA64 Business features. AIDA64 is by FinalWire Ltd. (aida64.com);\n" \
                   "Linux System Audit is not affiliated with FinalWire. GPL-2.0-or-later."
int lsa_cli_main(int argc, char **argv);
#endif
