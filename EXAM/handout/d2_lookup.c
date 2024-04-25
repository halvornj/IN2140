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
    if (id < 1000)
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
    int rc = d1_recv_data(client->peer, buffer, sz);
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
    free(buffer);
    /*
    fprintf(stderr, "AY CARAMBA START\n");
    //------------------ERROR IS AFTER THIS POINT----------
    free(buffer);
    fprintf(stderr, "AY CARAMBA END \n");
    //-----------------ERROR IS BEFORE THIS POINT---------
    */
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
    free(tree->nodes);
    free(tree);
}

int d2_add_to_local_tree(LocalTreeStore *nodes_out, int node_idx, char *buffer, int buflen)
{
    int num_nodes = (buflen - sizeof(PacketResponse)) / sizeof(NetNode);
    if (num_nodes > 5)
    {
        fprintf(stderr, "error: too many nodes in buffer");
        return TOO_MANY_NODES;
    }
    for (int i = node_idx; i < num_nodes + node_idx; i++)
    {
        NetNode *node = (NetNode *)buffer; /*read the first netNode-size bytes from buffer and use as netnode*/
        /*host order all the node values*/
        node->id = ntohl(node->id);
        node->value = ntohl(node->value);
        node->num_children = ntohl(node->num_children);
        /*host order all the child ids*/
        for (uint32_t j = 0; j < node->num_children; j++)
        {
            node->child_id[j] = ntohl(node->child_id[j]);
        }
        nodes_out->nodes[i] = (*node);
        buffer += sizeof(NetNode); /*move buffer to the next node*/
    }
    return node_idx + num_nodes;
}

void d2_print_tree(LocalTreeStore *nodes_out)
{
    for (int i = 0; i < nodes_out->number_of_nodes; i++)
    {
        printf("id %d value %d children %d\n", nodes_out->nodes[i].id, nodes_out->nodes[i].value, nodes_out->nodes[i].num_children);
        for (uint32_t j = 0; j < nodes_out->nodes[i].num_children; j++)
        {
            printf("--");
        }
        printf("\n");
    }
}
