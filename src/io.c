#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlerror.h>
#include "split.h"
#include "verbose.h"

/**
 * Write out the document in its current state. If `targetfile' is NULL,
 * the document is output to the standard output. If it isn’t, the document
 * is written to that file.
 */
void splitter_write_part(struct Splitter* p_splitter, const char* targetfile)
{
    if (targetfile) {
        verbprintf("Writing file '%s'\n", targetfile);

        FILE* p_file = fopen(targetfile, "w");
        if (p_file) {
            htmlDocDump(p_file, p_splitter->p_document);
            fclose(p_file);
        }
        else {
            int errsav = errno;
            printf("Failed to open file '%s': %s\n", targetfile, strerror(errsav));
            exit(ERR_IO);
        }
    }
    else { /* Output to stdout */
        xmlChar* xmlstr = NULL;
        int size = 0;

        verbprintf("Writing to standard output\n", targetfile);
        htmlDocDumpMemory(p_splitter->p_document, &xmlstr, &size);

        printf("%s", (char*) xmlstr);

        xmlFree(xmlstr);
    }
}

/**
 * Read input from either standard input or a file, depending on
 * the contents of the `infile` attribute of `p_splitter`.
 */
void splitter_read_input(struct Splitter* p_splitter)
{
    p_splitter->p_document = NULL;

    if (strlen(p_splitter->infile) == 0) { /* stdin requested */
        char* p_buffer       = NULL;
        xmlChar* p_xmlbuffer = NULL;
        size_t size          = 0;

        verbprintf("Reading from standard input.\n");

        while (!feof(stdin)) {
            p_buffer = realloc(p_buffer, size + 4096);
            memset(p_buffer + size, '\0', 4096);

            size += fread(p_buffer + size, 1, 4096, stdin);
        }

        verbprintf("Read %li bytes from standard input.\n", size);

        p_xmlbuffer = xmlCharStrndup(p_buffer, size);
        p_splitter->p_document = htmlReadDoc(p_xmlbuffer, "(stdin)", "UTF-8", 0);

        xmlFree(p_xmlbuffer);
        free(p_buffer);
    }
    else { /* File requested */
        verbprintf("Reading file '%s'.\n", p_splitter->infile);
        p_splitter->p_document = htmlParseFile(p_splitter->infile, "UTF-8");
    }
}
