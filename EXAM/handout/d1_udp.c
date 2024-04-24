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
#include <errno.h>

#include "d1_udp.h"

#define D1_UDP_PORT 2311
#define MAX_PACKET_SIZE 1024

#define RECVFROM_ERROR -1
#define CHECKSUM_ERROR -2
#define SIZE_ERROR -3
#define CALLOC_ERROR -4
#define RECV_BUFFER_TOO_SMALL -5
#define SEND_PACKET_TOO_LARGE_ERROR -6
#define SENDTO_ERROR -7
#define WAIT_ACK_ERROR -8

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
    struct hostent *host_info;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    host_info = gethostbyname(peername);
    if (host_info == NULL)
    {
        perror("gethostbyname");
        return 0;
    }
    ip_addr = *(struct in_addr *)host_info->h_addr_list[0];
    // todo check if the ip_addr is valid
    addr.sin_addr = ip_addr;
    /*addr should be good*/
    peer->addr = addr;
    return 1;
}

int d1_recv_data(struct D1Peer *peer, char *buffer, size_t sz)
{ /*the comment in d1_udp.h says to return negative numbers in case of error, I'm interpreting wrong checksum or size as errors.*/

    uint8_t *packet = (uint8_t *)calloc(1, MAX_PACKET_SIZE);
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
    D1Header *header = (D1Header *)packet; /*cast the packet to a D1Header struct, should work because we only cast the first(D1HEADER) bytes?*/

    /*check the size of the packet matches the size in the headerø*/
    if ((int)ntohl(header->size) != rc) /*check if the size of the packet is the same as the size in the header, converted from network order to host order*/
    {
        d1_send_ack(peer, !(header->flags & SEQNO)); /*send an ack with the opposite of the seqno flag, triggers retransmit*/
        free(packet);                                /*free the packet and return*/
        return SIZE_ERROR;
    }

    uint16_t incoming_checksum = ntohs(header->checksum);
    header->checksum = 0;                                      /*set the checksum to 0 to compute the checksum of the packet without the checksum*/
    uint16_t computed_checksum = compute_checksum(packet, rc); /*compute the checksum of the packet, without xor-ing the checksum. this allows me to reuse the simple checksum-function i wrote for send.*/

    if (incoming_checksum != computed_checksum)
    {
        fprintf(stderr, "checksum error. computed checksum: %d, packet checksum: %d\n", compute_checksum(packet, rc), ntohs(header->checksum));

        d1_send_ack(peer, !(header->flags & SEQNO)); /*send an ack with the opposite of the seqno flag, triggers retransmit. seqno could also be the peer->seqno.*/
        free(packet);
        return CHECKSUM_ERROR;
    }

    /*the packet should be properly formed, we can now copy the data to the buffer */
    /*first, check that the buffer is large enough, just for safety :)*/
    if (sz < (rc - sizeof(D1Header)))
    {
        fprintf(stderr, "Error: the buffer passed to the recv_data was not large enough to hold the data. quitting...");
        free(packet);
        return (RECV_BUFFER_TOO_SMALL);
    }
    memcpy(buffer, packet + sizeof(D1Header), rc - sizeof(D1Header)); /*copy the data from the packet to the buffer. we offset the header data in the src*/

    int ackno = ntohs(header->flags) & SEQNO;
    if (ackno) /*the ackno is currently the seqno-flag isolated in the entire flag number, i only want 0 or 1 in an int*/
    {          /*okay look - could i do this with some bitwise? yes. will i stick to this? also yes*/
        ackno = 1;
    }

    d1_send_ack(peer, ackno); /*send an ack with the seqno flag*/
    free(packet);
    return rc - sizeof(D1Header); /*return the size of the data*/
}

