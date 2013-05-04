// EE122 Project 2 - router.c
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

#define FLAG_ON 1
#define FLAG_OFF 0

//Input Arguments to router.c:
//argv[1] is the number of queues
//argv[2] is the router dequeuing interval, or service rate in milliseconds/pkt,
//  so one packet will be dequeued and sent per dequeuing interval
//argv[3] is the maximum queue size (in packets). If there are 2 queues, this argument
//  means that the length of EACH queue = maximum queue size. 

int main(int argc, char *argv[]) {
    //Variables used for input arguments
    unsigned int q_amount;
    unsigned int dq_time; // router service rate
    unsigned int max_q_size;
    
    //Variables used for establishing connection
    int listen_sockfd, d1_sockfd, d2_sockfd;
    struct addrinfo hints, *router_info, *dest1_info, *dest2_info;
    int return_val, r1_return_val, r2_return_val;
    struct sockaddr_storage their_addr;
    socklen_t addr_len; 
    
    //Variables used for incoming/outgoing packets
    int packet_success, sent_success;
    struct msg_payload *buff, *received_pkt;
    int router_packet_count = 0, enq_return = 0, q_index = 0;
    struct q_elem *node, *dqd_pkt = NULL;
    struct router_q *q1, *q2;
    int packets_sent = 0, sent_d1 = 0, sent_d2 = 0;
    unsigned int host_recv_id = 0;
    
    //Variables used for managing router service rate
    struct timeval last_time;
    struct timeval curr_time;
    time_t delta_time = 0; 
    unsigned int sent_flag = 0;
    
    //Variables used for obtaining average queue lengths
    //q_dq_cnt: total number of dequeue operations performed so far
    //cum_q_size: sum of all queue lengths for every dequeue operation so far
    unsigned int q_dq_cnt = 0, q1_dq_cnt = 0, q2_dq_cnt = 0;
    unsigned int cum_q_size = 0, cum_q1_size = 0, cum_q2_size = 0;
    unsigned int avg_q_size = 0, avg_q1_size = 0, avg_q2_size = 0;
        
    //Parsing input arguments
    if (argc!= 4) {
        perror("Router: incorrect number of command-line arguments\n");
        return 1;
    } else {
        q_amount = atoi(argv[1]);
        dq_time = atoi(argv[2]);
        max_q_size = atoi(argv[3]);
    }
    
    //load struct addrinfo with router information
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; 
    
    //Get address information
    if ((return_val = getaddrinfo(NULL, ROUTER_PORT, &hints, &router_info)) != 0) {
        perror("Router: unable to get address info\n");
        return 1;
    }
    
    //Take fields from first record in router_info, and create socket from it
    if ((listen_sockfd = socket(router_info->ai_family, router_info->ai_socktype, router_info->ai_protocol)) == -1) {
        perror("Router: unable to create listening socket\n");
        return 2;
    }
    //set listening socket to be nonblocking
    fcntl(listen_sockfd, F_SETFL, O_NONBLOCK);
    
    if((bind(listen_sockfd, router_info->ai_addr, router_info->ai_addrlen)) == -1) {
        close(listen_sockfd);
        perror("Router: unable to bind socket to port\n");
        return 3;
    }
    printf("Router: waiting to recvfrom...\n");
    
    //Create a datagram socket for Receiver 1
    //Receiver 1 is on same computer as router, use localhost information
    if ((r1_return_val = getaddrinfo("127.0.0.1", get_receiver_port(1), &hints, &dest1_info)) != 0) {
        perror("Router: unable to get address info for Destination 1\n");
        return 4;
    }
    
    if ((d1_sockfd = socket(dest1_info->ai_family, dest1_info->ai_socktype, dest1_info->ai_protocol)) == -1) {
        close(d1_sockfd);
        perror("Router: unable to create socket for receiver 1\n");
    }
    
    //Create datagram socket for Receiver 2
    //Receiver 2 is on same computer as router, use localhost information
    if((r2_return_val = getaddrinfo("127.0.0.1", get_receiver_port(2), &hints, &dest2_info)) != 0) {
        perror("Router: unable to get address info for Destination 2\n");
        return 5;
    }
    
    if ((d2_sockfd = socket(dest2_info->ai_family, dest2_info->ai_socktype, dest2_info->ai_protocol)) == -1) {
        close(d2_sockfd);
        perror("Router: unable to create socket for receiver 2\n");
    }
    
    addr_len = sizeof their_addr;
    last_time.tv_sec = 0;
    last_time.tv_usec = 0;
    
    //Memory allocation of the buffer for the incoming packets, queues, & packet to be queued
    buff = malloc(sizeof (struct msg_payload));
    q1 = malloc(sizeof (struct router_q));
    q2 = malloc(sizeof (struct router_q));
    node = malloc(sizeof (struct q_elem));
    
    memset(buff, 0, sizeof (struct msg_payload));
    memset(q1, 0, sizeof (struct router_q));
    memset(q2, 0, sizeof (struct router_q));
    memset(node, 0, sizeof (struct q_elem));
    memset(&last_time, 0, sizeof last_time);
    memset(&curr_time, 0, sizeof curr_time);
    gettimeofday(&last_time, NULL); 
     
    while (1) {
        gettimeofday(&curr_time, NULL);
        //delta_time is the time elapsed in milliseconds
        delta_time =abs(((curr_time.tv_usec + curr_time.tv_sec * ONE_MILLION) - (last_time.tv_usec +last_time.tv_sec * ONE_MILLION))/1000);
        
        packet_success = recvfrom(listen_sockfd, buff, sizeof (struct msg_payload), 0, (struct sockaddr *)&their_addr, &addr_len);
        if (packet_success > 0) {//router has received a packet
            router_packet_count++;
            //printf("Total packets recvfrom by router so far: %d\n", router_packet_count);
            //printf("Delta time is %d\n", (int)delta_time);
            received_pkt = buff;
            //received packet becomes buffer within the linked-list node 
            node->buffer = received_pkt; 
            if (q_amount == 1) {
                //enqueue node into linked-list
                enq_return = enqueue(node, q1, max_q_size);
            }
            if (q_amount > 1) {
                host_recv_id = ntohl(node->buffer->receiver_id);
                if ((int)host_recv_id == 1) {
                   enq_return = enqueue(node, q1, max_q_size);
                }
                if ((int)host_recv_id == 2) {
                    enq_return = enqueue(node, q2, max_q_size);
                }
            }
            if (enq_return == 0) {
                buff = malloc(sizeof (struct msg_payload));
                node = malloc(sizeof (struct q_elem));
                memset(buff, 0, sizeof buff);
                memset(node, 0, sizeof node); 
            }
        }
        
        if ((delta_time >= dq_time) && sent_flag == FLAG_OFF) {
            //printf("Delta time: %d, >= dequeue rate for router\n", (int)delta_time);
            if (q_amount == 1) {
                dqd_pkt = dequeue(q1);
                //printf("Packet Sequence number %d\n", dqd_pkt->buffer->seq);
                //Obtain the average queue length
                if (q1->q_size != 0) {
                    q_dq_cnt++;
                    cum_q_size += q1->q_size;
                    avg_q_size = running_avg(q_dq_cnt, cum_q_size);
                    printf("SINGLE QUEUE - Cumulative sum of queue lengths: %d | # of dequeue operations: %d | Average router queue size: %d\n", cum_q_size, q_dq_cnt, avg_q_size);
                }
            }
            if (q_amount == 2) {
                //The flow (sender1, destination1) is prioritized,
                //so dequeueing q1 is prioritized. Only dequeued from q2 if q1 is empty.
                if (q1->q_size > 0) {
                    dqd_pkt = dequeue(q1);
                    printf("Dequeued from q1, q1 size is %d\n", q1->q_size);
                    //Obtain the average queue 1 length
                    if (q1->q_size != 0) {
                        cum_q1_size += q1->q_size;
                        q1_dq_cnt++;
                        avg_q1_size = running_avg(q1_dq_cnt, cum_q1_size);
                        printf("QUEUE 1 - Cum. sum of queue lengths: %d | # of dequeue operations: %d | Avg router Q1 size: %d | Avg router Q2 size: %d\n", cum_q1_size, q1_dq_cnt, avg_q1_size, avg_q2_size);
                    }
                } else {
                    dqd_pkt = dequeue(q2);
                    //printf("Dequeued from q2, q2 size is %d\n", q2->q_size);
                    //Obtain the average queue 2 length
                    if (q2->q_size != 0) {
                        cum_q2_size += q2->q_size;
                        q2_dq_cnt++; 
                        avg_q2_size = running_avg(q2_dq_cnt, cum_q2_size);
                        printf("QUEUE 2 - Cum. sum of queue lengths: %d | # of dequeue operations: %d | Avg router Q1 size: %d | Avg router Q2 size: %d\n", cum_q2_size, q2_dq_cnt, avg_q1_size, avg_q2_size);
                    }
                }
            }
            if (dqd_pkt != NULL) {
                host_recv_id = ntohl(dqd_pkt->buffer->receiver_id);
                if ((int)host_recv_id == 1) {
                    sent_success = sendto(d1_sockfd, dqd_pkt->buffer, sizeof (struct msg_payload), 0, dest1_info->ai_addr, dest1_info->ai_addrlen);
                    sent_d1++;
                    //printf("Pkts sent to dest_1 so far: %d\n", sent_d1);
                    printf("Drop count %d\n", q1->drop_cnt); 
                }
                if ((int)host_recv_id == 2) {
                   sent_success = sendto(d2_sockfd, dqd_pkt->buffer, sizeof (struct msg_payload), 0, dest2_info->ai_addr, dest2_info->ai_addrlen);
                    sent_d2++;
                    //printf("Pkts sent to dest_2 so far: %d\n", sent_d2);
                }
                packets_sent++;
                //printf("Overall total pkts sent by router so far: %d\n", packets_sent);
                free(dqd_pkt->buffer);
                free(dqd_pkt);
            }
            gettimeofday(&last_time, NULL); 
            sent_flag = FLAG_ON;
        }
        if (delta_time < dq_time) {
            sent_flag = FLAG_OFF;
        }
    }
    close(listen_sockfd);
    close(d1_sockfd);
    close(d2_sockfd);
}