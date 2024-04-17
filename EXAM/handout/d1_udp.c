/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "d1_udp.h"

#define D1_UDP_PORT 2311
#define MAX_PACKET_SIZE 1024*uint8_t

D1Peer* d1_create_client( )
{
    D1Peer* peer = (D1Peer*)malloc( sizeof(D1Peer) );
    if( peer == NULL ){
        perror( "malloc" );
        return NULL;
    }
    peer -> socket = socket(AF_INET, SOCK_DGRAM, 0);

    if( peer -> socket < 0 ){
        perror( "socket" );
        free( peer );
        return NULL;
    }

    struct sockaddr_in addr;
    struct in_addr ip_addr;
    addr.sin_family = AF_INET;
    /*defaults to port 2311, todo default to 0, so long as htons() doesn't make a big stink*/
    addr.sin_port = htons(D1_UDP_PORT);

    int wc;
    wc = inet_pton(AF_INET, "0.0.0.0", &ip_addr.s_addr);
    /*ip addr error handling*/
    if( wc == 0 ){
        fprintf( stderr, "inet_pton failed: invalid address\n" );
        close( peer -> socket );
        free( peer );
        return NULL;
    }
    if(wc < 0){
        perror("inet_pton");
        close(peer -> socket);
        free(peer);
        return NULL;
    }
    addr.sin_addr = ip_addr;

    peer -> addr = addr;
    peer -> next_seqno = 0;
    return peer;
}

D1Peer* d1_delete( D1Peer* peer )
{
    //surely it isn't this easy??
    //do i free the peer.addr as well? but theyre not pointers theyre structs
    if( peer != NULL ){
        close( peer -> socket );
        free( peer );
    }

    return NULL;
}

int d1_get_peer_info( struct D1Peer* peer, const char* peername, uint16_t server_port )
{
    struct in_addr ip_addr;
    int wc;
    wc = inet_pton(AF_INET, peername, &ip_addr.s_addr);
    /*ip addr error handling*/
    if( wc == 0 ){
        fprintf( stderr, "inet_pton failed: invalid address\n" );
        return 0;
    }
    if(wc < 0){
        perror("inet_pton");
        return 0;
    }
    /*addr should be good*/
    peer-> addr.sin_addr = ip_addr;
    peer-> addr.sin_port = htons(server_port);
    return 1;
}

int d1_recv_data( struct D1Peer* peer, char* buffer, size_t sz )
{

    return 0;
}

int d1_wait_ack( D1Peer* peer, char* buffer, size_t sz )
{
    /* This is meant as a helper function for d1_send_data.
     * When D1 data has send a packet, this one should wait for the suitable ACK.
     * If the arriving ACK is wrong, it resends the packet and waits again.
     *
     * Implementation is optional.
     */
    return 0;
}

int d1_send_data( D1Peer* peer, char* buffer, size_t sz )
{
    /* implement this */
    return 0;
}

void d1_send_ack( struct D1Peer* peer, int seqno )
{
    //trying to only send the header for the ack, not the whole packet
    D1Header* header = (D1Header*)malloc( sizeof(D1Header) );
    if( header == NULL ){
        perror( "malloc" );
        return;
    }
    header->flags = FLAG_ACK;   //this is an ack packet

    if( seqno == 1){    //set the ackno flag if the incoming seqno is 1, else leave it as 0.
        header ->flags |= ACKNO;
    }
    int wc;
    wc = sendto(
        peer -> socket,
        header,
        sizeof(D1Header),
        0,
        (struct sockaddr*)&peer -> addr,
        sizeof(peer -> addr));
    if( wc < 0 ){
        perror( "sendto" );
        free( header );
        return;
    }
    free( header );
    return;
}

/*
custom helper methods:
 */
