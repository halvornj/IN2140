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

D2Client* d2_client_create( const char* server_name, uint16_t server_port )
{
    D2Client* client = (D2Client*)malloc( sizeof(D2Client) );
    if( ! client )
    {
        perror("malloc");
        return NULL;
    }
    /*i've forgotten - do i malloc this shit first?*/
    client->peer = d1_create_client();
    if( ! client->peer )
    {
        free(client);
        return NULL;
    }
    if(d1_get_peer_info(client->peer, server_name, server_port) ==0){
        return NULL;
    }
    return client;
}

D2Client* d2_client_delete( D2Client* client )
{
    if(client!=NULL){
        d1_delete(client->peer);
        free(client);
    }
    return NULL;
}

int d2_send_request( D2Client* client, uint32_t id )
{
    if(id < 1000){
        fprintf(stderr, "error: id %d for d2_send_request must be over 1000", id);
        return ID_TOO_LOW_ERROR;
    }

    PacketRequest * reqpacket = (PacketRequest*) calloc(1, sizeof(PacketRequest));

    reqpacket ->type = htons(TYPE_REQUEST);
    reqpacket ->id = htonl(id);

    char *outbuffr;
    memcpy(reqpacket, outbuffr, sizeof(PacketRequest));

    int rc;
    rc = d1_send_data(client->peer, outbuffr, sizeof(PacketRequest));
    if(rc < 0){
        return SEND_ERROR;
    }
    /*things should be good)*/
    return 1;
}

int d2_recv_response_size( D2Client* client )
{
   char *buffer = (char*)calloc(1, sizeof(PacketResponseSize));
    int rc = d1_recv_data(client->peer, buffer, sizeof(PacketResponseSize));
    if(rc < 0){
        return rc;
    }
    PacketHeader * header = (PacketHeader*)buffer;    /*cast buffer to header first to check type*/
    if(ntohs(header->type) != TYPE_RESPONSE_SIZE){
        fprintf(stderr, "error: expected response size packet, got something else");
        free(buffer);
        free(header);
        return NON_MATCHING_PACKET_TYPE;
    }
    free(header); /*free header, because we don't need it anymore*/
    /*cast buffer to PacketResponseSize, because we know that's the type from the header*/
    PacketResponseSize * responsePacket = (PacketResponseSize*)buffer;
    return ntohs(responsePacket->size);
}

int d2_recv_response( D2Client* client, char* buffer, size_t sz )/*if i remember correctly i handle buffer size errors in d1*/
{
    int rc = d1_recv_data(client->peer, buffer, sz);
    if(rc < 0){
        return rc;
    }
    PacketHeader * header = (PacketHeader*)buffer;
    if(ntohs(header->type) != TYPE_RESPONSE && ntohs(header->type) != TYPE_LAST_RESPONSE){
        fprintf(stderr, "error: expected response packet, got something else");
        return NON_MATCHING_PACKET_TYPE;
    }
    return rc;
}

LocalTreeStore* d2_alloc_local_tree( int num_nodes )
{
    LocalTreeStore* store = (LocalTreeStore*)malloc(sizeof(LocalTreeStore));
    if(!store){
        perror("malloc");
        return NULL;
    }
    store->number_of_nodes = num_nodes;
    store->nodes = (NetNode*)calloc(num_nodes, sizeof(NetNode));
    if(!store->nodes){
        perror("calloc");
        free(store);
        return NULL;
    }
    return store;
}

void  d2_free_local_tree( LocalTreeStore* nodes )
{
    /* implement this */
}

int d2_add_to_local_tree( LocalTreeStore* nodes_out, int node_idx, char* buffer, int buflen )
{
    /* implement this */
    return 0;
}

void d2_print_tree( LocalTreeStore* nodes_out )
{
    /* implement this */
}

