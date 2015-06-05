#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include "config.h"
#include "split.h"

int main(int argc, char* argv[])
{
    xmlInitParser();

    struct Splitter* p_splitter = splitter_new();
    splitter_split_file(p_splitter, "../test.html", "/tmp/test");
    splitter_free(p_splitter);

    xmlCleanupParser();
    return 0;
}
