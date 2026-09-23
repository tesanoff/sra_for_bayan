/* sra_config.c — argv and config-file parsing */

#include "sra_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SRA_VERSION_STR "4.06"

/* ------------------------------------------------------------------ */
/* Defaults                                                             */
/* ------------------------------------------------------------------ */

void sra_config_defaults(SraConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->chord_ch    = 3;                 /* 1-based: channel 3 */
    cfg->ctrl_offset = 0;

#ifdef _WIN32
    /* Default config path on Windows: %APPDATA%\sra\sra.conf.
       If APPDATA is not set, leave the path empty (no default). */
    {
        const char *appdata = getenv("APPDATA");
        if (appdata && *appdata) {
            snprintf(cfg->config_path, sizeof(cfg->config_path),
                     "%s\\sra\\sra.conf", appdata);
        }
    }
#else
    strncpy(cfg->config_path, "/etc/sra/sra.conf",
            sizeof(cfg->config_path) - 1);
#endif
}

/* ------------------------------------------------------------------ */
/* Help / version                                                       */
/* ------------------------------------------------------------------ */

void sra_config_print_help(void) {
    printf(
"sra — Software-based Real-time Arranger, version " SRA_VERSION_STR "\n"
"\n"
"Usage:\n"
"  sra                                  interactive mode\n"
"  sra --daemon [options]               daemon mode\n"
"\n"
"Options:\n"
"  --daemon            run as daemon (no UI, logs to syslog)\n"
"  --config PATH       config file (default: /etc/sra/sra.conf)\n"
"  --in ADDR           MIDI IN  rawmidi address, e.g. hw:5,0\n"
"  --out ADDR          MIDI OUT rawmidi address, e.g. hw:5,1\n"
"  --chord-ch N        chord channel (1-16)\n"
"  --ctrl-offset N     shift command key zone: -1, 0, or +1\n"
"  --styles-dir PATH   directory with style*.mid files\n"
"                      (default: current working directory)\n"
"  --help              show this help and exit\n"
"  --version           show version and exit\n"
"\n"
"Config file format:\n"
"  key = value      one per line; '#' starts a comment\n"
"  Recognised keys: in, out, chord_ch, ctrl_offset, styles_dir\n"
);
}

void sra_config_print_version(void) {
    printf("sra " SRA_VERSION_STR " (libsracore 1.3)\n");
}

/* ------------------------------------------------------------------ */
/* argv parsing                                                         */
/* ------------------------------------------------------------------ */

static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v;
    if (!s || !*s) return -1;
    v = strtol(s, &end, 10);
    if (!end || *end != '\0') return -1;
    if (v < -32768 || v > 32767) return -1;
    *out = (int)v;
    return 0;
}

