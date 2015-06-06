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
#include "verbose.h"

void splitter_collect_toc_info(struct Splitter* p_splitter, int index)
{
    xmlXPathContextPtr p_context = NULL;
    xmlXPathObjectPtr p_results  = NULL;

    p_context = xmlXPathNewContext(p_splitter->p_document);
    p_results = xmlXPathEvalExpression(BAD_CAST("//h1|//h2|//h3|//h4|//h5|//h6"), p_context);

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

                titlestr = xmlNodeListGetString(p_splitter->p_document, p_results->nodesetval->nodeTab[0]->xmlChildrenNode, 1);

                strcpy(p_section->anchor, (char*) anchorid);
                strcpy(p_section->title, (char*) titlestr);
                sprintf(p_section->filename, "%04d", index);

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

void splitter_generate_tocfile(struct Splitter* p_splitter)
{
    struct SectionInfo* p_section = p_splitter->p_sectioninfo;

    while(p_section) {
        printf("TITLE: %s | ANCHOR: %s | FILENAME: %s\n", p_section->title, p_section->anchor, p_section->filename);
        p_section = p_section->p_next;
    }
}
