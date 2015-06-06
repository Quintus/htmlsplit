#ifndef HTMLSPLITTER_SPLIT_H
#define HTMLSPLITTER_SPLIT_H

/**
 * This structure holds the information about a title
 * and its anchor.
 */
struct SectionInfo {
    char title[8192];    /*< Title of the section */
    char filename[1024]; /*< Filename of the file the section is contained in */
    char anchor[8192];   /*< NAME attribute to target for linking to this section */

    struct SectionInfo* p_next; /*< Next section */
};

/**
 * Main structure of this program.
 */
struct Splitter {
    char splitexpr[4096];
    char infile[PATH_MAX];
    char outdir[PATH_MAX];
    char stdoutsep[1024];
    int secnum;
    bool interlink;
    int tocdepth;

    /***** Internal use *****/
    htmlDocPtr p_document;
    xmlNodePtr* p_following_nodes;
    xmlNodePtr* p_preceeding_nodes;
    int num_following_nodes;
    int num_preceeding_nodes;
    struct SectionInfo* p_sectioninfo;

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
