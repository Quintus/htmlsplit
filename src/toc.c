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
#include "toc.h"
#include "io.h"
#include "verbose.h"

static xmlNodePtr strip_document(struct Splitter* p_splitter);

/**
 * This function is to be called during the splitting process.
 * `index` is the number of the current splitting point.
 *
 * It examines all data left in the document after the nodes
 * irrelevant to this splitting point have been removed and
 * filters it for HTML heading elements, whose info is then
 * stored inside a linked list inside the `p_sectioninfo`
 * member of the `p_splitter` object.
 */
void splitter_collect_toc_info(struct Splitter* p_splitter, int index)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;

    p_context = xmlXPathNewContext(p_splitter->p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST("//h1|//h2|//h3|//h4|//h5|//h6"), p_context);

    verbprintf("Collecting ToC info for %d headings below split point %d.\n", p_results->nodesetval->nodeNr, index);

    /* Very first part before first split point may not have
     * any heading tags. */
    if (p_results && p_results->nodesetval->nodeNr > 0) {
        struct SectionInfo* p_last_section = NULL;
        int i = 0;

        /* We need to append to the end of the list; note that
         * for the first item p_last_section will be NULL. */
        p_last_section = p_splitter->p_sectioninfo;
        while (p_last_section && p_last_section->p_next) {
            p_last_section = p_last_section->p_next;
        }

        for(i=0; i < p_results->nodesetval->nodeNr; i++) {
            xmlChar* anchorid = xmlGetProp(p_results->nodesetval->nodeTab[i], BAD_CAST("id"));

            /* If this heading as an ID attribute, remember it for later ToC generation. */
            if (anchorid) {
                struct SectionInfo* p_section = (struct SectionInfo*) malloc(sizeof(struct SectionInfo));
                xmlChar* titlestr = NULL;
                memset(p_section, '\0', sizeof(struct SectionInfo));

                titlestr = xmlNodeListGetString(p_splitter->p_document, p_results->nodesetval->nodeTab[i]->xmlChildrenNode, 1);

                p_section->level = atoi(((char*) p_results->nodesetval->nodeTab[i]->name) + 1); /* Strip leading “h” of h1, h2, etc. */
                strcpy(p_section->anchor, (char*) anchorid);
                strcpy(p_section->title, (char*) titlestr);
                sprintf(p_section->filename, "%04d.html", index);

                if (p_last_section)
                    p_last_section->p_next = p_section;
                else /* First section info */
                    p_splitter->p_sectioninfo = p_section;
                p_last_section = p_section;

                xmlFree(anchorid);
                xmlFree(titlestr);
            }
        }
    }

    xmlXPathFreeObject(p_results);
    xmlXPathFreeContext(p_context);
}

/**
 * This function evaluates the data collected with
 * splitter_collect_toc_info() and writes a ToC file
 * out to disk, or, if output to standard output was
 * requested, as a separate “part” out to standard
 * output.
 */
void splitter_generate_tocfile(struct Splitter* p_splitter)
{
    struct SectionInfo* p_section = NULL;
    xmlNodePtr p_parent_node = NULL;
    xmlNodePtr p_node = NULL;
    xmlNodePtr p_list = NULL;
    int current_level = 1; /* Level for <h1> tags */

    verbprintf("Generating Table of Contents.\n");
    p_parent_node = strip_document(p_splitter);

    /* Create base structure */
    p_node = xmlNewChild(p_parent_node, NULL, BAD_CAST("div"), NULL);
    xmlNewProp(p_node, BAD_CAST("class"), BAD_CAST("htmlsplit-toc"));

    xmlNewTextChild(p_node, NULL, BAD_CAST("h1"), BAD_CAST("Table of Contents"));
    p_node = xmlNewChild(p_node, NULL, BAD_CAST("ul"), NULL);

    /* Add ToC items */
    p_list = p_node;
    p_section = p_splitter->p_sectioninfo;
    while(p_section) {
        char uri[1024];
        xmlChar* xmltitle = NULL;

        /* Honour user-specified depth limit */
        if (p_splitter->tocdepth < p_section->level) {
            verbprintf("Section '%s' has level %d, which is above the threshold of %d.\n", p_section->title, p_section->level, p_splitter->tocdepth);
            p_section = p_section->p_next;
            continue;
        }

        /* Indentation */
        if (p_section->level < current_level) {
            /* This node has a lower level than the current list.
             * We need to close the lists corresponding to this. */
            for(; current_level > p_section->level; current_level--) {
                verbprintf("Closing ToC level %d.\n", current_level);
                p_list = p_list->parent->parent; /* <ul>→<li>→<ul> */
            }
        }
        else if (p_section->level > current_level) {
            /* This node has a higher level than the current list.
             * We need to open a new sublist. */
            for(; current_level < p_section->level; current_level++) {
                xmlNodePtr p_li = NULL;
                verbprintf("Opening ToC level %d.\n", current_level+1);

                p_li   = xmlNewChild(p_list, NULL, BAD_CAST("li"), NULL);
                p_list = xmlNewChild(p_li, NULL, BAD_CAST("ul"), NULL);
            }
        }

        /* Preparation */
        memset(uri, '\0', 1024);
        sprintf(uri, "%s#%s", p_section->filename, p_section->anchor);
        xmltitle = xmlCharStrdup(p_section->title);

        verbprintf("Adding section '%s' (section level %d) to ToC on level %d.\n", p_section->title, p_section->level, current_level);

        /* Add node */
        p_node = xmlNewChild(p_list, NULL, BAD_CAST("li"), NULL);
        p_node = xmlNewTextChild(p_node, NULL, BAD_CAST("a"), xmltitle);
        xmlNewProp(p_node, BAD_CAST("href"), BAD_CAST(uri));

        /* Next */
        p_section = p_section->p_next;
        xmlFree(xmltitle);
    }

    /* Write out */
    if (strlen(p_splitter->outdir) == 0) { /* stdout requested */
        printf("%s\n", p_splitter->stdoutsep);
        splitter_write_part(p_splitter, NULL);
    }
    else {
        char path[PATH_MAX];
        memset(path, '\0', PATH_MAX);

        sprintf(path, "%s/toc.html", p_splitter->outdir);
        splitter_write_part(p_splitter, path);
    }
}

/**
 * Executes the query XPath on the document again and strips
 * all the tags below the common parent of all split points.
 * What remains is a bare document skeleton with all splitpoints
 * and surroundings removed that can be used to fill in custom
 * content.
 *
 * Nice thing is that things outside the common parent, like
 * navigation bars or headers/footers, remain in the document.
 *
 * Returns the common parent node.
 */
xmlNodePtr strip_document(struct Splitter* p_splitter)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;
    xmlNodePtr p_parent_node     = NULL;
    xmlNodePtr p_node            = NULL;

    p_context = xmlXPathNewContext(p_splitter->p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST(p_splitter->splitexpr), p_context);

    p_parent_node = p_results->nodesetval->nodeTab[0]->parent;

    /* Clear the parent tag */
    p_node = xmlFirstElementChild(p_parent_node);
    while (p_node) {
        xmlUnlinkNode(p_node);
        xmlFreeNode(p_node); /* We do not need to resurrect it again */

        p_node = xmlFirstElementChild(p_parent_node);
    }

    xmlXPathFreeObject(p_results);
    xmlXPathFreeContext(p_context);

    return p_parent_node;
}
