/*
 * A simple UDP relay program. The purpose of the program is to relay the UDP
 * message that it gets on one port to multiple IP addresses.
 *
 * I wrote this program to solve something at work. Thought someone might find
 * this useful.
 *
 * gcc *.c -lpthread (Make sure you have pthread_barrier_t support)
 *
 * ./a.out -p 2344 -i 192.168.1.101 -i 192.168.2.102
 *
 * In the above example, the program listens for incoming messages on port 2344
 * and relays the payload to the IP addresses 192.168.1.101 and 192.168.2.102 on
 * port 2344.
 *
 * NOTE: If the IPs are in different subnets ensure the routing is all setup for
 * the relay to work properly and please feel free to make this program more
 * interesting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <semaphore.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cb.h"

#define BUFF_SIZE 570

/* Circular buffer shared by listen and relay threads */
static cb cbuff;

/* Holds information about where to relay the payload to */
struct relay_info {
	uint16_t num_addresses;
	uint16_t port;
	struct in_addr addr;
};

/* Holds the payload to be relayed */
struct payload {
	uint16_t size;
	char data[BUFF_SIZE];
};

/* Keeps track of the number of threads entering/leaving the ciritical region */
int enter_count, leave_count;

/* Mutex to serialise relay thread's entry into the critical region */
pthread_mutex_t enter_mutex, leave_mutex;

/* Barriers that relay threads are subjected to */
pthread_barrier_t enter_barrier, leave_barrier;

/* Rendevouz semaphore for communication between listener and relay threads */
sem_t full_buff, empty_buff;

/* Function that handles the CLI arguments supplied by the user */
void process_args(char**);

/* Function that prints the usage of this program */
void print_usage(char**);

/* A pre-requisite function that relay threads should go through before relaying */
void enter_critical_region(uint16_t);

/* Function that all relay threads should go through after relaying */
void leave_critical_region(uint16_t);

/* Function that performs house keeping on global variables */
void cleanup_globals();

/* Function that relay threads begin executing from when created */
void* relay_thread_begin(void*);

/* Function that listens for incoming UDP packets and signals the realy thread */
int udp_listener(int, cb*);

/* Helper function to check for valid IP addresses */
int is_valid_ip(char*);

/* Helper function that relays the payload */
int relay_payload(struct in_addr, int, struct payload *);

int main(int argc, char* argv[])
{
	int i_flag, p_flag, lp_port, i_index, opt;
	/* Points to the first in_addr type given by the user in -i */
	char *ip_addr;
	i_index = p_flag = i_index = opt = 0;
	ip_addr = NULL;

	/* Set the available buffer semaphore to be 50 initially*/
	sem_init(&empty_buff, 0, 50);
	sem_init(&full_buff, 0, 0);

	/* Circular buffer can hold 50 blobs of length BUFF_SIZE bytes each */
	cb_init(&cbuff, sizeof(struct payload), 50);

	while ((opt = getopt(argc, argv, "i:p:")) != -1) {
		switch (opt) {
		case 'p':
			if (p_flag)
				break;

			lp_port = atoi(optarg);
			p_flag = 1;
			break;
		case 'i':
			if (is_valid_ip(optarg) <= 0) {
				free(ip_addr);
				cb_destroy(&cbuff);
				print_usage(argv);
			}

			i_flag = 1;

			if (i_index == 0) {
				ip_addr = malloc(sizeof(struct in_addr));
				assert(ip_addr != NULL);
				inet_aton(optarg, (struct in_addr *) ip_addr);
			} else {
				/* We are given multiple IP addresses */
				char* next_addr;
				next_addr = NULL;
				/* Increase the size of ip_addr */
				ip_addr = realloc(ip_addr, \
						  sizeof(struct in_addr) * i_index);
				assert(ip_addr != NULL);
				/* Get to the start of the next in_addr */
				next_addr = ip_addr + \
					    sizeof(struct in_addr) * i_index;
				inet_aton(optarg, (struct in_addr *) next_addr);
			}
			i_index++;
			break;
		}
	}

	/* No -i or -p flag was passed in */
	if (!(i_flag && p_flag)) {
		cb_destroy(&cbuff);
		print_usage(argv);
	}

	/* Port number is invalid */
	if (lp_port <= 0 || lp_port >= 65535) {
		cb_destroy(&cbuff);
		print_usage(argv);
	}

	int i,ret;
	pthread_t *pt;
	struct relay_info *rptr;

	/* Space for relay threads */
	pt = malloc(sizeof(pthread_t) * i_index);
	assert(pt != NULL);

	/* Loop through each IP and create a relay thread */
	for (i=0; i < i_index; i++) {
		rptr = (struct relay_info *) malloc(sizeof(struct relay_info));
		assert(rptr != NULL);
		rptr->num_addresses = i_index;
		rptr->port = lp_port;
		rptr->addr = *(struct in_addr *)(ip_addr + sizeof(struct in_addr)*i);
		ret = pthread_create(&pt[i], NULL, relay_thread_begin, rptr);
	}

	/* Run the listener function */
	udp_listener(lp_port, &cbuff);

	/* Cleanups */
	free(ip_addr);
	free(rptr);
	free(pt);
	cb_destroy(&cbuff);
}

