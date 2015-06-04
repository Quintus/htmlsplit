#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "config.h"
#include "split.h"

/* The BAD_CAST() macro comes from libxml2 itself,
 * see http://www.xmlsoft.org/html/libxml-xmlstring.html. */

static void extract_head(struct Splitter* p_splitter, htmlDocPtr p_document);

/**
 * Create a new Splitter struct. The result must be
 * freed with splitter_free() when you are done with it.
 */
struct Splitter* splitter_new()
{
    struct Splitter* ptr = (struct Splitter*) malloc(sizeof(struct Splitter));

    /* Sanitize memory */
    memset(ptr, '\0', sizeof(struct Splitter));

    /* Init all the fields */
    ptr->level = 1;
    ptr->p_head = xmlBufferCreate();

    return ptr;
}

/**
 * Free a Splitter instance.
 */
void splitter_free(struct Splitter* ptr)
{
    xmlBufferFree(ptr->p_head);
    free(ptr);
}

void splitter_split_file(struct Splitter* p_splitter, const char* infile, const char* outdir)
{
    htmlDocPtr p_document = NULL;
    p_document = htmlParseFile(infile, "UTF-8");

    extract_head(p_splitter, p_document);

    printf("=== <head> ===\n");
    printf(xmlBufferContent(p_splitter->p_head));
    printf("\n=== </head> ===\n");

    xmlFreeDoc(p_document);
}

void extract_head(struct Splitter* p_splitter, htmlDocPtr p_document)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;

    p_context = xmlXPathNewContext(p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST("/html/head"), p_context);

    if (!xmlNodeDump(p_splitter->p_head, p_document, p_results->nodesetval->nodeTab[0], 2, 1)) {
        fprintf(stderr, "Failed to extract <head> element.\n");
        exit(1);
    }

    xmlXPathFreeObject(p_results);
    xmlXPathFreeContext(p_context);
}
