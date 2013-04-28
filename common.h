// EE122 Project 2 - common.h
// Xiaodian (Yinyin) Wang and Arnab Mukherji
//  
// common.h stores all of the data structures and definitions used by the sender,
// router and receiver. 

#ifndef _common_h
#define _common_h
#define ROUTER_PORT "6000"
#define ONE_MILLION 1000000
#define RECEIVER_PORT_BASE 5000

//UDP datagram payload format, 128 bytes total
struct msg_payload {
    unsigned int seq; //packet Sequence ID, 4 bytes
    unsigned int timestamp_sec; //seconds portion of timestamp, 4 bytes
    unsigned int timestamp_usec; //microsec portion of timestamp, 4 bytes
    unsigned int sender_id; //4 bytes
    unsigned int receiver_id; //4 bytes
    unsigned char msg[108];
} __pack__; //pack so that the CPU does not assign spacing between fields

//The router queue is a linked list data structure (router_q) with q_elem nodes
struct q_elem { //q_elem is a linked list node
    struct msg_payload *buffer; //this points to the actual received payload buffer
    struct q_elem *next; //points to the next q elemenet in the linked list
};

struct router_q { 
    struct q_elem *head; //points to the q header
    struct q_elem *tail; // points to the q tail
    unsigned int q_size; //number of q elements in the router_q linked-list
    unsigned int drop_cnt; //how many received payloads that the buffer dropped
};

extern void *get_in_addr(struct sockaddr *sa); 

extern int enqueue (struct q_elem *elem, struct router_q *q, unsigned int max_q_size);

extern struct q_elem *dequeue (struct router_q *q);

extern void poisson_delay (double mean);

extern char *get_receiver_port(unsigned int receiver_id);
#endif