int d1_wait_ack(D1Peer *peer, char *buffer, size_t sz) /*i don't get it, is the buffer and sz the data buffer? I'm assuming it is, to use recursion*/
{
    int rc;
    char buff[sizeof(D1Header)];

    // todo add timeout
    struct timeval tv;
    tv.tv_sec = 1;  // 1 second timeout
    tv.tv_usec = 0; // 0 microseconds
    if (setsockopt(peer->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }

    //printf("waiting for ack...\n");

    rc = recv(peer->socket, buff, sizeof(D1Header), 0);
    if (rc < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {    /*timeout occurred: with the setsockopt() the recv will return -1 and set errno to EAGAIN or EWOULDBLOCK (idk what these are, but thats the documentation so...*/
            fprintf(stderr, "Timeout occurred while waiting for ACK, resending data...\n");
            d1_send_data(peer, buffer, sz);
            return d1_wait_ack(peer, buffer, sz);
        } else {
            perror("recv");
            return -1;
        }
    }
    D1Header *header = (D1Header *)buff;
    if (!((ntohs(header->flags) & FLAG_ACK)))
    {
        fprintf(stderr, "WAIT_ACK_ERROR: recieved packet is not an ack. Aborting...\n");
        return -2; // todo define the return codes at top
    }

    int incoming_ackno = (ntohs(header->flags) & ACKNO);
    if (incoming_ackno != peer->next_seqno)
    {
        fprintf(stderr, "WAIT_ACK_ERROR: recieved ackno: %d, next_seqno is: %d. retrying...\n", incoming_ackno, peer->next_seqno);
        d1_send_data(peer, buffer, sz);
    }
    else
    {
        /*the seqno should now match*/
        peer->next_seqno = !peer->next_seqno;
        //printf("WAIT_ACK_SUCCESS: ack recieved. new seqno: %d\n", peer->next_seqno);
        return 1;
    }
    return -1;
}

int d1_send_data(D1Peer *peer, char *buffer, size_t sz)
{

    //printf("sending data: \"%s\", size %zu (with header)\n", buffer, sz + sizeof(D1Header));

    /*assuming, for now, that sz is the size of *buffer */
    if (sz > (MAX_PACKET_SIZE - sizeof(D1Header))) // if the size of the incomming buffer is greater than the max packet size minus the header size
    {
        fprintf(stderr, "error: size of data for data-packet is too large."); // todo double check error output
        return SEND_PACKET_TOO_LARGE_ERROR;
    }
    D1Header *header = (D1Header *)calloc(1, sizeof(D1Header));
    if (header == NULL)
    {
        perror("calloc");
        return CALLOC_ERROR;
    }

    header->size = sz + sizeof(D1Header);
    header->size = htonl(header->size); // convert the size to network byte order

    header->flags = FLAG_DATA;
    header->flags |= peer->next_seqno << 7; // set the seqno flag to the next_seqno value in the peer struct
    header->flags = htons(header->flags);   // convert the flags to network byte order

    uint8_t *packet = (uint8_t *)calloc(1, sizeof(D1Header) + sz); /*allocate memory for the packet, should always be at most 1024*/

    if (packet == NULL)
    {
        perror("calloc");
        free(header);
        return CALLOC_ERROR;
    }
    memcpy(packet, header, sizeof(D1Header));
    memcpy(packet + sizeof(D1Header), buffer, sz);

    header->checksum = htons(compute_checksum(packet, sizeof(D1Header) + sz)); /*compute the checksum of the packet when all the relevant data has been filled*/
    memcpy(packet, header, sizeof(D1Header));                                  /*copy the header back into the packet, to override the 0-checksum*/

    int wc;
    // printf("sending to %s:%d\n", inet_ntoa(peer->addr.sin_addr), ntohs(peer->addr.sin_port));
    wc = sendto(
        peer->socket,
        packet,
        sizeof(D1Header) + sz,
        0,
        (struct sockaddr *)&peer->addr,
        sizeof(peer->addr));
    //printf("sent %d bytes\n", wc);
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
    D1Header *header = (D1Header *)calloc(1, sizeof(D1Header));
    if (header == NULL)
    {
        perror("calloc");
        return;
    }
    header->flags = FLAG_ACK; /*this is an ack packet*/

    header->flags |= (seqno & ACKNO); /*set the ack number to be the same as the seqno*/

    header->flags = htons(header->flags);   /*flags in network byte order*/
    header->size = htonl(sizeof(D1Header)); /*set the size of the packet to the size of the header*/

    header->checksum = htons(compute_checksum((uint8_t *)header, sizeof(D1Header))); /*calculate the checksum of the header, ! assuming checksum field is 0*/

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

    //printf("sent ACK for seqno: %d, ackno in header: %d.\n", seqno, ntohs(header->flags & ACKNO));

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

    for (size_t i = 0; i < sz; i++)
    {
        if (i % 2 == 0)
        {
            odd ^= packet[i];
        }
        else
        {
            even ^= packet[i];
        }
    }
    return (odd << 8) | even;
}