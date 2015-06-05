#ifndef HTMLSPLITTER_SPLIT_H
#define HTMLSPLITTER_SPLIT_H

/**
 * Main structure of this program.
 */
struct Splitter {
    unsigned short level;
    const char* langtag;

    /***** Internal use *****/
    xmlNodePtr* p_following_nodes;
    xmlNodePtr* p_preceeding_nodes;
    int num_following_nodes;
    int num_preceeding_nodes;
};

struct Splitter* splitter_new();
void splitter_free(struct Splitter* ptr);

void splitter_split_file(struct Splitter* p_splitter, const char* infile, const char* outdir);

#endif
