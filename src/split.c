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
static void write_file(struct Splitter* p_splitter, htmlDocPtr p_document, const char* targetfile);
static void slice_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void slice_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);

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

    /* If no split points were found we can just dump everything
     * into the first “part”. */
    memset(targetfilename, '\0', PATH_MAX);
    sprintf(targetfilename, "%s/%04d.html", outdir, 0);
    if (total == 0) {
        write_file(p_splitter, p_document, targetfilename);
    }
    else {
        /* Now iterate them all. We do the splitting by deleting every node
         * on our level before the last target, and everything behind the
         * current target. Graphically:
         *
         *   xxxxxxx<>.........<>xxxxxxxx
         *
         * where x will be deleted, <> is a split point, and . is kept. */
        for(i = 0; i <= total; i++) {
            xmlNodePtr p_start_node = NULL; /* Start split point; will be kept */
            xmlNodePtr p_end_node   = NULL; /* End split point; will be deleted */
            xmlNodePtr p_parent_node = NULL; /* Common parent */

            /* As we modify the document using the following functions,
             * we invalidate the XPath result and must query for each
             * tag anew. */
            p_results = xmlXPathEvalExpression(BAD_CAST("//h1"), p_context); /* TODO: use p_splitter->level instead of <h1> */

            if (i > 0) {
                p_start_node = p_results->nodesetval->nodeTab[i-1];
            }
            if (i < total) {
                p_end_node = p_results->nodesetval->nodeTab[i];
                p_parent_node = p_end_node->parent;
            }

            /* Remove those parts we are not interested in */
            slice_preceeding_nodes(p_splitter, p_start_node);
            slice_following_nodes(p_splitter, p_end_node);

            /* Write out */
            memset(targetfilename, '\0', PATH_MAX);
            sprintf(targetfilename, "%s/%04d.html", outdir, i);
            write_file(p_splitter, p_document, targetfilename);

            /* Resurrect preceeding parts */
            reinsert_preceeding_nodes(p_splitter, p_start_node);

            /* Resurrect following parts */
            if (i < total) {
                xmlXPathFreeObject(p_results);

                p_results  = xmlXPathNodeEval(p_parent_node, BAD_CAST("child::*"), p_context);
                p_end_node = p_results->nodesetval->nodeTab[p_results->nodesetval->nodeNr - 1];
            }
            else {
                /* No following part in last iteration */
                p_end_node = NULL;
            }

            reinsert_following_nodes(p_splitter, p_end_node);
            xmlXPathFreeObject(p_results);
        }
    }

    xmlXPathFreeContext(p_context);
}

void slice_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node)
{
    xmlNodePtr p_next_node = NULL;
    xmlNodePtr* nodestore = NULL;
    int nodecount = 0;
    int i = 0;

    /* Node is NULL for the very last part. There is nothing following,
     * so we can just return. */
    if (!p_node) {
        p_splitter->num_following_nodes = 0;
        p_splitter->p_following_nodes = NULL;
        return;
    }

    /* Count the number of following nodes we have to store */
    p_next_node = p_node; /* Trailing next split point must be removed */
    while(p_next_node) {
        p_next_node = xmlNextElementSibling(p_next_node);
        nodecount++;
    }

    printf("Going to temporaryly delete %d following nodes.\n", nodecount);

    /* Allocate the space we need for storing */
    nodestore = (xmlNodePtr*) malloc(nodecount * sizeof(xmlNodePtr));

    /* Store all the nodes and unlink them from the document */
    p_next_node = p_node; /* Trailing next split point must be removed */
    while (p_next_node) {
        xmlNodePtr p_next_next_node = xmlNextElementSibling(p_next_node);

        printf("Removing <%s>\n", p_next_node->name);
        xmlUnlinkNode(p_next_node);

        nodestore[i++] = p_next_node;
        p_next_node    = p_next_next_node;
    }

    p_splitter->num_following_nodes = nodecount;
    p_splitter->p_following_nodes   = nodestore;
}

void slice_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node)
{
    xmlNodePtr p_prev_node;
    xmlNodePtr* nodestore = NULL;
    int nodecount = 0;
    int i = 0;

    /* Node is NULL for the very first part. There is nothing preceeding,
     * so we can just return. */
    if (!p_node) {
        p_splitter->num_preceeding_nodes = 0;
        p_splitter->p_preceeding_nodes = NULL;
        return;
    }

    p_prev_node = xmlPreviousElementSibling(p_node); /* Previous splitpoint itself must not be removed */
    while (p_prev_node) {
        p_prev_node = xmlPreviousElementSibling(p_prev_node);
        nodecount++;
    }

    /* Allocate the space we need for storing */
    printf("Going to temporaryly delete %d preceeding nodes.\n", nodecount);
    nodestore = (xmlNodePtr*) malloc(nodecount * sizeof(xmlNodePtr));

    /* Store all the nodes and unlink them from the document */
    p_prev_node = xmlPreviousElementSibling(p_node);  /* Previous splitpoint itself must not be removed */
    while (p_prev_node) {
        xmlNodePtr p_prev_prev_node = xmlPreviousElementSibling(p_prev_node);
        xmlUnlinkNode(p_prev_node);

        nodestore[i++] = p_prev_node;
        p_prev_node = p_prev_prev_node;
    }

    p_splitter->num_preceeding_nodes = nodecount;
    p_splitter->p_preceeding_nodes   = nodestore;
}

void reinsert_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node)
{
    xmlNodePtr p_next_node = NULL;
    int i = 0;

    /* If this was the last part, there was nothing following. */
    if (!p_node)
        return;

    /* Re-add the unlinked nodes */
    p_next_node = p_node;
    for(i=0; i < p_splitter->num_following_nodes; i++) {
        printf("Re-Adding following node %d (%s).\n", i, p_splitter->p_following_nodes[i]->name);

        xmlAddNextSibling(p_next_node, p_splitter->p_following_nodes[i]);
        p_next_node = xmlNextElementSibling(p_next_node);
    }

    /* Cleanup */
    free(p_splitter->p_following_nodes);
    p_splitter->p_following_nodes = NULL;
    p_splitter->num_following_nodes = 0;
}

void reinsert_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node)
{
    xmlNodePtr p_prev_node = NULL;
    int i = 0;

    /* If this was the very first part, there was nothing preceeding. */
    if (!p_node)
        return;

    /* Re-add the unlinked nodes */
    p_prev_node = p_node;
    for(i=0; i < p_splitter->num_preceeding_nodes; i++) {
        printf("Re-Adding preceeding node %d (%s).\n", i, p_splitter->p_preceeding_nodes[i]->name);

        xmlAddPrevSibling(p_prev_node, p_splitter->p_preceeding_nodes[i]);
        p_prev_node = xmlPreviousElementSibling(p_prev_node);
    }

    /* Cleanup */
    free(p_splitter->p_preceeding_nodes);
    p_splitter->p_preceeding_nodes = NULL;
    p_splitter->num_preceeding_nodes = 0;
}

void write_file(struct Splitter* p_splitter, htmlDocPtr p_document, const char* targetfile)
{
    printf("Writing file '%s'\n", targetfile);

    FILE* p_file = fopen(targetfile, "w");
    htmlDocDump(p_file, p_document);
    fclose(p_file);
}
