/* ======================================================================
 * YOU CAN MODIFY THIS FILE.
 * ====================================================================== */

#ifndef D2_LOOKUP_MOD_H
#define D2_LOOKUP_MOD_H

#include "d1_udp.h"

struct D2Client
{
    D1Peer* peer;
};

typedef struct D2Client D2Client;

struct LocalTreeStore
{
    int number_of_nodes;
    struct NetNode* nodes;    /* I think I'm gonna go for an array of nodes */
};
typedef struct LocalTreeStore LocalTreeStore;

void print_node(LocalTreeStore* tree, uint32_t node_id, int level);
#endif /* D2_LOOKUP_MOD_H */
