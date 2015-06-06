#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <linux/limits.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "config.h"
#include "split.h"
#include "verbose.h"

/* The BAD_CAST() macro comes from libxml2 itself,
 * see http://www.xmlsoft.org/html/libxml-xmlstring.html. */

static void handle_body(struct Splitter* p_splitter, htmlDocPtr p_document);
static void write_file(struct Splitter* p_splitter, htmlDocPtr p_document, const char* targetfile);
static void slice_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void slice_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);

/**
 * Create a new Splitter struct. The result must be
 * freed with splitter_free() when you are done with it.
 * Returns NULL if malloc() fails.
 */
struct Splitter* splitter_new()
{
    struct Splitter* ptr = (struct Splitter*) malloc(sizeof(struct Splitter));

    if (!ptr) {
        perror("Failed to allocate Splitter struct");
        return NULL;
    }

    /* Sanitize memory */
    memset(ptr, '\0', sizeof(struct Splitter));

    /* Init all the fields */
    ptr->p_following_nodes    = NULL;
    ptr->p_preceeding_nodes   = NULL;
    ptr->num_following_nodes  = 0;
    ptr->num_preceeding_nodes = 0;
    ptr->terminate            = false;
    strcpy(ptr->splitexpr, "//h1"); /* default split point xpath */

    return ptr;
}

/**
 * Free a Splitter instance.
 */
void splitter_free(struct Splitter* ptr)
{
    free(ptr);
}

void splitter_split_file(struct Splitter* p_splitter)
{
    htmlDocPtr p_document = NULL;
    p_document = htmlParseFile(p_splitter->infile, "UTF-8");

    if (!p_document) {
        fprintf(stderr, "Failed to parse document file '%s'.\n", p_splitter->infile);
        exit(ERR_PARSE);
    }

    handle_body(p_splitter, p_document);

    xmlFreeDoc(p_document);
}

void handle_body(struct Splitter* p_splitter, htmlDocPtr p_document)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;
    char targetfilename[PATH_MAX];
    int i = 0;
    int total = 0;

    /* Determine total number of split points */
    p_context = xmlXPathNewContext(p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST(p_splitter->splitexpr), p_context);
    total     = p_results->nodesetval->nodeNr;
    xmlXPathFreeObject(p_results);

    verbprintf("Found %d nodes for splitting.\n", total);

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

        if (p_splitter->terminate) {
            fprintf(stderr, "Abnormal termination requested, quitting before handling split point %d.\n", i);
            return;
        }

        /* As we modify the document using the following functions,
         * we invalidate the XPath result and must query for each
         * tag anew. */
        p_results = xmlXPathEvalExpression(BAD_CAST(p_splitter->splitexpr), p_context);

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
        sprintf(targetfilename, "%s/%04d.html", p_splitter->outdir, i);
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

    verbprintf("Going to temporaryly delete %d following nodes.\n", nodecount);

    /* Allocate the space we need for storing */
    nodestore = (xmlNodePtr*) malloc(nodecount * sizeof(xmlNodePtr));

    if (!nodestore) {
        perror("Failed to allocate memory for following-nodes store");
        exit(ERR_MEM);
    }

    /* Store all the nodes and unlink them from the document */
    p_next_node = p_node; /* Trailing next split point must be removed */
    while (p_next_node) {
        xmlNodePtr p_next_next_node = xmlNextElementSibling(p_next_node);

        verbprintf("Removing <%s>\n", p_next_node->name);
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
    verbprintf("Going to temporaryly delete %d preceeding nodes.\n", nodecount);
    nodestore = (xmlNodePtr*) malloc(nodecount * sizeof(xmlNodePtr));

    if (!nodestore) {
        perror("Failed to allocate memory for preceeding-nodes store");
        exit(ERR_MEM);
    }

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
        verbprintf("Re-Adding following node %d (%s).\n", i, p_splitter->p_following_nodes[i]->name);

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
        verbprintf("Re-Adding preceeding node %d (%s).\n", i, p_splitter->p_preceeding_nodes[i]->name);

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
    verbprintf("Writing file '%s'\n", targetfile);

    FILE* p_file = fopen(targetfile, "w");
    if (p_file) {
        htmlDocDump(p_file, p_document);
        fclose(p_file);
    }
    else {
        int errsav = errno;
        printf("Failed to open file '%s': %s\n", targetfile, strerror(errsav));
        exit(ERR_IO);
    }
}
