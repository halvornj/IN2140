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
#define MAX_PACKET_SIZE 1024 * sizeof(uint8_t)

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

    struct sockaddr_in addr;
    struct in_addr ip_addr;
    addr.sin_family = AF_INET;
    /*defaults to port 2311, todo default to 0, so long as htons() doesn't make a big stink*/
    addr.sin_port = htons(D1_UDP_PORT);

    int wc;
    wc = inet_pton(AF_INET, "0.0.0.0", &ip_addr.s_addr);
    /*ip addr error handling*/
    if (wc == 0)
    {
        fprintf(stderr, "inet_pton failed: invalid address\n");
        close(peer->socket);
        free(peer);
        return NULL;
    }
    if (wc < 0)
    {
        perror("inet_pton");
        close(peer->socket);
        free(peer);
        return NULL;
    }
    addr.sin_addr = ip_addr;

    peer->addr = addr;
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
    struct in_addr ip_addr;
    int wc;
    struct hostent *host_info;
    host_info = gethostbyname(peername);
    if (host_info == NULL)
    {
        perror("gethostbyname");
        return 0;
    }
    /*addr should be good*/
    memcpy(&ip_addr.s_addr, host_info->h_addr_list[0], sizeof(ip_addr.s_addr));
    peer->addr.sin_port = htons(server_port);
    return 1;
}

int d1_recv_data(struct D1Peer *peer, char *buffer, size_t sz)
{
    /*TODO checksum*/

    return 0;
}

int d1_wait_ack(D1Peer *peer, char *buffer, size_t sz)    /*i don't get it, is the buffer and sz the data buffer? I'm assuming it is, to use recursion*/
{
    int rc;
    char buff[sizeof(D1Header)];
    printf("DEBUG: waiting for ack\n");
    rc = recv(peer->socket, buff, sizeof(D1Header), 0);
    if (rc < 0)
    {
        perror("recv");
        return -1;
    }
    D1Header *header = (D1Header *)buff;
    if ((header->flags & ACKNO)!=peer->next_seqno)
    {
        printf("ackno: %d, next_seqno: %d. retrying...\n", header->flags & ACKNO, peer->next_seqno);
        d1_send_data(peer, buffer, sz);
    }
    /*the seqno should now match*/
    peer->next_seqno = !peer->next_seqno;
    return 1;
}

int d1_send_data(D1Peer *peer, char *buffer, size_t sz)
{
    #define PACKET_TOO_LARGE_ERROR -1
    #define MALLOC_ERROR -2
    #define SENDTO_ERROR -3

    printf("DEBUG: sending data: \"%s\", size %zu\n", buffer, sz);

    /*assuming, for now, that sz is the size of *buffer */
    if (sz > (MAX_PACKET_SIZE - sizeof(D1Header))) // if the size of the incomming buffer is greater than the max packet size minus the header size
    {
        fprintf(stderr, "error: size of data for data-packet is too large."); // todo double check error output
        return PACKET_TOO_LARGE_ERROR;
    }
    D1Packet *packet = (D1Packet *)malloc(sizeof(D1Packet));    /* allocate memory for the packet*/
    if (packet == NULL)    /*check that */
    {
        perror("malloc");
        return MALLOC_ERROR;
    }
    packet->header = (D1Header *)malloc(sizeof(D1Header));
    if (packet->header == NULL)
    {
        perror("malloc");
        free(packet);
        return MALLOC_ERROR;
    }
    packet->header->flags = FLAG_DATA;
    if (peer->next_seqno)
    { // set the seqno flag to 1 if the next seqno is 1, otherwise leave it at 0
        packet->header->flags |= SEQNO;
    }

    packet->header->size = sz;

    packet->data = (uint8_t *)malloc(sz); /*allocate memory for the data*/
    if (packet->data == NULL)
    {
        perror("malloc");
        free(packet->header);
        free(packet);
        return MALLOC_ERROR;
    }
    memcpy(packet->data, buffer, sz);
    packet->header->checksum = compute_checksum(packet);

    int wc;
    printf("sending to %s:%d\n", inet_ntoa(peer->addr.sin_addr), ntohs(peer->addr.sin_port));
    wc = sendto(
        peer->socket,
        packet,
        sizeof(D1Header) + sz,
        0,
        (struct sockaddr *)&peer->addr,
        sizeof(peer->addr));
    printf("DEBUG: sent %d bytes\n", wc);
    if (wc < 0)
    {
        perror("sendto");
        free(packet->header);
        free(packet->data);
        free(packet);
        return SENDTO_ERROR;
    }

    d1_wait_ack(peer, buffer, sz);

    /*everything should have worked. free everything and return.*/
    free(packet->header);
    free(packet->data);
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

int compute_checksum(D1Packet *packet)
{
    // todo
    return 0;
}