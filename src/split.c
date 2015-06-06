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
#include "config.h"
#include "split.h"
#include "verbose.h"

/* The BAD_CAST() macro comes from libxml2 itself,
 * see http://www.xmlsoft.org/html/libxml-xmlstring.html. */

static htmlDocPtr read_input(struct Splitter* p_splitter);
static void handle_body(struct Splitter* p_splitter, htmlDocPtr p_document);
static void write_part(struct Splitter* p_splitter, htmlDocPtr p_document, const char* targetfile);
static void slice_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void slice_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_following_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static void reinsert_preceeding_nodes(struct Splitter* p_splitter, xmlNodePtr p_node);
static xmlNodePtr add_interlinks(struct Splitter* p_splitter, xmlNodePtr p_parent_node, int i, int total);
static void remove_interlinks(struct Splitter* p_splitter, xmlNodePtr p_interlinks_node);

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
    ptr->secnum               = -1;
    ptr->interlink            = false;
    ptr->tocdepth             = 1;
    strcpy(ptr->splitexpr, "//h1"); /* default split point xpath */
    strcpy(ptr->stdoutsep, "<!-- HTMLSPLIT -->"); /* default stdout split separator */

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
    htmlDocPtr p_document = read_input(p_splitter);

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

    if (!p_results->nodesetval) {
        fprintf(stderr, "XPath expression '%s' is invalid.\n", p_splitter->splitexpr);
        exit(ERR_CLI);
    }

    total = p_results->nodesetval->nodeNr;
    xmlXPathFreeObject(p_results);

    verbprintf("Found %d split points.\n", total);

    /* Now iterate them all. We do the splitting by deleting every node
     * on our level before the last target, and everything behind the
     * current target. Graphically:
     *
     *   xxxxxxx<>.........<>xxxxxxxx
     *
     * where x will be deleted, <> is a split point, and . is kept. */
    for(i = 0; i <= total; i++) {
        xmlNodePtr p_start_node     = NULL; /* Start split point; will be kept */
        xmlNodePtr p_end_node       = NULL; /* End split point; will be deleted */
        xmlNodePtr p_parent_node    = NULL; /* Common parent */
        xmlNodePtr p_interlink_node = NULL; /* Temporary node for the links between parts */

        if (p_splitter->terminate) {
            fprintf(stderr, "Abnormal termination requested, quitting before handling split point %d.\n", i);
            return;
        }

        /* If only a specific section was queried, abort if we are not there. */
        if (p_splitter->secnum >= 0 && i != p_splitter->secnum) {
            continue;
        }

        /* As we modify the document using the following functions,
         * we invalidate the XPath result and must query for each
         * tag anew. */
        p_results = xmlXPathEvalExpression(BAD_CAST(p_splitter->splitexpr), p_context);

        if (i > 0) {
            p_start_node = p_results->nodesetval->nodeTab[i-1];
            p_parent_node = p_start_node->parent; /* Only needed for interlinking */
        }
        if (i < total) {
            p_end_node = p_results->nodesetval->nodeTab[i];
            p_parent_node = p_end_node->parent;
        }

        /* Remove those parts we are not interested in */
        slice_preceeding_nodes(p_splitter, p_start_node);
        slice_following_nodes(p_splitter, p_end_node);

        if (p_splitter->interlink)
            p_interlink_node = add_interlinks(p_splitter, p_parent_node, i, total);

        /* Write out */
        if (strlen(p_splitter->outdir) == 0) { /* stdout requested */
            write_part(p_splitter, p_document, NULL);

            if (i < total) { /* Separator */
                printf("%s\n", p_splitter->stdoutsep);
            }
        }
        else {
            memset(targetfilename, '\0', PATH_MAX);
            sprintf(targetfilename, "%s/%04d.html", p_splitter->outdir, i);
            write_part(p_splitter, p_document, targetfilename);
        }

        if (p_splitter->interlink)
            remove_interlinks(p_splitter, p_interlink_node);

        /* Resurrect deleted parts */
        reinsert_preceeding_nodes(p_splitter, p_start_node);
        reinsert_following_nodes(p_splitter, p_parent_node);

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
    int i = 0;

    /* If this was the last part, there was nothing following. */
    if (!p_node)
        return;

    /* Re-add the unlinked nodes */
    for(i=0; i < p_splitter->num_following_nodes; i++) {
        verbprintf("Re-Adding following node %d (%s).\n", i, p_splitter->p_following_nodes[i]->name);

        xmlAddChild(p_node, p_splitter->p_following_nodes[i]);
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

/**
 * Write out the document in its current state. If `targetfile' is NULL,
 * the document is output to the standard output. If it isn’t, the document
 * is written to that file.
 */
void write_part(struct Splitter* p_splitter, htmlDocPtr p_document, const char* targetfile)
{
    if (targetfile) {
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
    else { /* Output to stdout */
        xmlChar* xmlstr = NULL;
        int size = 0;

        verbprintf("Writing to standard output\n", targetfile);
        htmlDocDumpMemory(p_document, &xmlstr, &size);

        printf("%s", (char*) xmlstr);

        xmlFree(xmlstr);
    }
}

xmlNodePtr add_interlinks(struct Splitter* p_splitter, xmlNodePtr p_parent_node, int i, int total)
{
    xmlNodePtr interlink_div = NULL;
    xmlNodePtr interlink_ul = NULL;

    /* Return if single-part document */
    if (!p_parent_node)
        return NULL;

    verbprintf("Adding interlinks section to part %i.\n", i);

    interlink_div = xmlNewChild(p_parent_node, NULL, BAD_CAST("div"), NULL);
    xmlNewProp(interlink_div, BAD_CAST("class"), BAD_CAST("htmlsplit-interlinks"));

    interlink_ul = xmlNewChild(interlink_div, NULL, BAD_CAST("ul"), NULL);

    char uri[1024];

    if (i > 0) {
        xmlNodePtr li = NULL;
        xmlNodePtr a  = NULL;

        memset(uri, '\0', 1024);
        sprintf(uri, "%04d.html", i-1);

        li = xmlNewChild(interlink_ul, NULL, BAD_CAST("li"), NULL);
        a  = xmlNewChild(li, NULL, BAD_CAST("a"), BAD_CAST("&larr;"));

        xmlNewProp(a, BAD_CAST("href"), BAD_CAST(uri));
    }
    if (i < total) {
        xmlNodePtr li = NULL;
        xmlNodePtr a  = NULL;

        memset(uri, '\0', 1024);
        sprintf(uri, "%04d.html", i+1);

        li = xmlNewChild(interlink_ul, NULL, BAD_CAST("li"), NULL);
        a  = xmlNewChild(li, NULL, BAD_CAST("a"), BAD_CAST("&rarr;"));

        xmlNewProp(a, BAD_CAST("href"), BAD_CAST(uri));
    }

    return interlink_div;
}

void remove_interlinks(struct Splitter* p_splitter, xmlNodePtr p_interlinks_node)
{
    if (p_interlinks_node) {
        verbprintf("Removing interlinks section.\n");

        xmlUnlinkNode(p_interlinks_node);
        xmlFreeNode(p_interlinks_node);
    }
}

/**
 * Read input from either standard input or a file, depending on
 * the contents of the `infile` attribute of `p_splitter`.
 */
htmlDocPtr read_input(struct Splitter* p_splitter)
{
    htmlDocPtr p_document = NULL;

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
        p_document  = htmlReadDoc(p_xmlbuffer, "(stdin)", "UTF-8", 0);

        xmlFree(p_xmlbuffer);
        free(p_buffer);
    }
    else { /* File requested */
        verbprintf("Reading file '%s'.\n", p_splitter->infile);
        p_document = htmlParseFile(p_splitter->infile, "UTF-8");
    }

    return p_document;
}
