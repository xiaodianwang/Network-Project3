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
#include <sys/fcntl.h>
#include <math.h>
#include "common.h"

//Input Arguments to sender.c:
//argv[1] is Sender ID, which is either 1 (for Sender1) or 2 (for Sender2)
//argv[2] is the mean value inter-packet time R in millisec (based on Poisson distr). 
//argv[3] is the receiver ID, which is either 1 (Receiver1) or 2 (Receiver2)
//argv[4] is the router IP
//argv[5] is the size of the sliding window for Go-Back-N ARQ (default should be 32 packets)
//argv[6] is the initial timeout time (in milliseconds) for Go-Back-N
 // ARQ (it is only used once, after the first packet is received, the
 // timeout time is estimated using adaptive exponential averaging)
//argv[7] is the option for AIMD (additive increase, multiplicative decrease)
 //If Sender is using AIMD, argv[7] is 1. If not, argv[7] is 0.

int main(int argc, char *argv[]) {
    //Variables used for input arguments
    unsigned int sender_id; 
    unsigned int r; //inter-packet time W w/ mean R
    unsigned int receiver_id;
    char *dest_ip; //destination/router IP
    unsigned int slide_window_size;
    unsigned int timeout_time;
    unsigned int aimd_option;
    
    //Variables used for establishing the connection
    int sockfd, listen_sockfd;
    struct addrinfo hints, *receiver_info, *sender_info;
    int return_val, sender_return_val;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    
    //Variabes used for outgoing packets
    int packet_success;
    struct msg_payload *buffer;
    struct msg_payload payload;
    struct timeval start_time;
    struct timeval curr_time;
    time_t delta_time = 0, curr_timestamp_sec = 0, curr_timestamp_usec = 0;
    //Variables used for incoming packets
    int recv_success;
    struct msg_payload *buff;
    
    //Variables used for the sliding window Go-Back-N ARQ
    unsigned int next_seq_no = 0, beg_seq_no = 0, recv_no = 0;
    
    //Variables used for estimation of packet timeout value
    unsigned int ack_pkt_cnt = 0;
    float avg_rtt = 0, current_rtt = 0, avg_dev = 0; //units are in ms
    
    //Parsing input arguments
    if (argc != 8) {
        perror("Sender 2: incorrect number of command-line arguments\n");
        return 1; 
    } else {
        sender_id = atoi(argv[1]);
        r = atoi(argv[2]);
        receiver_id = atoi(argv[3]);
        dest_ip = argv[4];
        slide_window_size = atoi(argv[5]);
        timeout_time = atoi(argv[6]);
        aimd_option = atoi(argv[7]);
        printf("Sender id %d, r value %d, receiver id %d, router IP address %s, port number %s, sliding window size is %d, the timeout time is %d, AIMD option is %d\n", sender_id, r, receiver_id, dest_ip, ROUTER_PORT, slide_window_size, timeout_time, aimd_option);
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
        perror("Sender 2: unable to get target's address info\n");
        return 2;
    }
    
    //Take fields from first record in receiver_info, and create socket from it.
    if ((sockfd = socket(receiver_info->ai_family,receiver_info->ai_socktype,receiver_info->ai_protocol)) == -1) {
        perror("Sender 2: unable to create sending socket\n");
        return 3;
    }
    
    //Creating the listening socket
    if ((sender_return_val = getaddrinfo(NULL, SENDER_PORT, &hints, &sender_info)) != 0) {
        perror("Sender 2: unable to get address info for listening socket\n");
        return 4;
    }
    if((listen_sockfd = socket(sender_info->ai_family, sender_info->ai_socktype, sender_info->ai_protocol)) == -1) {
        perror("Sender 2: unable to create listening socket\n");
        return 5;
    }
    //set listening socket to be nonblocking
    fcntl(listen_sockfd, F_SETFL, O_NONBLOCK);
    
    if ((bind(listen_sockfd, sender_info->ai_addr, sender_info->ai_addrlen)) == -1) {
        close(listen_sockfd);
        perror("Sender 2: unable to bind listening socket to port\n");
        return 6;
    }
    printf("Sender 2: waiting to receive ACKS from D2...\n");
    
    //Establishing the packet: filling basic packet information
    memset(&payload, 0, sizeof payload);
    buffer = &payload;
    buffer->sender_id = htonl(sender_id); //Sender ID
    buffer->receiver_id = htonl(receiver_id); //Receiver ID

    addr_len = sizeof their_addr;
    //Memory set timeval structs for calculuating elapsed time
    memset(&curr_time, 0, sizeof (struct timeval));
    memset(&start_time, 0, sizeof (struct timeval));
    gettimeofday(&start_time, NULL);
    //allocate memory to buffer incoming ACK packets
    buff = malloc(sizeof (struct msg_payload));
    memset(buff, 0, sizeof (struct msg_payload));
    
    while (1) {
        //delta_time is elapsed time in milliseconds
        delta_time = ((curr_time.tv_sec * ONE_MILLION + curr_time.tv_usec) - (start_time.tv_sec * ONE_MILLION+ start_time.tv_usec))/1000;
        //printf("Elapsed (Delta) time for Sender 2: %f ms\n", (float)delta_time);
        
        if (next_seq_no < (beg_seq_no + slide_window_size)) {
            buffer->seq = htonl(next_seq_no); //pkt sequence ID, initialized at 0
            //Get the current packet timestamp
            gettimeofday(&curr_time, NULL);
            curr_timestamp_sec = curr_time.tv_sec;
            curr_timestamp_usec = curr_time.tv_usec;
            buffer->timestamp_sec = htonl(curr_timestamp_sec); //Pkt timestamp_sec
            buffer->timestamp_usec = htonl(curr_timestamp_usec); //Pkt timestamp_usec
            printf("SENT Pkt data: seq#-%d, senderID-%d, receiverID-%d, timestamp_sec-%d, timestamp_usec %d\n", ntohl(buffer->seq), ntohl(buffer->sender_id), ntohl(buffer->receiver_id), ntohl(buffer->timestamp_sec), ntohl(buffer->timestamp_usec));
            printf("Sender 2 current window size: %d\n", slide_window_size);
            //Send packet
            packet_success = sendto(sockfd, buffer, sizeof(struct msg_payload), 0, receiver_info->ai_addr, receiver_info->ai_addrlen);
            printf("Sender 2: Total packets sent so far: %d\n",next_seq_no+1);
            poisson_delay((double)r);
            //Update the packet sequence ID
            next_seq_no++;
        }
        //Receive ACK packets from listening socket
        recv_success = recvfrom(listen_sockfd, buff, sizeof (struct msg_payload), 0, (struct sockaddr *)&their_addr, &addr_len);
        if (recv_success > 0) {//received ACK packet
            ack_pkt_cnt++; //increment ACK packet counter
            buff->seq = ntohl(buff->seq);
            buff->sender_id = ntohl(buff->sender_id);
            buff->receiver_id = ntohl(buff->receiver_id);
            buff->timestamp_sec = ntohl(buff->timestamp_sec);
            buff->timestamp_usec = ntohl(buff->timestamp_usec);
            printf("RECEIVED Pkt data: seq#-%d, senderID-%d, receiverID-%d, timestamp_sec-%d, timestamp_usec:%d\n", buff->seq, buff->sender_id, buff->receiver_id, (int)buff->timestamp_sec, (int)buff->timestamp_usec);
            
            //Additively increase sliding window size of Sender
            if (aimd_option == 1) {
                slide_window_size++;
                printf("Window size updated to %d\n", slide_window_size);
            }
            //Estimate new timeout time using exponential averaging
            gettimeofday(&curr_time, NULL);
            current_rtt = ONE_MILLION * (curr_time.tv_sec - buff->timestamp_sec) + (curr_time.tv_usec - buff->timestamp_usec);
            
            printf("Current time: %d sec %d usec, timestamp: %d sec %d usec\n", (int)curr_time.tv_sec, (int)curr_time.tv_usec, buff->timestamp_sec, buff->timestamp_usec);
            if (ack_pkt_cnt == 1) {//1st ACK packet received
                avg_rtt = current_rtt;
                avg_dev = current_rtt;
                printf("RTT for 1st received ACK pkt: %f usec\n", avg_rtt);
            }
            if (ack_pkt_cnt > 1) {//All subsequent ACKs received
                gettimeofday(&curr_time, NULL);
                avg_rtt = avg_round_trip_time(avg_rtt, current_rtt, 0.875);
                avg_dev = avg_deviation(avg_dev, current_rtt, 0.75);
            }
            //New timeout_time
            timeout_time = timeout(avg_rtt, avg_dev);
            printf("The time out time is %d\n", timeout_time);
            /*Shift the window if Sender 2 receives an ACK for a
             packet sequence ID outside of the current window, or if
             the packet sequence ID is smaller than the current
             sequence number (next_seq_no) and S2 has timed out*/
            buff->seq = ntohl(buff->seq);
            if ((buff->seq < next_seq_no && delta_time >= timeout_time)|| (buff->seq > (beg_seq_no + slide_window_size))) {
                beg_seq_no = buff->seq;
                next_seq_no = buff->seq;
                //reset the timer
                gettimeofday(&curr_time, NULL);
                gettimeofday(&start_time, NULL);
            }
        } else {
            if (aimd_option == 1) {
                //ACKs are missing, divide sliding window size by 2
                slide_window_size = slide_window_size/2;
            }
        }
    }
    close(sockfd);
    close(listen_sockfd);
    return 0; 
}