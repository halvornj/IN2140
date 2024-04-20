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
#define MAX_PACKET_SIZE 1024

D1Peer *d1_create_client()
{
    D1Peer *peer = (D1Peer *)malloc(sizeof(D1Peer));
    if (peer == NULL)
    {
        perror("malloc");
        return NULL;
    }
    peer->socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (peer->socket < 0)
    {
        perror("socket");
        free(peer);
        return NULL;
    }

    peer->next_seqno = 0;
    return peer;
}

D1Peer *d1_delete(D1Peer *peer)
{
    // surely it isn't this easy??
    // do i free the peer.addr as well? but theyre not pointers theyre structs
    if (peer != NULL)
    {
        close(peer->socket);
        free(peer);
    }

    return NULL;
}

int d1_get_peer_info(struct D1Peer *peer, const char *peername, uint16_t server_port)
{
    struct sockaddr_in addr;
    struct in_addr ip_addr;
    int wc;
    struct hostent *host_info;


    addr.sin_family = AF_INET;
    addr.sin_port = htons(D1_UDP_PORT);
    host_info = gethostbyname(peername);
    if (host_info == NULL)
    {
        perror("gethostbyname");
        return 0;
    }
    ip_addr = *(struct in_addr *)host_info->h_addr_list[0];
    //todo check if the ip_addr is valid
    addr.sin_addr = ip_addr;
    /*addr should be good*/
    peer->addr = addr;
    return 1;
}

int d1_recv_data(struct D1Peer *peer, char *buffer, size_t sz)
{    /*the comment in d1_udp.h says to return negative numbers in case of error, I'm interpreting wrong checksum or size as errors.*/
    #define RECVFROM_ERROR -1
    #define CHECKSUM_ERROR -2
    #define CALLOC_ERROR -3


    uint8_t *packet = (uint8_t *)calloc(1,MAX_PACKET_SIZE);
    if (packet == NULL)
    {
        perror("calloc");
        return CALLOC_ERROR;
    }
    int rc;
    rc = recv(peer->socket, packet, MAX_PACKET_SIZE, 0);
    if (rc < 0)
    {
        perror("recv");
        free(packet);
        return RECVFROM_ERROR;
    }
    D1Header *header = (D1Header *)packet;    /*cast the packet to a D1Header struct, should work because we only cast the first(D1HEADER) bytes?*/
    if(ntohl(header->size) != rc)    /*check if the size of the packet is the same as the size in the header, converted from network order to host order*/
    {
        d1_send_ack(peer, !(header->flags & SEQNO));    /*send an ack with the opposite of the seqno flag, triggers retransmit*/
        free(packet); /*free the packet and return*/
        return CHECKSUM_ERROR;
    }

    if (ntohs(header->checksum) != compute_checksum(packet, rc))
    {
        d1_send_ack(peer, !(header->flags & SEQNO));    /*send an ack with the opposite of the seqno flag, triggers retransmit. seqno could also be the peer->seqno.*/
        free(packet);
        return CHECKSUM_ERROR;
    }

    /*the packet should be properly formed, we can now copy the data to the buffer */
    memcpy(buffer, packet + sizeof(D1Header), rc - sizeof(D1Header));    /*copy the data from the packet to the buffer. we offset the header data in the src*/
    printf("recieved data: \"%s\", size %lu. acking...\n", buffer, rc - sizeof(D1Header));
    d1_send_ack(peer, header->flags & SEQNO);    /*send an ack with the seqno flag*/
    free(packet);
    return rc - sizeof(D1Header);    /*return the size of the data*/

}

int d1_wait_ack(D1Peer *peer, char *buffer, size_t sz)    /*i don't get it, is the buffer and sz the data buffer? I'm assuming it is, to use recursion*/
{
    int rc;
    char buff[sizeof(D1Header)];
    printf("waiting for ack...\n");
    //todo add timeout
    rc = recv(peer->socket, buff, sizeof(D1Header), 0);
    if (rc < 0)
    {
        perror("recv");
        return -1;
    }
    D1Header *header = (D1Header *)buff;
    if (!(ntohs(header->flags) & FLAG_ACK))
    {
        printf("WAIT_ACK_ERROR: recieved packet is not an ack. Aborting...\n");
        return -2;    //todo define the return codes at top
    }

    int incoming_ackno = (ntohs(header->flags) & ACKNO);
    if (incoming_ackno != peer->next_seqno)
    {
        printf("WAIT_ACK_ERROR: recieved ackno: %d, next_seqno is: %d. retrying...\n", incoming_ackno, peer->next_seqno);
        d1_send_data(peer, buffer, sz);
    }
    else{
    /*the seqno should now match*/
    peer->next_seqno = !peer->next_seqno;
    printf("WAIT_ACK_SUCCESS: ack recieved. new seqno: %d\n",peer->next_seqno);
    return 1;
    }
    return -1;
}

