#ifndef HTMLSPLITTER_SPLIT_H
#define HTMLSPLITTER_SPLIT_H

/**
 * Main structure of this program.
 */
struct Splitter {
    unsigned short level;
    const char* langtag;
    char infile[PATH_MAX];
    char outdir[PATH_MAX];

    /***** Internal use *****/
    xmlNodePtr* p_following_nodes;
    xmlNodePtr* p_preceeding_nodes;
    int num_following_nodes;
    int num_preceeding_nodes;

    bool terminate;
};

/**
 * Error codes used by this program.
 */
enum errcode {
    ERR_SUCCESS = 0,
    ERR_CLI,
    ERR_IO,
    ERR_PARSE,
    ERR_MEM
};

struct Splitter* splitter_new();
void splitter_free(struct Splitter* ptr);

void splitter_split_file(struct Splitter* p_splitter);

#endif
