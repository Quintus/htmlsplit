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

    /*
    xmlDocPtr document          = NULL;
    xmlXPathContextPtr xcontext = NULL;
    xmlXPathObjectPtr xobject   = NULL;
    xmlChar* path                = xmlCharStrdup("/root/test");

    document = xmlParseFile("../test.xml");
    xcontext = xmlXPathNewContext(document);

    xobject = xmlXPathEvalExpression(path, xcontext);

    printf("Found %d nodes.\n", xobject->nodesetval->nodeNr);
    printf("First node: '%s'.\n", xobject->nodesetval->nodeTab[0]->name);
    printf("Text: >%s<\n", xmlNodeListGetString(document, xobject->nodesetval->nodeTab[0]->children, 1));

    xmlXPathFreeObject(xobject);
    xmlXPathFreeContext(xcontext);
    xmlFreeDoc(document);
    free(path);
    */

    xmlCleanupParser();
    return 0;
}
