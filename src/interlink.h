#ifndef HTMLSPLIT_INTERLINK_H
#define HTMLSPLIT_INTERLINK_H

xmlNodePtr splitter_add_interlinks(struct Splitter* p_splitter, xmlNodePtr p_parent_node, int i, int total); /*< \private */
void splitter_remove_interlinks(struct Splitter* p_splitter, xmlNodePtr p_interlinks_node); /*< \private */

#endif
