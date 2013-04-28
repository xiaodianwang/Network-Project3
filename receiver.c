// EE122 Project 2 - receiver.c
// Xiaodian (Yinyin) Wang and Arnab Mukherji
//  

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
#include <sys/fcntl.h>
#include <math.h>
#include "common.h"

//Input Arguments:
//agv[1] is the receiver ID

int main(int argc, char *argv[]) {
    //Variables used for input argument
    unsigned int receiver_id;
    
    //Variables used in establishing socket and connection
    struct addrinfo hints, *dest_info;
    int return_val, sockfd;
    
    //Variables used for receiving incoming packets
    struct msg_payload *buff;
    int recv_success, rcvd_pkt_cnt = 0;
    struct sockaddr_storage their_addr;
    socklen_t addr_len; 
    
    //Variables used in calculating delay time
    struct timeval receival_time; 
    time_t delta_time = 0;
    unsigned int avg_pkt_delay = 0;
    
    //Parsing input argument
    if (argc != 2) {
        perror("Receiver: incorrect number of input arguments\n");
        return 1;
    } else {
        receiver_id = atoi(argv[1]);
    }
    
    //Load struct addrinfo with host information
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    
    //Get address information
    if ((return_val = getaddrinfo(NULL, get_receiver_port(receiver_id), &hints, &dest_info)) != 0) {
        perror("Receiver: unable to get address info\n");
        return 2;
    }
    //Take fields from first record in dest_info, create socket and bind to port
    if ((sockfd = socket(dest_info->ai_family, dest_info->ai_socktype, dest_info->ai_protocol)) == -1) {
        printf("Receiver %d: unable to create socket\n", receiver_id);
        return 3;
    }
    if((bind(sockfd, dest_info->ai_addr, dest_info->ai_addrlen)) == -1) {
        close(sockfd);
        printf("Receiver %d: unable to bind socket to port\n", receiver_id);
        return 4;
    }
    printf("Receiver %d: waiting to recvfrom...\n", receiver_id);
    
    //Memory allocation for buffering the incoming packets
    buff = malloc(sizeof (struct msg_payload));
    memset(buff, 0, sizeof (struct msg_payload));

    addr_len = sizeof their_addr;
    memset(&receival_time, 0, sizeof (struct timeval));
    while (1) { 
        recv_success = recvfrom(sockfd, buff, sizeof (struct msg_payload), 0, (struct sockaddr *)&their_addr, &addr_len);
        gettimeofday(&receival_time, NULL);
        if (recv_success > 0) { //destination received a packet
            rcvd_pkt_cnt++;
            printf("Total packets recvfrom by receiver %d so far: %d\n", receiver_id, rcvd_pkt_cnt);
            buff->seq = ntohl(buff->seq);
            buff->sender_id = ntohl(buff->sender_id);
            buff->receiver_id = ntohl(buff->receiver_id);
            buff->timestamp_sec = ntohl(buff->timestamp_sec);
            buff->timestamp_usec = ntohl(buff->timestamp_usec);
            printf("Pkt data: seq#-%d, senderID-%d, receiverID-%d, timestamp_sec-%d, timestamp_usec:%d\n", buff->seq, buff->sender_id, buff->receiver_id, (int)buff->timestamp_sec, (int)buff->timestamp_usec);
            
            //Calculating the avg packet propagation/delay time in microsec
            //printf("Time of packet receival: %d sec, %d microsec\n", (int)receival_time.tv_sec, (int)receival_time.tv_usec);
            delta_time = abs((receival_time.tv_usec - buff->timestamp_usec) + (receival_time.tv_sec - buff->timestamp_sec) * ONE_MILLION);
            
            avg_pkt_delay = running_avg(rcvd_pkt_cnt, (unsigned int)delta_time);
            //printf("Delay time for this packet: %d microsec | Average packet delay:%d microsec\n", (int)delta_time, avg_pkt_delay);
        }
    }
    close(sockfd);
}