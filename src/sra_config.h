#ifndef SRA_CONFIG_H
#define SRA_CONFIG_H

/* ------------------------------------------------------------------ */
/* Configuration for SRA.                                               */
/*                                                                      */
/* Values are filled in three steps, each overriding the previous:      */
/*   1. defaults                                                        */
/*   2. config file  (see --config, default /etc/sra/sra.conf)          */
/*   3. command line                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int  daemon;                /* 1 = daemon mode (no UI, syslog)      */
    int  show_help;             /* 1 = print --help and exit            */
    int  show_version;          /* 1 = print --version and exit         */
    char in_addr[64];           /* MIDI IN  rawmidi address, e.g. hw:5,0 */
    char out_addr[64];          /* MIDI OUT rawmidi address, e.g. hw:5,1 */
    int  chord_ch;              /* chord channel, 1-based (1..16)       */
    int  ctrl_offset;           /* -1, 0, +1 (same as UI Ctrl Offset)   */
    char styles_dir[512];       /* directory with style*.mid files      */
    char config_path[256];      /* path to config file                   */

    /* "set by argv" flags — used so the config file does not
       overwrite values explicitly given on the command line. */
    int  has_in;
    int  has_out;
    int  has_chord_ch;
    int  has_ctrl_offset;
    int  has_styles_dir;
} SraConfig;

/* Initialise cfg with defaults. */
void sra_config_defaults(SraConfig *cfg);

/* Parse argv into cfg.  Overrides values already set.
   Returns 0 on success, non-zero on error (message printed to stderr). */
int  sra_config_parse_args(SraConfig *cfg, int argc, char **argv);

/* Read key = value lines from cfg->config_path.
   Missing file is not an error in interactive mode, but is an error
   in daemon mode (checked by the caller).
   Returns 0 on success, non-zero on parse error. */
int  sra_config_load(SraConfig *cfg);

/* Print --help / --version text to stdout. */
void sra_config_print_help(void);
void sra_config_print_version(void);

#endif /* SRA_CONFIG_H */
