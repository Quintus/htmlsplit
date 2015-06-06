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

void splitter_generate_tocfile(struct Splitter* p_splitter)
{
    struct SectionInfo* p_section = p_splitter->p_sectioninfo;

    while(p_section) {
        printf("TITLE: %s | ANCHOR: %s | FILENAME: %s\n", p_section->title, p_section->anchor, p_section->filename);
        p_section = p_section->p_next;
    }
}