int sra_config_parse_args(SraConfig *cfg, int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];

        if      (strcmp(a, "--daemon")  == 0) {
#ifdef _WIN32
            fprintf(stderr, "sra: daemon mode is not supported on Windows\n");
            return 1;
#else
            cfg->daemon = 1;
#endif
        }
        else if (strcmp(a, "--help")    == 0) { cfg->show_help = 1; }
        else if (strcmp(a, "--version") == 0) { cfg->show_version = 1; }

        else if (strcmp(a, "--config") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --config requires an argument\n");
                return 1;
            }
            strncpy(cfg->config_path, argv[i],
                    sizeof(cfg->config_path) - 1);
        }
        else if (strcmp(a, "--in") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --in requires an argument\n");
                return 1;
            }
            strncpy(cfg->in_addr, argv[i], sizeof(cfg->in_addr) - 1);
            cfg->has_in = 1;
        }
        else if (strcmp(a, "--out") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --out requires an argument\n");
                return 1;
            }
            strncpy(cfg->out_addr, argv[i], sizeof(cfg->out_addr) - 1);
            cfg->has_out = 1;
        }
        else if (strcmp(a, "--chord-ch") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --chord-ch requires an argument\n");
                return 1;
            }
            if (parse_int(argv[i], &cfg->chord_ch) != 0) {
                fprintf(stderr, "sra: --chord-ch must be an integer\n");
                return 1;
            }
            if (cfg->chord_ch < 1 || cfg->chord_ch > 16) {
                fprintf(stderr, "sra: --chord-ch must be in 1..16\n");
                return 1;
            }
            cfg->has_chord_ch = 1;
        }
        else if (strcmp(a, "--ctrl-offset") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --ctrl-offset requires an argument\n");
                return 1;
            }
            if (parse_int(argv[i], &cfg->ctrl_offset) != 0) {
                fprintf(stderr, "sra: --ctrl-offset must be an integer\n");
                return 1;
            }
            if (cfg->ctrl_offset < -1 || cfg->ctrl_offset > 1) {
                fprintf(stderr, "sra: --ctrl-offset must be -1, 0, or +1\n");
                return 1;
            }
            cfg->has_ctrl_offset = 1;
        }
        else if (strcmp(a, "--styles-dir") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "sra: --styles-dir requires an argument\n");
                return 1;
            }
            if (argv[i][0] == '\0') {
                fprintf(stderr, "sra: --styles-dir requires a non-empty argument\n");
                return 1;
            }
            strncpy(cfg->styles_dir, argv[i], sizeof(cfg->styles_dir) - 1);
            cfg->styles_dir[sizeof(cfg->styles_dir) - 1] = '\0';
            cfg->has_styles_dir = 1;
        }
        else {
            fprintf(stderr, "sra: unknown option '%s'\n", a);
            fprintf(stderr, "Try 'sra --help' for usage.\n");
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Config file                                                          */
/* ------------------------------------------------------------------ */

static char *trim(char *s) {
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

int sra_config_load(SraConfig *cfg) {
    FILE *f;
    char  line[512];
    int   lineno = 0;

    f = fopen(cfg->config_path, "r");
    if (!f) return 0;   /* missing file is not an error here */

    while (fgets(line, sizeof(line), f)) {
        char *p, *eq, *key, *val;
        lineno++;

        p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        eq = strchr(p, '=');
        if (!eq) {
            fprintf(stderr, "sra: %s:%d: missing '='\n",
                    cfg->config_path, lineno);
            fclose(f);
            return 1;
        }
        *eq = '\0';
        key = trim(p);
        val = trim(eq + 1);

        if (strcmp(key, "in") == 0) {
            if (!cfg->has_in) {
                strncpy(cfg->in_addr, val, sizeof(cfg->in_addr) - 1);
                cfg->in_addr[sizeof(cfg->in_addr) - 1] = '\0';
            }
        }
        else if (strcmp(key, "out") == 0) {
            if (!cfg->has_out) {
                strncpy(cfg->out_addr, val, sizeof(cfg->out_addr) - 1);
                cfg->out_addr[sizeof(cfg->out_addr) - 1] = '\0';
            }
        }
        else if (strcmp(key, "chord_ch") == 0) {
            int v;
            if (parse_int(val, &v) != 0) {
                fprintf(stderr, "sra: %s:%d: chord_ch must be an integer\n",
                        cfg->config_path, lineno);
                fclose(f);
                return 1;
            }
            if (v < 1 || v > 16) {
                fprintf(stderr, "sra: %s:%d: chord_ch must be 1..16\n",
                        cfg->config_path, lineno);
                fclose(f);
                return 1;
            }
            if (!cfg->has_chord_ch) cfg->chord_ch = v;
        }
        else if (strcmp(key, "ctrl_offset") == 0) {
            int v;
            if (parse_int(val, &v) != 0) {
                fprintf(stderr, "sra: %s:%d: ctrl_offset must be an integer\n",
                        cfg->config_path, lineno);
                fclose(f);
                return 1;
            }
            if (v < -1 || v > 1) {
                fprintf(stderr, "sra: %s:%d: ctrl_offset must be -1, 0, or +1\n",
                        cfg->config_path, lineno);
                fclose(f);
                return 1;
            }
            if (!cfg->has_ctrl_offset) cfg->ctrl_offset = v;
        }
        else if (strcmp(key, "styles_dir") == 0) {
            if (val[0] == '\0') {
                fprintf(stderr, "sra: %s:%d: styles_dir must not be empty\n",
                        cfg->config_path, lineno);
                fclose(f);
                return 1;
            }
            if (!cfg->has_styles_dir) {
                strncpy(cfg->styles_dir, val, sizeof(cfg->styles_dir) - 1);
                cfg->styles_dir[sizeof(cfg->styles_dir) - 1] = '\0';
            }
        }
        else {
            fprintf(stderr, "sra: %s:%d: unknown key '%s'\n",
                    cfg->config_path, lineno, key);
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}