void print_usage(char *argv[])
{
	printf("USAGE: %s -p PORT_NUM -i IP\n", argv[0]);
	printf("\tPORT_NUM is the port to listen for incoming UDP msg\n");
	printf("\tIP (or -i IP1 -i IP2 etc.) is the address to relay the msg to\n");
	exit(0);
}

int is_valid_ip(char* ip_string)
{
	int result;
	struct sockaddr_in sa;
	result = inet_pton(AF_INET, ip_string, &(sa.sin_addr));
	return result != 0;
}

void* relay_thread_begin(void *ri)
{
	char *r_ip;
	uint16_t r_port, num_addresses;
	int processed = 0;
	struct in_addr addr;
	enter_count = 0;
	cb* c_buff = &cbuff;
	num_addresses = (*(struct relay_info *) ri).num_addresses;
	r_ip = inet_ntoa((*(struct relay_info *) ri).addr);

	addr = (*(struct relay_info *) ri).addr;
	r_port = (*(struct relay_info *) ri).port;

	printf("Relay to IP %s on Port %d from %u\n", r_ip, r_port, pthread_self());
	pthread_barrier_init(&enter_barrier, NULL, num_addresses);
	pthread_barrier_init(&leave_barrier, NULL, num_addresses);

	for (;;) {
		sem_wait(&full_buff);
		pthread_mutex_lock(&enter_mutex);
		enter_critical_region(num_addresses);
		relay_payload(addr, r_port, (struct payload *) c_buff->r_ptr);
		leave_critical_region(num_addresses);
		sem_post(&empty_buff);
	}
}

void enter_critical_region(uint16_t num_threads)
{
 	/* Let waiting relay threads enter the critical region */
	if (enter_count < num_threads) {
		enter_count = enter_count + 1;
		pthread_mutex_unlock(&enter_mutex);
		if (enter_count != num_threads)
			sem_post(&full_buff);
	}

	/* Only return when all relay threads are confirmed to have entered */
	pthread_barrier_wait(&enter_barrier);
	enter_count = 0;
}

void leave_critical_region(uint16_t num_threads)
{
	/* Leave en masse and the last guy out update the r_ptr on the CB */
	pthread_mutex_lock(&leave_mutex);
	leave_count++;
	if (leave_count == num_threads)
		cb_move_rptr(&cbuff);
	pthread_mutex_unlock(&leave_mutex);

	pthread_barrier_wait(&leave_barrier);
	leave_count = 0;
}

int udp_listener(int port, cb* cbuff)
{
	int recv_sock, b;
	char recv_buff[BUFF_SIZE];
	struct sockaddr_in recv_sockaddr;
	socklen_t recv_addrlen;
	ssize_t received;

	recv_sock = socket(AF_INET, SOCK_DGRAM, 0);

	/* Cannot create a UDP socket */
	if (recv_sock < 0) {
		printf("Cannot create Socket to receive UDP messages\n");
		return -1;
	}

	/* Assigning a name to the socket */
	memset(&recv_sockaddr, 0, sizeof(recv_sockaddr));
	recv_addrlen                  = sizeof(recv_sockaddr);
	recv_sockaddr.sin_family      = AF_INET;
	recv_sockaddr.sin_port        = htons(port);
	recv_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	b = bind(recv_sock, (struct sockaddr*) &recv_sockaddr, sizeof(recv_sockaddr));
	if (b < 0) {
		printf("Cannot bind socket to an IP\n");
		return -1;
	}	

	printf("Listening on %s:%d\n", inet_ntoa(recv_sockaddr.sin_addr),
		(int) ntohs(recv_sockaddr.sin_port));

	/* Listen for incoming packets */
	struct payload p;
	for(;;) {
		received = recvfrom(recv_sock, recv_buff, BUFF_SIZE, 0,
				    (struct sockaddr*) &recv_sockaddr,
				    &recv_addrlen);

		if (received == 0) {
			continue;
		}

		p.size = received;
		memcpy(p.data, recv_buff, p.size);
		sem_wait(&empty_buff);
		cb_write_elem(cbuff, &p);
		sem_post(&full_buff);

		printf("Received payload size - %d Bytes\n", received);
		printf("Stored payload in %p\n", cbuff->w_ptr);
		memset(recv_buff, 0, sizeof(recv_buff));
		memset(&p, 0, sizeof(struct payload));
	}

}

int
relay_payload(struct in_addr addr, int port, struct payload* pl)
{
	/* Relay the payload to a given IP address */
	int relay_sock;
	struct sockaddr_in relay_sockaddr;
	socklen_t relay_addrlen;
	ssize_t sent;
	int i;

	memset((char *) &relay_sockaddr, 0, sizeof(relay_sockaddr));
	memset((char *) &sent, 0, sizeof(sent));

	relay_addrlen = sizeof(relay_sockaddr);
	relay_sockaddr.sin_family = AF_INET;
	relay_sockaddr.sin_port = htons(port);
	relay_sockaddr.sin_addr = addr;

	relay_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (relay_sock < 0) {
		printf("\tCannot create relay socket\n");
		return -1;
	}

	sent = sendto(relay_sock, pl->data, pl->size, 0, \
		      (struct sockaddr*) &relay_sockaddr, relay_addrlen);

	if (sent == -1)
		printf("\tCannot relay payload to %s \n", inet_ntoa(addr));
	else
		printf("\tRelayed payload at %p to %s:%d - %d Bytes\n",
			pl, inet_ntoa(addr) ,port, sent);
	close(relay_sock);

}