int d1_send_data(D1Peer *peer, char *buffer, size_t sz)
{
    #define PACKET_TOO_LARGE_ERROR -1
    #define CHECKSUM_ERROR -2
    #define CALLOC_ERROR -3
    #define MALLOC_ERROR -4
    #define SENDTO_ERROR -5

    printf("sending data: \"%s\", size %zu (with header)\n", buffer, sz+sizeof(D1Header));

    /*assuming, for now, that sz is the size of *buffer */
    if (sz > (MAX_PACKET_SIZE - sizeof(D1Header))) // if the size of the incomming buffer is greater than the max packet size minus the header size
    {
        fprintf(stderr, "error: size of data for data-packet is too large."); // todo double check error output
        return PACKET_TOO_LARGE_ERROR;
    }
    D1Header *header = (D1Header *)calloc(1,sizeof(D1Header));
    if (header == NULL)
    {
        perror("calloc");
        return CALLOC_ERROR;
    }
    header->size = sz+sizeof(D1Header);
    header->size = htonl(header->size); // convert the size to network byte order

    header->flags = FLAG_DATA;
    header->flags |= peer->next_seqno << 7; // set the seqno flag to the next_seqno value in the peer struct
    header -> flags = htons(header->flags); // convert the flags to network byte order

    uint8_t *packet = (uint8_t *)calloc(1, sizeof(D1Header) + sz);    /*allocate memory for the packet, should always be at most 1024*/


    if (packet == NULL)
    {
        perror("calloc");
        free(header);
        return CALLOC_ERROR;
    }
    memcpy(packet, header, sizeof(D1Header));
    memcpy(packet + sizeof(D1Header), buffer, sz);

    header->checksum = htons(compute_checksum(packet, sizeof(D1Header) + sz)); /*compute the checksum of the packet when all the relevant data has been filled*/
    memcpy(packet, header, sizeof(D1Header)); /*copy the header back into the packet, to override the 0-checksum*/



    int wc;
    //printf("sending to %s:%d\n", inet_ntoa(peer->addr.sin_addr), ntohs(peer->addr.sin_port));
    wc = sendto(
        peer->socket,
        packet,
        sizeof(D1Header) + sz,
        0,
        (struct sockaddr *)&peer->addr,
        sizeof(peer->addr));
    printf("sent %d bytes\n", wc);
    if (wc < 0)
    {
        perror("sendto");
        free(header);
        free(packet);
        return SENDTO_ERROR;
    }

    d1_wait_ack(peer, buffer, sz);

    /*everything should have worked. free everything and return.*/
    free(header);
    free(packet);
    return wc;
}

void d1_send_ack(struct D1Peer *peer, int seqno)
{
    // trying to only send the header for the ack, not the whole packet
    D1Header *header = (D1Header *)malloc(sizeof(D1Header));
    if (header == NULL)
    {
        perror("malloc");
        return;
    }
    header->flags = FLAG_ACK; // this is an ack packet

    if (seqno == 1)
    { // set the ackno flag if the incoming seqno is 1, else leave it as 0.
        header->flags |= ACKNO;
    }
    header->flags = htons(header->flags);//set to network byte order

    int wc;
    wc = sendto(
        peer->socket,
        header,
        sizeof(D1Header),
        0,
        (struct sockaddr *)&peer->addr,
        sizeof(peer->addr));
    if (wc < 0)
    {
        perror("sendto");
        free(header);
        return;
    }
    free(header);
    return;
}

/*
custom helper methods:
 */

uint16_t compute_checksum(uint8_t *packet, size_t sz)
{
    uint8_t odd = 0;
    uint8_t even = 0;

    for(int i = 0; i < sz; i++)
    {
        if (i % 2 == 0)
        {
            even ^= packet[i];
        }
        else
        {
            odd ^= packet[i];
        }
    }
    return (even << 8) | odd;
}