#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <linux/limits.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "config.h"
#include "split.h"
#include "verbose.h"

static void print_usage(const char* name)
{
    fprintf(stderr, "Usage: %s [-v] [-i FILE] [-o FILE]\n", name);
}

static bool parse_argv(int argc, char* argv[], struct Splitter* p_splitter)
{
    int curopt = 0;

    while ((curopt = getopt(argc, argv, "vhi:o:")) > 0) {
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

    return true;
}

void cleanup()
{
    xmlCleanupParser();
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");
    xmlInitParser();

    atexit(cleanup);

    struct Splitter* p_splitter = splitter_new();

    if (!p_splitter) {
        fprintf(stderr, "Failed to initialize Splitter instance\n");
        exit(ERR_MEM);
    }

    parse_argv(argc, argv, p_splitter);
    splitter_split_file(p_splitter);
    splitter_free(p_splitter);

    return 0;
}
