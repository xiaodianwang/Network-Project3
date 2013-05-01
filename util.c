// EE122 Project 2 - util.c
// Xiaodian (Yinyin) Wang and Arnab Mukherji
//
// util.c contains all of the common functions that are used by the senders, router and receivers.

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

//Get the socket address, IPv6 or IPv6 (taken from Beej's guide)
/*If the sa_family field is AF_INET (IPv4), return the IPv4 address. Otherwise return the IPv6 address.*/
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Enqueue packets to linked list
int enqueue (struct q_elem *elem, struct router_q *q, unsigned int max_q_size) {
    if (q->q_size >= max_q_size) {//If queue size is at max, drop incoming packets
        q->drop_cnt++;
        return 1; 
    }
    
    if (q->head == NULL) {
        q->head = elem;
    } else {
        q->tail->next = elem;
    }
    q->tail = elem; 
    q->q_size++;
    elem->next = NULL;
    //printf("%s %d Queue size is %d\n", __func__, __LINE__, q->q_size);
    return 0; 
}

//Dequeue packets from linked list
struct q_elem *dequeue (struct router_q *q) {
    struct q_elem *elem = NULL;
    //printf("%s %d Queue size is %d\n", __func__, __LINE__, q->q_size);
    if (q->head == NULL) {
        return NULL;
    } else {
        elem = q->head;
        q->head = q->head->next;
        q->q_size--;
    }
    if (q->head == NULL) {
        q->tail = NULL;
        q->q_size = 0;
    }
    return elem; 
}

//Packet delay time, generates a time delay according to a poisson distribution
/*
 Because the rand() function isn't really random even when you seed random() with
  the current time, it generates exponential numbers at poisson loop times. Not as
  precise but has better "randomness" than without the looping (Otherwise you get
  consistently very similar exponential values).
 */
void poisson_delay(double mean) {
    double l = 0, p = 1; 
    double delay_time = 0, rand_num = 0;
    struct timeval curr_time;
    
    l = exp(-1*mean);
    gettimeofday(&curr_time, NULL);
    srand(curr_time.tv_usec); //seed random number generator with current time
    
    do {
        rand_num = rand()/(double)RAND_MAX;
        //printf("%s: The random number is %f\n", __func__, rand_num);
        
        p = p * rand_num;
    } while (p > l);
    
    delay_time = -log(1.0 - rand_num)*mean;
    //printf("*****%s delay is: %f milliseconds\n",__func__, delay_time);
    usleep((useconds_t) (delay_time * 1000)); //usleep is in usec, want millisec
}

//Generates a time delay according to a uniform distribution.
//Given an input b, will delay a random time within the uniform
//distribution of [0, b].
void uniform_delay(int b) {
    int delay_time = 0, rand_num = 0;
    struct timeval curr_time;
    
    gettimeofday(&curr_time, NULL);
    srand(curr_time.tv_usec); //seed randnum generator w/ current time
    rand_num = rand()%(b+1);
    delay_time = rand_num * 1000; //get delay time in millisec
    printf("%s in util.c: uniform delay time is %d ms\n", __func__, delay_time/1000);
    usleep((useconds_t) delay_time); 
}

//Function to obtain the exponential avg of packet round-trip-times (in ms)
//Used in estimating the sender window timeout time
//Based on the equation A(n+1) = (1-b)*A(n) + b*T(n+1), where:
//A(n) is the exponential average RTT, b is a pre-chosen parameter value (typically 0.875), T(n) is the RTT of the current packet
float exp_avg = 0;
float avg_round_trip_time(float avg_so_far, float curr_rtt, float b) {
    exp_avg = (1-b)*avg_so_far + b*curr_rtt;
    printf("%s in util.c: exponential average RTT is %f ms\n", __func__, exp_avg);
    return exp_avg;
}

//Function to obtain the exponential avg of round-trip-time deviation (in ms)
//Used in estimating the sender window timeout time
//Based on the equation D(n+1) = (1-b)*D(n) + b*|T(n+1) - A(n+1)|, where:
//D(n) is the exponential average deviation, b is a pre-chosen parameter value (typically 0.75), T(n) is the RTT of the current packet
float exp_dev = 0;
float avg_deviation(float dev_so_far, float curr_rtt, float b) {
    exp_dev = (1-b)*dev_so_far + b*abs(curr_rtt - exp_avg);
    return exp_dev;
    printf("%s in util.c: exponential deviation is %f ms\n", __func__, exp_dev);
}

//Function to obtain the timeout time for the Sender's sliding window (in ms)
float timeout_t =0;
float timeout(float exp_avg_rtt, float deviation) {
    timeout_t = exp_avg_rtt + (4 * deviation);
    printf("%s in util.c: timeout time is %f ms\n", __func__, timeout_t);
    return timeout_t;
}
//Function to retreive the port for a given receiver ID
char port_str[32];
char *get_receiver_port(unsigned int receiver_id) {
    unsigned int port;
    port = (RECEIVER_PORT_BASE + (receiver_id - 1));
    sprintf(port_str, "%d", port);
    return port_str; 
}

/*Function to obtain a running average, given a count of total elements and
 the cumulative sum of the elements themselves. Used by the router to 
 obtain the average length of each queue. Also used by the receiver to 
 obtain the  average packet delay from the sender to the receiver.*/
unsigned int avg;
unsigned int running_avg(unsigned int count, unsigned int cumulative) {
    avg = cumulative / count;
    printf("%s in util.c: is currently %d\n",__func__, avg);
    return avg; 
}