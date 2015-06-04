#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
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
static void handle_body(struct Splitter* p_splitter, htmlDocPtr p_document, const char* outdir);
static void write_file(struct Splitter* p_splitter, xmlBufferPtr p_prevnodestr, const char* targetfile);

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
    handle_body(p_splitter, p_document, outdir);

    /*
    printf("=== <head> ===\n");
    printf(xmlBufferContent(p_splitter->p_head));
    printf("\n=== </head> ===\n");
    */

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

void handle_body(struct Splitter* p_splitter, htmlDocPtr p_document, const char* outdir)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;
    char targetfilename[PATH_MAX];
    int i = 0;
    int total = 0;

    /* Determine total number of split points */
    p_context = xmlXPathNewContext(p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST("//h1"), p_context); /* TODO: use p_splitter->level instead of <h1> */
    total     = p_results->nodesetval->nodeNr;
    xmlXPathFreeObject(p_results);

    printf("Found %d nodes for splitting.\n", total);

    for(i = 0; i < total; i++) {
        xmlNodePtr p_node = NULL;
        xmlNodePtr p_next_node = NULL;
        int nodecount = 0;
        xmlNodePtr* nodestore = NULL;
        int j = 0;
        p_results = xmlXPathEvalExpression(BAD_CAST("//h1"), p_context); /* TODO: use p_splitter->level instead of <h1> */
        p_node = p_results->nodesetval->nodeTab[i];

        printf("Examining node %d.\n", i);

        /* Count the number of following nodes we have to store */
        p_next_node = p_node;
        while(p_next_node) {
            p_next_node = xmlNextElementSibling(p_next_node);
            nodecount++;
        }
        nodecount--; /* NULL element not required */

        printf("Going to temporaryly delete %d sibling nodes.\n", nodecount);

        /* Allocate the space we need for storing */
        nodestore = malloc(nodecount * sizeof(xmlNodePtr));

        /* Store all the nodes and unlink them from the document */
        p_next_node = xmlNextElementSibling(p_node);
        while (p_next_node) {
            xmlNodePtr p_next_next_node = xmlNextElementSibling(p_next_node);
            xmlUnlinkNode(p_next_node);

            nodestore[j++] = p_next_node;
            p_next_node    = p_next_next_node;
        }


        memset(targetfilename, '\0', PATH_MAX);
        sprintf(targetfilename, "%s/%04d.html", outdir, i);

        FILE* p_file = fopen(targetfilename, "w");
        xmlDocDump(p_file, p_document);
        fclose(p_file);
        /* write_file(p_splitter, p_prevnodestr, targetfilename); */

        /* Re-add the unlinked nodes */
        p_next_node = p_node;
        for(j=0; j < nodecount; j++) {
            printf("Re-Adding node %d.\n", j);
            xmlAddNextSibling(p_next_node, nodestore[j]);
        }

        free(nodestore);
        xmlXPathFreeObject(p_results);
    }

    xmlXPathFreeContext(p_context);
}

/*
void recurse_collect_node(struct Splitter* p_splitter, xmlBufferPtr p_buffer, xmlNodePtr p_node)
{
    
}

xmlBufferPtr collect_preceeding_nodes(struct Splitter* p_splitter, htmlDocPtr p_docment, xmlNodePtr p_node)
{
    xmlXPathContextPtr p_bodyctx = NULL;
    xmlXPathObjectPtr p_bodyresult = NULL;
    xmlBufferPtr p_buffer = xmlBufferCreate();

    p_bodyctx = xmlXPathNewContext(p_document);
    p_bodyresult = xmlXPathEvalExpression(BAD_CAST("/html/body"), p_bodyctx);

    recurse_collect_node(p_splitter, p_buffer, p_bodyresult->nodesetval->nodeTab[0]);

    xmlXPathFreeObject(p_bodyresult);
    xmlXPathFreeContext(p_bodyresult);

    return p_buffer;
}
*/


void write_file(struct Splitter* p_splitter, xmlBufferPtr p_prevnodestr, const char* targetfile)
{
    printf("Writing file '%s'\n", targetfile);

    FILE* p_file = fopen(targetfile, "w");
    /* TODO */
    fclose(p_file);
}

