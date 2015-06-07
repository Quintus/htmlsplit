#ifndef HTMLSPLIT_TOC_H
#define HTMLSPLIT_TOC_H

/**
 * This structure holds the information about a title
 * and its anchor.
 */
struct SectionInfo {
    xmlNodePtr content_nodes; /*< Contents of the <h*> tag, as an xmlNodePtr array */
    char filename[1024]; /*< Filename of the file the section is contained in */
    char anchor[8192];   /*< NAME attribute to target for linking to this section */
    int level;           /*< Level. 1 for h1, 2 for h2, etc. */

    struct SectionInfo* p_next; /*< Next section */
};

void splitter_generate_tocfile(struct Splitter* p_splitter);

void splitter_collect_toc_info(struct Splitter* p_splitter, int index); /*< \private */

#endif
