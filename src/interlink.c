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
 * Temporaryly add a <div> containing navigational elements
 * to the bottom of the common parent tag of all split points.
 * The added <div> tag is returned; pass it as an argument
 * to splitter_remove_interlinks() to get rid of it again.
 */
xmlNodePtr splitter_add_interlinks(struct Splitter* p_splitter, xmlNodePtr p_parent_node, int i, int total)
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

/**
 * Remove the temporary <div> added by splitter_add_interlinks()
 * from the document again and free it.
 */
void splitter_remove_interlinks(struct Splitter* p_splitter, xmlNodePtr p_interlinks_node)
{
    if (p_interlinks_node) {
        verbprintf("Removing interlinks section.\n");

        xmlUnlinkNode(p_interlinks_node);
        xmlFreeNode(p_interlinks_node);
    }
}
