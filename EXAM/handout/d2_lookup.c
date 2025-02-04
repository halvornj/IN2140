/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "d2_lookup.h"

#define ID_TOO_LOW_ERROR -1
#define SEND_ERROR -2
#define NON_MATCHING_PACKET_TYPE -3
#define TOO_MANY_NODES -4
#define CALLOC_ERROR -5

D2Client *d2_client_create(const char *server_name, uint16_t server_port)
{
    D2Client *client = (D2Client *)malloc(sizeof(D2Client));
    if (!client)
    {
        perror("malloc");
        return NULL;
    }
    /*i've forgotten - do i malloc this shit first?*/
    client->peer = d1_create_client();
    if (!client->peer)
    {
        free(client);
        return NULL;
    }
    if (d1_get_peer_info(client->peer, server_name, server_port) == 0)
    {
        /*uhhh do i free here? suppose i do innit*/
        d1_delete(client->peer);
        free(client);
        return NULL;
    }
    return client;
}

D2Client *d2_client_delete(D2Client *client)
{
    if (client != NULL)
    {
        d1_delete(client->peer);
        free(client);
    }
    return NULL;
}

int d2_send_request(D2Client *client, uint32_t id)
{
    if (id <= 1000)
    {
        fprintf(stderr, "error: id %d for d2_send_request must be over 1000", id);
        return ID_TOO_LOW_ERROR;
    }

    /*
    printf("DEBUG: send_request type: %d\n", (uint16_t)TYPE_REQUEST);
    printf("DEBUG: send_request id: %d\n", id);
    */

    PacketRequest *reqpacket = (PacketRequest *)calloc(1, sizeof(PacketRequest));
    reqpacket->type = htons((uint16_t)TYPE_REQUEST);
    reqpacket->id = htonl(id);

    char *outbuffr = (char *)calloc(1, sizeof(PacketRequest));
    if (!outbuffr)
    {
        free(reqpacket);
        perror("calloc");
        return CALLOC_ERROR;
    }
    memcpy(outbuffr, reqpacket, sizeof(PacketRequest));
    /*
    printf("DEBUG: after packet insertion (now in network byte order)\n:");
    printf("DEBUG: send_request packet-> type: %d\n", reqpacket->type);
    printf("DEBUG send_request packet->id = %d\n", reqpacket->id);
    */

    int rc;
    rc = d1_send_data(client->peer, outbuffr, sizeof(PacketRequest));
    if (rc < 0)
    {
        perror("sendto");
        free(reqpacket);
        free(outbuffr);
        return SEND_ERROR;
    }
    /*things should be good)*/
    free(reqpacket);
    free(outbuffr);
    return 1;
}

int d2_recv_response_size(D2Client *client)
{
    char *buffer = (char *)calloc(1, sizeof(PacketResponseSize));
    int rc = d1_recv_data(client->peer, buffer, sizeof(PacketResponseSize));
    if (rc < 0)
    {
        return rc;
    }
    PacketHeader *header = (PacketHeader *)buffer; /*cast buffer to header first to check type*/
    if (ntohs(header->type) != TYPE_RESPONSE_SIZE)
    {
        fprintf(stderr, "error: expected response size packet, got something else");
        free(buffer);
        return NON_MATCHING_PACKET_TYPE;
    }
    /*cast buffer to PacketResponseSize, because we know that's the type from the header*/
    PacketResponseSize *responsePacket = (PacketResponseSize *)buffer;
    uint16_t size = ntohs(responsePacket->size);
    // printf("DEBUG: response-size = %d", size);
    free(buffer);
    return size;
}

int d2_recv_response(D2Client *client, char *buffer, size_t sz) /*if i remember correctly i handle buffer size errors in d1*/
{
    int rc;
    rc = d1_recv_data(client->peer, buffer, sz);
    if (rc < 0)
    {
        return rc;
    }
    PacketHeader *header = (PacketHeader *)buffer;
    if (ntohs(header->type) != TYPE_RESPONSE && ntohs(header->type) != TYPE_LAST_RESPONSE)
    {
        fprintf(stderr, "error: expected response packet, got something else");
        return NON_MATCHING_PACKET_TYPE;
    }
    return rc;
}

LocalTreeStore *d2_alloc_local_tree(int num_nodes)
{
    LocalTreeStore *tree = (LocalTreeStore *)malloc(sizeof(LocalTreeStore));
    if (!tree)
    {
        perror("malloc");
        return NULL;
    }
    tree->number_of_nodes = num_nodes;
    tree->nodes = (NetNode *)calloc(num_nodes, sizeof(NetNode));
    if (!tree->nodes)
    {
        perror("calloc");
        free(tree);
        return NULL;
    }
    return tree;
}

void d2_free_local_tree(LocalTreeStore *tree)
{
    free(tree->nodes); /*free the children!*/
    free(tree);
}

int d2_add_to_local_tree(LocalTreeStore *nodes_out, int node_idx, char *buffer, int buflen)
{
    int num_nodes = buflen / 3*sizeof(uint32_t);
    if (num_nodes >5){
        num_nodes = 5;
    }
    for (int i = 0; i < num_nodes; i++)
    {
        NetNode *node = (NetNode *) calloc(1, sizeof(NetNode));
        /*first 4 bytes are id, next 4 bytes are value, next 4 bytes are num_children*/
        /*because the child-id[]-length is variable, I've chosen to advance the buffer for every read, just to make it crystal clear.
        I could advance it the (num_children+3)*sizeof(uint32_t), but this makes it clear i read, and then advance the buffer*/
        uint32_t id;
        memcpy(&id, buffer, sizeof(uint32_t));
        buffer += sizeof(uint32_t);
        uint32_t value;
        memcpy(&value, buffer, sizeof(uint32_t));
        buffer += sizeof(uint32_t);
        uint32_t num_children;
        memcpy(&num_children, buffer, sizeof(uint32_t));
        buffer += sizeof(uint32_t);

        node->id = ntohl(id);
        node->value = ntohl(value);
        node->num_children = ntohl(num_children);
        //printf("DEBUG:host order: id = %d, value= %d, num_children=%d\n", node->id,node->value, node->num_children);
        /*host order all the child ids*/
        for (uint32_t j = 0; j < node->num_children; j++)
        {
            memcpy(&node->child_id[j], buffer, sizeof(uint32_t));
            buffer += sizeof(uint32_t);
            node->child_id[j] = ntohl(node->child_id[j]);
        }
        nodes_out->nodes[node_idx] = (*node);
        node_idx++;
        free(node);
    }
    return node_idx;
}
void print_node(LocalTreeStore* tree, uint32_t node_id, int level)
{
    /*Find the node with the given id in the tree. Because id-s are assigned by dfs, index == id. actually a pretty neat map*/
    NetNode* node = &tree->nodes[node_id];

    /*If the node was not found, return */
    if (!node) return;

    /*Print the node details with the appropriate indentation */
    for (int i = 0; i < level; i++) printf("--");
    //printf("id %d value %d children %d\n", node->id, node->value, node->num_children);
    printf("id %d value %d children %d\n", node->id, node->value, node->num_children);
    /* For each child of the current node, call this function recursively with an increased indentation level */
    for (uint32_t i = 0; i < node->num_children; i++) {
        print_node(tree, node->child_id[i], level + 1);
    }
}

void d2_print_tree(LocalTreeStore* tree) {
    /* Start from the root node (node with id 0) and call the helper function with indentation level 0 */
    print_node(tree, 0, 0);
}