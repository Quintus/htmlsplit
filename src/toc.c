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
static xmlChar* detect_target_anchor(struct Splitter* p_splitter, xmlNodePtr p_heading_node);
static xmlNodePtr copy_heading_contents(struct Splitter* p_splitter, xmlNodePtr p_heading_node);

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
            xmlNodePtr p_curhead = p_results->nodesetval->nodeTab[i];
            xmlChar* anchorid    = detect_target_anchor(p_splitter, p_curhead);

            /* If this heading as an ID attribute, remember it for later ToC generation. */
            if (anchorid && p_curhead->children) { /* some silly people use empty <h*> tags */
                struct SectionInfo* p_section = (struct SectionInfo*) malloc(sizeof(struct SectionInfo));

                verbprintf("Collecting heading for later ToC generation.\n");
                memset(p_section, '\0', sizeof(struct SectionInfo));

                /* Copy easy things */
                p_section->level = atoi(((char*) p_curhead->name) + 1); /* Strip leading “h” of h1, h2, etc. */
                strcpy(p_section->anchor, (char*) anchorid);
                sprintf(p_section->filename, "%04d.html", index);

                /* Copy the heading’s content */
                p_section->content_nodes = copy_heading_contents(p_splitter, p_curhead);
                if (!p_section->content_nodes) {
                    fprintf(stderr, "Warning: Failed to copy node list for ToC collection, skipping this heading.\n");
                    free(p_section);
                    /* continue; */
                    exit(ERR_PARSE); /* DEBUG */
                }

                /* Advance section linked list */
                if (p_last_section)
                    p_last_section->p_next = p_section;
                else /* First section info */
                    p_splitter->p_sectioninfo = p_section;
                p_last_section = p_section;

                /* Cleanup */
                xmlFree(anchorid);
            }
            else {
                verbprintf("This heading has either no anchor or no content, thus no entry in ToC possible.\n");
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
    xmlChar* toctitle = xmlCharStrdup(p_splitter->tocname);

    verbprintf("Generating Table of Contents.\n");
    p_parent_node = strip_document(p_splitter);

    /* Create base structure */
    p_node = xmlNewChild(p_parent_node, NULL, BAD_CAST("div"), NULL);
    xmlNewProp(p_node, BAD_CAST("class"), BAD_CAST("htmlsplit-toc"));

    xmlNewTextChild(p_node, NULL, BAD_CAST("h1"), toctitle);
    p_node = xmlNewChild(p_node, NULL, BAD_CAST("ul"), NULL);

    /* Add ToC items */
    p_list = p_node;
    p_section = p_splitter->p_sectioninfo;
    while(p_section) {
        char uri[1024];

        /* Honour user-specified depth limit */
        if (p_splitter->tocdepth < p_section->level) {
            verbprintf("Section has level %d, which is above the threshold of %d.\n", p_section->level, p_splitter->tocdepth);
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

        verbprintf("Adding section with level %d to ToC on level %d.\n", p_section->level, current_level);

        /* Add node */
        p_node = xmlNewChild(p_list, NULL, BAD_CAST("li"), NULL);
        p_node = xmlNewChild(p_node, NULL, BAD_CAST("a"), NULL);
        xmlNewProp(p_node, BAD_CAST("href"), BAD_CAST(uri));
        xmlAddChildList(p_node, p_section->content_nodes);

        /* Next */
        p_section = p_section->p_next;
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

    xmlFree(toctitle);
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

xmlChar* detect_target_anchor(struct Splitter* p_splitter, xmlNodePtr p_heading_node)
{
    xmlChar* result = NULL;
    xmlNodePtr p_node = NULL;

    /* Try 1: ID attribute in H* tag */
    result = xmlGetProp(p_heading_node, BAD_CAST("id"));

    if (result)
        return result;

    /* Try 2: NAME attribute in <a> tag as child tag */
    verbprintf("No ID attribute found. Trying NAME attribute in child <a> tag.\n");
    p_node = xmlFirstElementChild(p_heading_node);

    if (p_node) {
        result = xmlGetProp(p_node, BAD_CAST("name"));

        if (result) {
            return result;
        }
    }

    /* Try 3: <a> before h* tag */
    verbprintf("No NAME attribute in contained <a> found. Trying preceeding <a> tag.\n");
    p_node = xmlPreviousElementSibling(p_heading_node);

    if (p_node) {
        result = xmlGetProp(p_node, BAD_CAST("name"));

        if (result) {
            return result;
        }
    }

    /* Try 4: <a> after h* tag */
    verbprintf("No NAME attribute in preceeding <a> found. Trying following <a> tag.\n");
    p_node = xmlNextElementSibling(p_heading_node);

    if (p_node) {
        result = xmlGetProp(p_node, BAD_CAST("name"));

        if (result) {
            return result;
        }
    }

    /* Nothing found. */
    verbprintf("No target anchors found. This heading cannot be added to the ToC.\n");
    return NULL;
}

xmlNodePtr copy_heading_contents(struct Splitter* p_splitter, xmlNodePtr p_heading_node)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;
    xmlNodePtr p_copy = NULL;

    /* Here be dragons. I don’t understand the XPath query below myself, but it
     * works appearently. What it is supposed to do is to select all tags inside
     * the heading tag except <a> tags, for which only the text should be selected.
     * This ensures that headings with embedded anchors that are used to target
     * the heading (<h1><a name="foo" href="bar">text</a></h1> and variant constructions,
     * especially the insane <h1><a name="foo" href="bar">text</a> normal text</h1>)
     * get stripped out and replaced with their textual content. To complicate things,
     * this only applies if the <a> tag has a NAME attribute, because without it,
     * it can’t be targetted anyway and should just be copied over verbatim. */
    p_context = xmlXPathNewContext(p_splitter->p_document);
    p_results = xmlXPathNodeEval(p_heading_node, BAD_CAST("a[@name]/text()|node()[not(self::a[@name])]"), p_context);

    if (p_results && p_results->nodesetval->nodeNr > 0) {
        verbprintf("Heading with %d relevant nodes in it.\n", p_results->nodesetval->nodeNr);
        p_copy = xmlDocCopyNodeList(p_splitter->p_document, p_results->nodesetval->nodeTab[0]);
    }

    xmlXPathFreeObject(p_results);
    xmlXPathFreeContext(p_context);

    return p_copy;
}
