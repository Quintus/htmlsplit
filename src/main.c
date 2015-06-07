#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "htmlsplit_config.h"
#include "split.h"
#include "verbose.h"
#include "toc.h"

static struct Splitter* sp_splitter = NULL;

static void print_usage(const char* name)
{
    fprintf(stderr, "Usage: %s -V | -h | [-v] [l] [-t] [-q] [-x XPATH] [-i FILE] [-o FILE] [-p SECNUM]\n", name);
}

static void print_copyright()
{
    fprintf(stderr, "                 8<---8<--- ::HTMLSPLIT:: ---8<---8<\n");
    fprintf(stderr, "                      8<    version %s    8<\n", HTMLSPLIT_VERSION);
    fprintf(stderr, "\n");
    fprintf(stderr,
            "Copyright (C) 2015 Marvin Gülker\n"
            "This program is free software under the GPL.\n"
            "It comes with ABSOLUTELY NO WARRANTY; for details, please see the file\n"
            "COPYING in the source tree.\n"
            "\n");
}

static bool parse_argv(int argc, char* argv[], struct Splitter* p_splitter)
{
    int curopt = 0;
    bool copyright = true;

    while ((curopt = getopt(argc, argv, "Vvhlqi:o:x:s:p:t:")) > 0) {
        switch (curopt) {
        case 'v':
            g_htmlsplit_verbose = true;
            break;
        case 'i':
            strcpy(p_splitter->infile, optarg);
            break;
        case 'o':
            strcpy(p_splitter->outdir, optarg);
            break;
        case 'x':
            strcpy(p_splitter->splitexpr, optarg);
            break;
        case 's':
            strcpy(p_splitter->stdoutsep, optarg);
            break;
        case 'p':
            p_splitter->secnum = atoi(optarg);
            break;
        case 'l':
            p_splitter->interlink = true;
            break;
        case 't':
            p_splitter->tocdepth = atoi(optarg);
            break;
        case 'q':
            copyright = false;
            break;
        case 'V':
            print_copyright();
            exit(0);
        case 'h':
            print_usage(argv[0]);
            xmlCleanupParser();
            exit(0);
            break;
        default: /* '?' */
            print_usage(argv[0]);
            exit(ERR_CLI);
        }
    }

    if (copyright)
        print_copyright();

    return true;
}

void cleanup()
{
    xmlCleanupParser();
}

void handle_sigterm_and_sigint(int sigval)
{
    sp_splitter->terminate = true;
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");
    xmlInitParser();

    atexit(cleanup);

    sp_splitter = splitter_new();

    signal(SIGTERM, handle_sigterm_and_sigint);
    signal(SIGINT, handle_sigterm_and_sigint);

    if (!sp_splitter) {
        fprintf(stderr, "Failed to initialize Splitter instance\n");
        exit(ERR_MEM);
    }

    parse_argv(argc, argv, sp_splitter);

    splitter_split_file(sp_splitter);

    if (sp_splitter->tocdepth > 0)
        splitter_generate_tocfile(sp_splitter);

    splitter_free(sp_splitter);

    return 0;
}
