// EE122 Project 2 - sender.c
// Xiaodian (Yinyin) Wang and Arnab Mukherji
//
// sender.c is the sender sending the packets to the router

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include "common.h"

//Input Arguments to sender.c:
//argv[1] is Sender ID, which is either 1 (for Sender1) or 2 (for Sender2)
//argv[2] is the mean value inter-packet time R in millisec (based on Poisson distr). 
//argv[3] is the receiver ID, which is either 1 (Receiver1) or 2 (Receiver2)
//argv[4] is the router IP
//argv[5] is the time duration in seconds (dictates how long sender will send pkts to target).

int main(int argc, char *argv[]) {
    //Variables used for input arguments
    unsigned int sender_id; 
    unsigned int r; //inter-packet time W w/ mean R
    unsigned int receiver_id;
    char *dest_ip; //destination/router IP
    unsigned int duration; //sending time duration in seconds
    
    //Variables used for establishing the connection
    int sockfd;
    struct addrinfo hints, *receiver_info;
    int return_val;
    
    //Variabes used for outgoing packets
    int packet_success;
    unsigned int seq = 0;
    struct msg_payload *buffer;
    struct msg_payload payload;
    struct timeval start_time;
    struct timeval curr_time;
    time_t delta_time = 0, curr_timestamp_sec = 0, curr_timestamp_usec = 0;
    
    //Parsing input arguments
    if (argc != 6) {
        perror("Sender: incorrect number of command-line arguments\n");
        return 1; 
    } else {
        sender_id = atoi(argv[1]);
        r = atoi(argv[2]);
        receiver_id = atoi(argv[3]);
        dest_ip = argv[4];
        duration = atoi(argv[5]);
        printf("Sender id %d, r value %d, receiver id %1d, router IP address %s, port number %s, time duration is %d\n", sender_id, r, receiver_id, dest_ip, ROUTER_PORT, duration);
    }
    
    //load struct addrinfo with host information
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    /*
     * for DEBUGGING purposes: change ROUTER_PORT TO get_receiver_port(1 or 2)
     * to have the sender directly send packets to the receiver.
     */
    //Get target's address information
    if ((return_val = getaddrinfo(dest_ip, ROUTER_PORT, &hints,
                          &receiver_info)) != 0) {
        perror("Sender: unable to get target's address info\n");
        return 2;
    }
    
    //Take fields from first record in receiver_info, and create socket from it.
    if ((sockfd = socket(receiver_info->ai_family,receiver_info->ai_socktype,receiver_info->ai_protocol)) == -1) {
        perror("Sender: unable to create socket\n");
        return 3;
    }
    
    //Establishing the packet: filling packet information
    gettimeofday(&start_time, NULL);
    gettimeofday(&curr_time, NULL);
    curr_timestamp_sec = curr_time.tv_sec;
    curr_timestamp_usec = curr_time.tv_usec; 
    //curr_timestamp = abs(curr_time.tv_sec * ONE_MILLION) + abs(curr_time.tv_usec);
    //printf("Current time: %f seconds %f microseconds\n", (double)curr_time.tv_sec, (double)curr_time.tv_usec);
    //printf("Current timestamp is %d sec, %d usec\n", (int)curr_timestamp_sec, (int)curr_timestamp_usec);
    
    memset(&payload, 0, sizeof payload);
    buffer = &payload;
    buffer->seq = htonl(seq++); //packet sequence ID
    buffer->sender_id = htonl(sender_id); //Sender ID
    buffer->receiver_id = htonl(receiver_id); //Receiver ID
    buffer->timestamp_sec = htonl(curr_timestamp_sec); //Pkt timestamp_sec
    buffer->timestamp_usec = htonl(curr_timestamp_usec); //Pkt timestamp_usec
    
    
    while ((delta_time / ONE_MILLION) < duration) {
        //printf("%s: payload size is %f Bytes\n", __func__, (double)sizeof(payload));
        printf("Pkt data: seq#-%d, senderID-%d, receiverID-%d, timestamp_sec-%d, timestamp_usec %d\n", ntohl(buffer->seq), ntohl(buffer->sender_id), ntohl(buffer->receiver_id), ntohl(buffer->timestamp_sec), ntohl(buffer->timestamp_usec));
        packet_success = sendto(sockfd, buffer, sizeof(struct msg_payload), 0, receiver_info->ai_addr, receiver_info->ai_addrlen);
        printf("Sender: Total packets sent so far: %d\n", seq);
        poisson_delay((double)r);
        gettimeofday(&curr_time, NULL);
        //delta_time is elapsed time in microseconds
        //   (divide by ONE_MILLION to get seconds)
        delta_time = (curr_time.tv_sec * ONE_MILLION + curr_time.tv_usec) - (start_time.tv_sec * ONE_MILLION + start_time.tv_usec);
        
        //Get the timestamp for the next packet
        curr_timestamp_sec = curr_time.tv_sec;
        curr_timestamp_usec = curr_time.tv_usec;
        buffer->timestamp_sec = htonl((long)curr_timestamp_sec);
        buffer->timestamp_usec = htonl((long)curr_timestamp_usec);
        buffer->seq = htonl(seq++);
        //printf("Sender: Delta time: %d usec, current time of seconds: %d sec, current time of microsec: %d microsec\n", (int)delta_time, (int)curr_timestamp_sec, (int)curr_timestamp_usec);
    }
    close(sockfd);
    return 0; 
}